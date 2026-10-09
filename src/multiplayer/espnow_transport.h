#ifndef PATHFINDERMINIEXTREME_025_ESPNOW_TRANSPORT_H
#define PATHFINDERMINIEXTREME_025_ESPNOW_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#include "network_protocol.h"

struct TransportAddress
{
    uint8_t bytes[6] = {};
};

inline bool operator==(
    const TransportAddress& first,
    const TransportAddress& second)
{
    for (uint8_t i = 0; i < sizeof(first.bytes); ++i)
        if (first.bytes[i] != second.bytes[i]) return false;
    return true;
}

inline bool operator!=(
    const TransportAddress& first,
    const TransportAddress& second)
{
    return !(first == second);
}

struct ReceivedNetworkFrame
{
    TransportAddress sender{};
    uint16_t size = 0;
    uint8_t data[ESPNOW_MAX_PACKET_SIZE] = {};
};

class EspNowTransport
{
public:
    bool begin();
    bool isReady() const;
    bool sendBroadcast(const uint8_t* data, size_t size);
    bool sendTo(
        const TransportAddress& destination,
        const uint8_t* data,
        size_t size);
    bool receive(ReceivedNetworkFrame& frame);
    bool addPeer(const TransportAddress& address);
    void removePeer(const TransportAddress& address);
    TransportAddress getLocalAddress() const;
    uint32_t getDroppedReceiveCount() const;
    uint32_t getImmediateSendFailureCount() const;
    uint32_t getSendCompletionSuccessCount() const;
    uint32_t getSendCompletionFailureCount() const;
    uint8_t getReceiveQueueCapacity() const;

private:
    bool initialized = false;
    TransportAddress localAddress{};
};

extern EspNowTransport multiplayerTransport;

#endif
