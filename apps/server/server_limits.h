#ifndef PFME_SERVER_LIMITS_H
#define PFME_SERVER_LIMITS_H
#include <cstddef>
namespace pfme::server
{
constexpr unsigned PROTOCOL_VERSION = 1;
constexpr std::size_t MAX_CONNECTIONS = 8;
constexpr std::size_t MAX_REQUEST_BYTES = 4096;
constexpr std::size_t MAX_RESPONSE_BYTES = 16384;
constexpr unsigned MAX_JSON_DEPTH = 8;
constexpr unsigned COMMANDS_PER_SECOND = 20;
constexpr unsigned AUTH_FAILURE_LIMIT = 3;
constexpr unsigned AUTH_SECONDS = 3;
constexpr unsigned IO_SECONDS = 15;
constexpr unsigned WRITE_SECONDS = 5;
}
#endif
