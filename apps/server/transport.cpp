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

namespace pfme::server
{
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;
using Error = boost::system::error_code;
using namespace std::chrono_literals;
#ifdef PFME_HAS_PLAYABLE
#include "web_assets.inc"
#endif

class Server;
class Peer : public std::enable_shared_from_this<Peer>
{
public:
    Peer(tcp::socket socket, Server& owner, uint64_t serial, uint16_t port);
    void start();
    void stop();
    void finish();
private:
    void arm(unsigned seconds);
    void read();
    void send(std::string text, bool close = false);
    void close(websocket::close_code code);
    websocket::stream<beast::tcp_stream> ws_;
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

class Server
{
public:
    Server(asio::io_context& io, uint16_t port, uint32_t seed, const SecretDigest& gate)
        : service(gate, seed), io_(io), acceptor_(io), signals_(io, SIGINT, SIGTERM), shutdown_(io), lifetime_(io), gameClock_(io)
    {
#ifdef _WIN32
        signals_.add(SIGBREAK);
#endif
        // Phase 3F policy: no bind-address option, DNS lookup or public interface.
        const tcp::endpoint endpoint(asio::ip::address_v4::loopback(), port);
        acceptor_.open(endpoint.protocol());
        acceptor_.bind(endpoint);
        acceptor_.listen(static_cast<int>(MAX_CONNECTIONS));
    }
    void run(uint32_t duration)
    {
        std::cout << "listening=127.0.0.1:" << acceptor_.local_endpoint().port() << " protocol=1\n" << std::flush;
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
private:
    void tick() {
        gameClock_.expires_after(20ms);
        gameClock_.async_wait([this](Error ec) { if(!ec && !stopped_) { service.tick(); tick(); } });
    }
    void accept()
    {
        acceptor_.async_accept([this](Error ec, tcp::socket socket)
        {
            if (stopped_) return;
            if (!ec)
            {
                if (peers_.size() >= MAX_CONNECTIONS || ++accepts_ > 32 || nextSerial_ == UINT64_MAX)
                { Error ignored; socket.close(ignored); }
                else
                {
                    const auto serial = nextSerial_++;
                    auto peer = std::make_shared<Peer>(std::move(socket), *this, serial, acceptor_.local_endpoint().port());
                    peers_.emplace(serial, peer); peer->start();
                }
            }
            const auto now = std::chrono::steady_clock::now();
            if (now - acceptWindow_ >= 1s) { acceptWindow_ = now; accepts_ = 0; }
            accept();
        });
    }
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

Peer::Peer(tcp::socket socket, Server& owner, uint64_t serial, uint16_t port)
    : ws_(std::move(socket)), authDeadline_(ws_.get_executor()), ioDeadline_(ws_.get_executor()),
      owner_(owner), port_(port)
{ connection_.serial = serial; }
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
        const auto host = req[http::field::host];
        const std::string expected = "127.0.0.1:" + std::to_string(self->port_);
        const std::string local = "localhost:" + std::to_string(self->port_);
        if(req.method()!=http::verb::get || (host!=expected && host!=local) ||
           req.count(http::field::host)!=1 || self->input_.size()!=0) { self->finish(); return; }
#ifdef PFME_HAS_PLAYABLE
        // Same-origin loopback only. SSH forwarding must preserve the local port.
        if(req.count(http::field::origin)>1 || (req.count(http::field::origin)==1 &&
           req[http::field::origin] != "http://"+std::string(host))) { self->finish(); return; }
        if(!websocket::is_upgrade(req)) {
            const char* body=nullptr; const char* type=nullptr;
            if(req.target()=="/play") { body=play_html; type="text/html; charset=utf-8"; }
            else if(req.target()=="/play.js") { body=play_js; type="text/javascript; charset=utf-8"; }
            else if(req.target()=="/play.css") { body=play_css; type="text/css; charset=utf-8"; }
            if(!body) { self->finish(); return; }
            self->page_=std::make_shared<http::response<http::string_body>>(http::status::ok,11);
            auto& page=*self->page_;
            page.set(http::field::content_type,type);
            page.set(http::field::cache_control,"no-store");
            page.set("X-Content-Type-Options","nosniff");
            page.set("Referrer-Policy","no-referrer");
            page.set("Content-Security-Policy","default-src 'none'; script-src 'self'; style-src 'self'; connect-src ws://"+std::string(host)+"; frame-ancestors 'none'; base-uri 'none'; form-action 'none'");
            page.keep_alive(false); page.body()=body; page.prepare_payload();
            self->arm(WRITE_SECONDS);
            http::async_write(self->ws_.next_layer(),page,[self](Error,std::size_t){self->finish();});
            return;
        }
#else
        if(req.count(http::field::origin)!=0) { self->finish(); return; }
#endif
        if(req.target()!="/" || !websocket::is_upgrade(req)) { self->finish(); return; }
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
            self->send(Service::hello());
        });
    });
}
void Peer::read()
{
    if (finished_ || stopping_ || closing_) return;
    arm(IO_SECONDS);
    ws_.async_read(input_, [self = shared_from_this()](Error ec, std::size_t)
    {
        if (ec || self->finished_) { self->finish(); return; }
        if (self->closing_) return;
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
int runServer(uint16_t port, uint32_t seed, uint32_t duration, const SecretDigest& gate)
{
    asio::io_context io(1);
    Server server(io, port, seed, gate);
    server.run(duration);
    io.run();
    std::cout << "stopped\n" << std::flush;
    return 0;
}
}
