#include "serve_identity.h"
#include "protocol.h"
#include <algorithm>
#include <stdexcept>
#include <set>
#ifdef __linux__
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif
namespace pfme::server {
namespace {
bool printable(std::string_view text,std::size_t maximum) {
    return !text.empty() && text.size()<=maximum && std::all_of(text.begin(),text.end(),[](unsigned char c){return c>=33&&c<=126;});
}
bool positive(const json::value& v,uint64_t& result) {
    if(v.is_uint64())result=v.as_uint64();
    else if(v.is_int64() && v.as_int64()>0)result=static_cast<uint64_t>(v.as_int64());
    else return false;
    return result>0&&result<=UINT32_MAX;
}
std::string rootPolicy(const std::string& path) {
#ifdef __linux__
    const int fd=::open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0)throw std::runtime_error("admission policy unavailable");
    struct Guard {int fd;~Guard(){::close(fd);}} guard{fd};
    struct stat before{},after{};
    if(::fstat(fd,&before)!=0 || !S_ISREG(before.st_mode) || before.st_uid!=0 ||
       (before.st_mode&0022)!=0 || before.st_nlink!=1 || before.st_size<1 || before.st_size>8192)
        throw std::runtime_error("admission policy permissions");
    std::string text(static_cast<std::size_t>(before.st_size),'\0');std::size_t used=0;
    while(used<text.size()) {const auto n=::read(fd,text.data()+used,text.size()-used);if(n<=0)throw std::runtime_error("admission policy read");used+=static_cast<std::size_t>(n);}
    if(::fstat(fd,&after)!=0 || before.st_size!=after.st_size ||
       before.st_mtim.tv_sec!=after.st_mtim.tv_sec || before.st_mtim.tv_nsec!=after.st_mtim.tv_nsec)
        throw std::runtime_error("admission policy changed during read");
    return text;
#else
    (void)path;throw std::runtime_error("Serve admission requires Linux");
#endif
}
}
bool validServeOrigin(std::string_view origin) {
    constexpr std::string_view prefix="https://";
    if(origin.substr(0,prefix.size())!=prefix)return false;
    const auto host=origin.substr(prefix.size());
    if(host.size()<10 || host.size()>253 || host.substr(host.size()-7)!=".ts.net")return false;
    return std::all_of(host.begin(),host.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='-';}) &&
           host.front()!='.'&&host.front()!='-'&&host.find("..") == std::string_view::npos;
}
bool parseServePolicy(std::string_view text,ServeSnapshot& result) {
    if(text.empty()||text.size()>8192)return false;
    try {
        json::parse_options options;options.max_depth=4;
        auto value=json::parse(text,{},options);if(!value.is_object())return false;
        const auto& object=value.as_object();
        if(object.size()!=3 || !object.contains("version") || !object.contains("revision") || !object.contains("users"))return false;
        uint64_t version=0;ServeSnapshot candidate;
        if(!positive(object.at("version"),version)||version!=1 || !positive(object.at("revision"),candidate.revision) || !object.at("users").is_array())return false;
        const auto& users=object.at("users").as_array();if(users.size()>32)return false;
        std::set<std::string> principals;
        for(const auto& item:users) {
            if(!item.is_object()||item.as_object().size()!=2||!item.as_object().contains("login")||!item.as_object().contains("principal"))return false;
            const auto& u=item.as_object();if(!u.at("login").is_string()||!u.at("principal").is_string())return false;
            const std::string login(u.at("login").as_string()),principal(u.at("principal").as_string());
            if(!printable(login,254)||!printable(principal,64)||!std::all_of(principal.begin(),principal.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_';}) ||
               !candidate.users.emplace(login,principal).second || !principals.insert(principal).second)return false;
        }
        result=std::move(candidate);return true;
    } catch(...) {return false;}
}
bool serveLeaseValid(const ServeSnapshot& current,const ServeLease& lease,ServeClock::time_point now) {
    const auto user=current.users.find(lease.login);
    return lease.revision!=0 && lease.revision==current.revision && now<lease.expires &&
           user!=current.users.end()&&user->second==lease.principal;
}
ServePolicy::ServePolicy(std::string path):path_(std::move(path)) {refresh();if(!healthy_)throw std::runtime_error("invalid admission policy");}
void ServePolicy::refresh() {
    const auto now=ServeClock::now();if(now<nextRead_)return;nextRead_=now+std::chrono::seconds(1);
    try {
        auto text=rootPolicy(path_);ServeSnapshot next;
        if(!parseServePolicy(text,next)||next.revision<snapshot_.revision ||
           (next.revision==snapshot_.revision && text!=lastText_))throw std::runtime_error("invalid policy revision");
        snapshot_=std::move(next);lastText_=std::move(text);healthy_=true;
    }catch(...){healthy_=false;}
}
bool ServePolicy::admit(std::string_view login,ServeLease& lease) {
    refresh();if(!healthy_)return false;
    const auto it=snapshot_.users.find(std::string(login));if(it==snapshot_.users.end())return false;
#ifdef PFME_TEST_SERVE_LIFETIME
    constexpr unsigned lifetime=2;
#else
    constexpr unsigned lifetime=900;
#endif
    lease={it->first,it->second,snapshot_.revision,ServeClock::now()+std::chrono::seconds(lifetime)};return true;
}
bool ServePolicy::valid(const ServeLease& lease) const {return healthy_&&serveLeaseValid(snapshot_,lease,ServeClock::now());}
}
