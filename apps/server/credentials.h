#ifndef PFME_SERVER_CREDENTIALS_H
#define PFME_SERVER_CREDENTIALS_H
#include <array>
#include <string>
#include <string_view>
namespace pfme::server
{
using SecretDigest = std::array<unsigned char, 32>;
std::string randomCredential();
bool credentialDigest(std::string_view hex, SecretDigest& digest);
bool sameDigest(const SecretDigest& a, const SecretDigest& b);
}
#endif
