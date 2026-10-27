#include "multiplayer/espnow_transport.h"
// No native ESP-NOW adapter. Real session code observes unavailable radio.
EspNowTransport multiplayerTransport;
bool EspNowTransport::begin() { return false; }
bool EspNowTransport::isReady() const { return false; }
bool EspNowTransport::sendBroadcast(const uint8_t*,size_t) { return false; }
bool EspNowTransport::sendTo(const TransportAddress&,const uint8_t*,size_t) { return false; }
bool EspNowTransport::receive(ReceivedNetworkFrame&) { return false; }
bool EspNowTransport::addPeer(const TransportAddress&) { return false; }
void EspNowTransport::removePeer(const TransportAddress&) {}
TransportAddress EspNowTransport::getLocalAddress() const { return {}; }
uint32_t EspNowTransport::getDroppedReceiveCount() const { return 0; }
uint32_t EspNowTransport::getImmediateSendFailureCount() const { return 0; }
uint32_t EspNowTransport::getSendCompletionSuccessCount() const { return 0; }
uint32_t EspNowTransport::getSendCompletionFailureCount() const { return 0; }
uint8_t EspNowTransport::getReceiveQueueCapacity() const { return 0; }
