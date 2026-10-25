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
    for (int i = 1; i < argc; i += 2)
    {
        if (i + 1 >= argc)
        { std::cerr << "usage: pfme_server [--port N] [--seed N] [--run-for-ms N]\n"; return 2; }
        const std::string option = argv[i];
        bool valid = false;
        if (option == "--port") valid = number(argv[i + 1], 65535, port) && (port == 0 || port >= 1024);
        else if (option == "--seed") valid = number(argv[i + 1], UINT32_MAX, seed);
        else if (option == "--run-for-ms") valid = number(argv[i + 1], 3600000, duration) && duration > 0;
        if (!valid) { std::cerr << "invalid startup option\n"; return 2; }
    }
    pfme::server::SecretDigest gate{};
    const char* secret = std::getenv("PFME_DEV_SECRET");
    if (!secret || !pfme::server::credentialDigest(secret, gate))
    { std::cerr << "PFME_DEV_SECRET must contain a securely generated 64-character lowercase hex secret\n"; return 2; }
    try
    {
        return pfme::server::runServer(static_cast<uint16_t>(port), static_cast<uint32_t>(seed),
                                      static_cast<uint32_t>(duration), gate);
    }
    catch (...) { std::cerr << "server startup failed\n"; return 1; }
}
