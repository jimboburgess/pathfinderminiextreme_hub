#include "transport.h"
#include "server_limits.h"
#include "service.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <csignal>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <filesystem>
#ifdef __linux__
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace pfme::server
{
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;
using StreamProtocol=asio::generic::stream_protocol;
using StreamSocket=StreamProtocol::socket;
using Error = boost::system::error_code;
using namespace std::chrono_literals;
#ifdef PFME_HAS_PLAYABLE
#include "web_assets.inc"
#endif

class Server;
class Peer : public std::enable_shared_from_this<Peer>
{
public:
    Peer(StreamSocket socket, Server& owner, uint64_t serial, uint16_t port);
    bool checkAdmission();
    bool countsPrincipal(const std::string& principal) const {return principalCounted_&&!finished_&&connection_.verifiedPrincipal==principal;}
    void start();
    void stop();
    void finish();
private:
    void arm(unsigned seconds);
    void read();
    void send(std::string text, bool close = false);
    void close(websocket::close_code code);
    websocket::stream<beast::basic_stream<StreamProtocol>> ws_;
    ServeLease lease_;
    bool hasLease_=false,principalCounted_=false;
    // Allow bounded framing/read-ahead slack; Beast enforces the logical 4 KiB message cap.
    beast::flat_buffer input_{MAX_REQUEST_BYTES * 2};
    http::request_parser<http::empty_body> upgrade_;
    asio::steady_timer authDeadline_, ioDeadline_;
    Server& owner_;
    ConnectionState connection_;
    uint16_t port_;
    std::string output_;
    std::shared_ptr<http::response<http::string_body>> page_;
    unsigned controls_ = 0;
    std::chrono::steady_clock::time_point controlWindow_ = std::chrono::steady_clock::now();
    bool opened_ = false, finished_ = false, writing_ = false, stopping_ = false, closing_ = false;
};

#ifdef __linux__
struct OwnedSocketPath {
    std::string path;dev_t device;ino_t inode;
    OwnedSocketPath(std::string p,dev_t d,ino_t i):path(std::move(p)),device(d),inode(i){}
    ~OwnedSocketPath(){struct stat s{};if(::lstat(path.c_str(),&s)==0&&S_ISSOCK(s.st_mode)&&s.st_dev==device&&s.st_ino==inode)::unlink(path.c_str());}
};
#endif
class Server
{
public:
    Server(asio::io_context& io, uint16_t port, uint32_t seed, const SecretDigest& gate, bool partyMode,const ServeOptions* serve)
        : service(gate, seed, partyMode,serve!=nullptr), io_(io), acceptor_(io), signals_(io, SIGINT, SIGTERM), shutdown_(io), lifetime_(io), gameClock_(io)
    {
#ifdef _WIN32
        signals_.add(SIGBREAK);
#endif
        if(serve) {
#ifdef __linux__
            if(::geteuid()==0)throw std::runtime_error("game runtime must be unprivileged");
            if(serve->cloudflare && (serve->peerUid==0||serve->peerUid==::geteuid()||serve->peerGid==0))
                throw std::runtime_error("separate unprivileged gateway identity required");
            options_=*serve;policy_=std::make_unique<ServePolicy>(options_.policy);
            const std::filesystem::path path(options_.socket),parent=path.parent_path();struct stat directory{},existing{};
            if(!path.is_absolute() || path.lexically_normal()!=path || options_.socket.size()>=100 ||
               std::filesystem::canonical(parent)!=parent || ::lstat(parent.c_str(),&directory)!=0 ||
               !S_ISDIR(directory.st_mode)||directory.st_uid!=::geteuid()||
               (serve->cloudflare?(directory.st_gid!=serve->peerGid||(directory.st_mode&0777)!=0710):(directory.st_mode&0077)!=0) ||
               ::lstat(options_.socket.c_str(),&existing)==0 || errno!=ENOENT)
                throw std::runtime_error("unsafe local socket path");
            ::umask(0077);
            localAcceptor_=std::make_unique<asio::local::stream_protocol::acceptor>(io);
            localAcceptor_->open();localAcceptor_->bind(asio::local::stream_protocol::endpoint(options_.socket));
            struct stat bound{};if(::lstat(options_.socket.c_str(),&bound)!=0)throw std::runtime_error("socket creation failed");
            ownedPath_=std::make_unique<OwnedSocketPath>(options_.socket,bound.st_dev,bound.st_ino);
            if(::chmod(options_.socket.c_str(),serve->cloudflare?0660:0600)!=0)throw std::runtime_error("socket permissions failed");
            localAcceptor_->listen(static_cast<int>(MAX_CONNECTIONS));return;
#else
            throw std::runtime_error("Serve requires Linux");
#endif
        }
        // Phase 3F policy: no bind-address option, DNS lookup or public interface.
        const tcp::endpoint endpoint(asio::ip::address_v4::loopback(), port);
        acceptor_.open(endpoint.protocol());
        acceptor_.bind(endpoint);
        acceptor_.listen(static_cast<int>(MAX_CONNECTIONS));
    }
    void run(uint32_t duration)
    {
        if(privateMode())std::cout<<"listening=unix protocol=1\n"<<std::flush;
        else std::cout << "listening=127.0.0.1:" << acceptor_.local_endpoint().port() << " protocol=1\n" << std::flush;
        signals_.async_wait([this](Error ec, int) { if (!ec) stop(); });
        if (duration)
        {
            lifetime_.expires_after(std::chrono::milliseconds(duration));
            lifetime_.async_wait([this](Error ec) { if (!ec) stop(); });
        }
        tick();
        accept();
    }
    void removed(uint64_t serial) { peers_.erase(serial); }
    void stop()
    {
        if (stopped_) return;
        stopped_ = true;
        Error ignored;
        acceptor_.cancel(ignored); acceptor_.close(ignored);
#ifdef __linux__
        if(localAcceptor_){localAcceptor_->cancel(ignored);localAcceptor_->close(ignored);}
#endif
        signals_.cancel(ignored); lifetime_.cancel(); gameClock_.cancel();
        std::vector<std::shared_ptr<Peer>> peers;
        for (auto& entry : peers_) peers.push_back(entry.second);
        for (auto& peer : peers) peer->stop();
        shutdown_.expires_after(1s);
        shutdown_.async_wait([this](Error ec)
        {
            if (ec) return;
            std::vector<std::shared_ptr<Peer>> remaining;
            for (auto& entry : peers_) remaining.push_back(entry.second);
            for (auto& peer : remaining) peer->finish();
            io_.stop();
        });
    }
    Service service;
    bool privateMode() const {return policy_!=nullptr;}
    ServePolicy* policy() {return policy_.get();}
    const ServeOptions& options() const {return options_;}
    bool principalCapacity(const std::string& principal) const {
        unsigned count=0;for(const auto& item:peers_)if(item.second->countsPrincipal(principal))++count;
        return count<2;
    }
private:
    void tick() {
        gameClock_.expires_after(20ms);
        gameClock_.async_wait([this](Error ec) { if(!ec && !stopped_) {
            if(policy_) {
                policy_->refresh();std::vector<std::shared_ptr<Peer>> peers;
                for(auto& item:peers_)peers.push_back(item.second);
                for(auto& peer:peers)peer->checkAdmission();
            }
            service.tick(); tick();
        } });
    }
    void accept()
    {
#ifdef __linux__
        if(localAcceptor_) {
            localAcceptor_->async_accept([this](Error ec,asio::local::stream_protocol::socket socket) {
                if(stopped_)return;
                struct ucred credentials{};socklen_t size=sizeof(credentials);
                if(!ec && ::getsockopt(socket.native_handle(),SOL_SOCKET,SO_PEERCRED,&credentials,&size)==0 &&
                   size==sizeof(credentials) && (options_.cloudflare?
                       (credentials.uid==options_.peerUid&&credentials.gid==options_.peerGid):credentials.uid==0))admit(StreamSocket(std::move(socket)),0);
                accept();
            });return;
        }
#endif
        acceptor_.async_accept([this](Error ec, tcp::socket socket)
        {
            if (stopped_) return;
            if (!ec)
            {
                admit(StreamSocket(std::move(socket)),acceptor_.local_endpoint().port());
            }
            accept();
        });
    }
    void admit(StreamSocket socket,uint16_t port) {
        const auto now=std::chrono::steady_clock::now();
        if(now-acceptWindow_>=1s){acceptWindow_=now;accepts_=0;}
        if(peers_.size()>=MAX_CONNECTIONS || ++accepts_>32 || nextSerial_==UINT64_MAX){Error ignored;socket.close(ignored);return;}
        const auto serial=nextSerial_++;auto peer=std::make_shared<Peer>(std::move(socket),*this,serial,port);
        peers_.emplace(serial,peer);peer->start();
    }
    ServeOptions options_;
    std::unique_ptr<ServePolicy> policy_;
#ifdef __linux__
    std::unique_ptr<asio::local::stream_protocol::acceptor> localAcceptor_;
    std::unique_ptr<OwnedSocketPath> ownedPath_;
#endif
    asio::io_context& io_;
    tcp::acceptor acceptor_;
    asio::signal_set signals_;
    asio::steady_timer shutdown_, lifetime_, gameClock_;
    std::map<uint64_t, std::shared_ptr<Peer>> peers_;
    uint64_t nextSerial_ = 1;
    unsigned accepts_ = 0;
    std::chrono::steady_clock::time_point acceptWindow_ = std::chrono::steady_clock::now();
    bool stopped_ = false;
};

Peer::Peer(StreamSocket socket, Server& owner, uint64_t serial, uint16_t port)
    : ws_(std::move(socket)), authDeadline_(ws_.get_executor()), ioDeadline_(ws_.get_executor()),
      owner_(owner), port_(port)
{ connection_.serial = serial; }
bool Peer::checkAdmission() {
    if(finished_)return false;
    if(hasLease_ && !owner_.policy()->valid(lease_)){finish();return false;}
    return true;
}
void Peer::arm(unsigned seconds)
{
    ioDeadline_.expires_after(std::chrono::seconds(seconds));
    ioDeadline_.async_wait([self = shared_from_this()](Error ec) { if (!ec) self->finish(); });
}
void Peer::start()
{
    auto self = shared_from_this();
    authDeadline_.expires_after(std::chrono::seconds(AUTH_SECONDS));
    authDeadline_.async_wait([self](Error ec) { if (!ec) self->finish(); });
    upgrade_.header_limit(MAX_REQUEST_BYTES);
    upgrade_.body_limit(0);
    http::async_read(ws_.next_layer(), input_, upgrade_, [self](Error ec, std::size_t)
    {
        if (ec || self->finished_) { self->finish(); return; }
        const auto& req = self->upgrade_.get();
        // TCP loopback authenticates no proxy process. Never infer a verified
        // person from forwarded headers on this development-only transport.
        // Private Unix mode checks kernel peer credentials before parsing HTTP.
        if(!self->owner_.privateMode())for (const auto& field : req) {
            const auto name=field.name_string();
            if (beast::iequals(name.substr(0,10),"Tailscale-") ||
                beast::iequals(name.substr(0,12),"X-Forwarded-") ||
                beast::iequals(name,"Forwarded") || beast::iequals(name,"X-Real-IP")) {
                self->finish(); return;
            }
        }
        const auto host = req[http::field::host];
        // Serve 1.104.4 rewrites Unix-backend Host to localhost; it does not
        // change the browser Origin. Only the root-verified Unix hop uses this.
        const std::string expected = self->owner_.privateMode()?"localhost":"127.0.0.1:" + std::to_string(self->port_);
        const std::string local = "localhost:" + std::to_string(self->port_);
        if(req.method()!=http::verb::get || (host!=expected && (self->owner_.privateMode()||host!=local)) ||
           req.count(http::field::host)!=1 || self->input_.size()!=0) { self->finish(); return; }
        if(self->owner_.privateMode()) {
            const bool cloudflare=self->owner_.options().cloudflare;
            const char* identityHeader=cloudflare?"Pfme-Verified-Identity":"Tailscale-User-Login";
            if(req.count(identityHeader)!=1 || req.count(cloudflare?"Tailscale-User-Login":"Pfme-Verified-Identity")!=0 ||
               req.count("Cf-Access-Jwt-Assertion")!=0 ||
               !self->owner_.policy()->admit(std::string(req[identityHeader]),self->lease_)) {self->finish();return;}
            self->hasLease_=true;self->connection_.verifiedPrincipal=self->lease_.principal;
        }
#ifdef PFME_HAS_PLAYABLE
        // Development uses its loopback origin; private mode retains the exact
        // configured external HTTPS origin despite the backend Host rewrite.
        if(req.count(http::field::origin)>1 || (req.count(http::field::origin)==1 &&
           req[http::field::origin] != (self->owner_.privateMode()?self->owner_.options().origin:"http://"+std::string(host)))) { self->finish(); return; }
        if(!websocket::is_upgrade(req)) {
            const char* body=nullptr; const char* type=nullptr;
            if(req.target()=="/play") { body=play_html; type="text/html; charset=utf-8"; }
            else if(req.target()=="/play.js") { body=play_js; type="text/javascript; charset=utf-8"; }
            else if(req.target()=="/audio.js") { body=audio_js; type="text/javascript; charset=utf-8"; }
            else if(req.target()=="/play.css") { body=play_css; type="text/css; charset=utf-8"; }
            if(!body) { self->finish(); return; }
            self->page_=std::make_shared<http::response<http::string_body>>(http::status::ok,11);
            auto& page=*self->page_;
            page.set(http::field::content_type,type);
            page.set(http::field::cache_control,"no-store");
            page.set("X-Content-Type-Options","nosniff");
            page.set("Referrer-Policy","no-referrer");
            const std::string websocketOrigin = self->owner_.privateMode()?
                "wss://"+self->owner_.options().origin.substr(8):"ws://"+std::string(host);
            page.set("Content-Security-Policy","default-src 'none'; script-src 'self'; style-src 'self'; connect-src "+websocketOrigin+"; frame-ancestors 'none'; base-uri 'none'; form-action 'none'");
            page.keep_alive(false); page.body()=body;
            if(self->owner_.privateMode() && body==play_html) {
                const std::string marker="data-auth-mode=\"development\"";
                const auto position=page.body().find(marker);
                if(position==std::string::npos){self->finish();return;}
                page.body().replace(position,marker.size(),"data-auth-mode=\"serve\"");
            }
            page.prepare_payload();
            self->arm(WRITE_SECONDS);
            http::async_write(self->ws_.next_layer(),page,[self](Error,std::size_t){self->finish();});
            return;
        }
#else
        if(req.count(http::field::origin)!=0) { self->finish(); return; }
#endif
        if(self->owner_.privateMode() && req.count(http::field::origin)!=1){self->finish();return;}
        if(req.target()!="/" || !websocket::is_upgrade(req)) { self->finish(); return; }
        if(self->owner_.privateMode()) {
            if(!self->owner_.principalCapacity(self->connection_.verifiedPrincipal)){self->finish();return;}
            self->principalCounted_=true;
        }
        websocket::stream_base::timeout timeouts;
        timeouts.handshake_timeout = std::chrono::seconds(AUTH_SECONDS);
        timeouts.idle_timeout = std::chrono::seconds(IO_SECONDS);
        timeouts.keep_alive_pings = true;
        self->ws_.set_option(timeouts);
        self->ws_.read_message_max(MAX_REQUEST_BYTES);
        self->ws_.control_callback([weak = self->weak_from_this()](websocket::frame_type, beast::string_view)
        {
            if (auto peer = weak.lock())
            {
                const auto now = std::chrono::steady_clock::now();
                if (now - peer->controlWindow_ >= 1s) { peer->controlWindow_ = now; peer->controls_ = 0; }
                if (++peer->controls_ > 40) peer->finish();
            }
        });
        self->ws_.async_accept(req, [self](Error error)
        {
            if (error || self->finished_) { self->finish(); return; }
            self->opened_ = true;
            self->send(Service::hello(self->owner_.privateMode()));
        });
    });
}
void Peer::read()
{
    if (finished_ || stopping_ || closing_ || !checkAdmission()) return;
    arm(IO_SECONDS);
    ws_.async_read(input_, [self = shared_from_this()](Error ec, std::size_t)
    {
        if (ec || self->finished_) { self->finish(); return; }
        if (self->closing_ || !self->checkAdmission()) return;
        if (!self->ws_.got_text())
        { self->send(json::serialize(failure("", "TextRequired")), true); return; }
        const auto text = beast::buffers_to_string(self->input_.data());
        self->input_.consume(self->input_.size());
        auto result = self->owner_.service.handle(self->connection_, text);
        if (self->connection_.authenticated) self->authDeadline_.cancel();
        self->send(std::move(result.text), result.close);
    });
}
void Peer::send(std::string text, bool shouldClose)
{
    if (finished_) return;
    if (text.size() > MAX_RESPONSE_BYTES) { finish(); return; }
    output_ = std::move(text);
    writing_ = true;
    arm(WRITE_SECONDS);
    ws_.text(true);
    ws_.async_write(asio::buffer(output_), [self = shared_from_this(), shouldClose](Error ec, std::size_t)
    {
        self->writing_ = false;
        if (ec || self->finished_) { self->finish(); return; }
        self->ioDeadline_.cancel();
        self->output_.clear();
        if (shouldClose || self->stopping_) self->close(shouldClose ? websocket::close_code::policy_error : websocket::close_code::normal);
        else self->read();
    });
}
void Peer::close(websocket::close_code code)
{
    if (finished_ || closing_) return;
    closing_ = true;
    arm(AUTH_SECONDS);
    ws_.async_close(code, [self = shared_from_this()](Error) { self->finish(); });
}
void Peer::stop()
{
    stopping_ = true;
    if (!opened_) finish();
    else if (!writing_) close(websocket::close_code::normal);
}
void Peer::finish()
{
    if (finished_) return;
    finished_ = true;
    authDeadline_.cancel(); ioDeadline_.cancel();
    Error ignored;
    ws_.next_layer().socket().shutdown(tcp::socket::shutdown_both, ignored);
    ws_.next_layer().socket().close(ignored);
    owner_.service.detach(connection_);
    owner_.removed(connection_.serial);
}
int runServer(uint16_t port, uint32_t seed, uint32_t duration, const SecretDigest& gate, bool partyMode,const ServeOptions* serve)
{
    asio::io_context io(1);
    Server server(io, port, seed, gate, partyMode,serve);
    server.run(duration);
    io.run();
    std::cout << "stopped\n" << std::flush;
    return 0;
}
}
