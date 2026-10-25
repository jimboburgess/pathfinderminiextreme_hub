#include "credentials.h"
#include <monocypher.h>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <sys/random.h>
#include <cerrno>
#endif

namespace pfme::server
{
std::string randomCredential()
{
    unsigned char bytes[32];
#ifdef _WIN32
    if (BCryptGenRandom(nullptr, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        throw std::runtime_error("random source unavailable");
#else
    std::size_t used = 0;
    while (used < sizeof(bytes))
    {
        const auto n = getrandom(bytes + used, sizeof(bytes) - used, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) throw std::runtime_error("random source unavailable");
        used += static_cast<std::size_t>(n);
    }
#endif
    const char* alphabet = "0123456789abcdef";
    std::string result(64, '0');
    for (std::size_t i = 0; i < sizeof(bytes); ++i)
    { result[2*i] = alphabet[bytes[i] >> 4]; result[2*i+1] = alphabet[bytes[i] & 15]; }
    crypto_wipe(bytes, sizeof(bytes));
    return result;
}

bool credentialDigest(std::string_view hex, SecretDigest& digest)
{
    if (hex.size() != 64) return false;
    unsigned char bytes[32] = {};
    unsigned invalid = 0;
    for (std::size_t i = 0; i < hex.size(); ++i)
    {
        const char c = hex[i];
        const unsigned value = c >= '0' && c <= '9' ? static_cast<unsigned>(c - '0') :
            c >= 'a' && c <= 'f' ? static_cast<unsigned>(c - 'a' + 10) : 256;
        invalid |= value >> 8;
        bytes[i / 2] |= static_cast<unsigned char>((value & 15) << (i % 2 ? 0 : 4));
    }
    crypto_blake2b(digest.data(), digest.size(), bytes, sizeof(bytes));
    crypto_wipe(bytes, sizeof(bytes));
    return invalid == 0;
}
bool sameDigest(const SecretDigest& a, const SecretDigest& b)
{ return crypto_verify32(a.data(), b.data()) == 0; }
}
