#ifndef PFME_SERVER_TRANSPORT_H
#define PFME_SERVER_TRANSPORT_H
#include "credentials.h"
#include <cstdint>
namespace pfme::server
{
int runServer(uint16_t port, uint32_t seed, uint32_t runMilliseconds, const SecretDigest& gate, bool partyMode=false);
}
#endif
