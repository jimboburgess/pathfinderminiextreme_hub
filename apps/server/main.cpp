#include "transport.h"
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
bool number(const char* text, uint64_t maximum, uint64_t& result)
{
    result = 0;
    if (!text || !*text) return false;
    for (; *text; ++text)
    {
        if (*text < '0' || *text > '9') return false;
        const unsigned digit = static_cast<unsigned>(*text - '0');
        if (result > (maximum - digit) / 10) return false;
        result = result * 10 + digit;
    }
    return result <= maximum;
}
}
int main(int argc, char** argv)
{
    uint64_t port = 8765, seed = 12345, duration = 0;
    bool partyMode=false,portSpecified=false,serveSpecified=false,cloudflareSpecified=false;
    pfme::server::ServeOptions serve;
    for (int i = 1; i < argc; i += 2)
    {
        if (i + 1 >= argc)
        { std::cerr << "usage: pfme_server [--port N] [--seed N] [--run-for-ms N] [--runtime single|party] [--serve-socket PATH --serve-origin HTTPS_URL --serve-policy PATH --run-for-ms N] [--cloudflare-socket PATH --cloudflare-origin HTTPS_URL --cloudflare-policy PATH --cloudflare-peer-uid N --cloudflare-peer-gid N --runtime party --run-for-ms N]\n"; return 2; }
        const std::string option = argv[i];
        bool valid = false;
        if(option.rfind("--serve-",0)==0)serveSpecified=true;
        if(option.rfind("--cloudflare-",0)==0)cloudflareSpecified=true;
        if(option=="--serve-socket") {serve.socket=argv[i+1];valid=!serve.socket.empty();}
        else if(option=="--serve-origin") {serve.origin=argv[i+1];valid=pfme::server::validServeOrigin(serve.origin);}
        else if(option=="--serve-policy") {serve.policy=argv[i+1];valid=!serve.policy.empty();}
        else if(option=="--cloudflare-socket") {serve.socket=argv[i+1];valid=!serve.socket.empty();}
        else if(option=="--cloudflare-origin") {serve.origin=argv[i+1];valid=pfme::server::validCloudflareOrigin(serve.origin);}
        else if(option=="--cloudflare-policy") {serve.policy=argv[i+1];valid=!serve.policy.empty();}
        else if(option=="--cloudflare-peer-uid") {uint64_t value=0;valid=number(argv[i+1],UINT32_MAX,value)&&value>0;serve.peerUid=static_cast<uint32_t>(value);}
        else if(option=="--cloudflare-peer-gid") {uint64_t value=0;valid=number(argv[i+1],UINT32_MAX,value)&&value>0;serve.peerGid=static_cast<uint32_t>(value);}
        else if(option=="--runtime") {const std::string mode=argv[i+1];valid=mode=="single" || mode=="party";partyMode=mode=="party";}
        else if (option == "--port") {portSpecified=true;valid = number(argv[i + 1], 65535, port) && (port == 0 || port >= 1024);}
        else if (option == "--seed") valid = number(argv[i + 1], UINT32_MAX, seed);
        else if (option == "--run-for-ms") valid = number(argv[i + 1], 86400000, duration) && duration > 0;
        if (!valid) { std::cerr << "invalid startup option\n"; return 2; }
    }
    pfme::server::SecretDigest gate{};
    const char* secret = std::getenv("PFME_DEV_SECRET");
    serve.cloudflare=cloudflareSpecified;
    if((serveSpecified&&cloudflareSpecified)||(cloudflareSpecified&&(!serve.peerUid||!serve.peerGid)))
    {std::cerr<<"conflicting or incomplete Cloudflare configuration\n";return 2;}
    const bool privateMode=serveSpecified||cloudflareSpecified;
    if(privateMode && (serve.socket.empty()||serve.origin.empty()||serve.policy.empty()||portSpecified||!partyMode||duration==0))
    {std::cerr<<"incomplete or conflicting Serve configuration\n";return 2;}
#ifndef __linux__
    if(privateMode){std::cerr<<"Serve mode requires Linux\n";return 2;}
#endif
    if (!privateMode && (!secret || !pfme::server::credentialDigest(secret, gate)))
    { std::cerr << "PFME_DEV_SECRET must contain a securely generated 64-character lowercase hex secret\n"; return 2; }
    try
    {
        return pfme::server::runServer(static_cast<uint16_t>(port), static_cast<uint32_t>(seed),
                                      static_cast<uint32_t>(duration), gate, partyMode,privateMode?&serve:nullptr);
    }
    catch (...) { std::cerr << "server startup failed\n"; return 1; }
}
