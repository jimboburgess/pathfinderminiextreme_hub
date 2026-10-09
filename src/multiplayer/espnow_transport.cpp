#include "espnow_transport.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <string.h>

namespace
{
// Activity snapshots are paced one acknowledged frame at a time. Six slots
// still cover control traffic without spending another full frame per slot.
constexpr uint8_t RECEIVE_QUEUE_LENGTH = 6;
constexpr uint8_t BROADCAST_ADDRESS_BYTES[6] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

StaticQueue_t receiveQueueControl;
uint8_t receiveQueueStorage[
    RECEIVE_QUEUE_LENGTH * sizeof(ReceivedNetworkFrame)] = {};
QueueHandle_t receiveQueue = nullptr;
volatile uint32_t droppedReceiveCount = 0;
volatile uint32_t immediateSendFailureCount = 0;
volatile uint32_t sendCompletionSuccessCount = 0;
volatile uint32_t sendCompletionFailureCount = 0;

void onEspNowSend(const uint8_t*, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS)
        ++sendCompletionSuccessCount;
    else
        ++sendCompletionFailureCount;
}

void onEspNowReceive(
    const uint8_t* senderAddress,
    const uint8_t* data,
    int dataLength)
{
    if (receiveQueue == nullptr || senderAddress == nullptr || data == nullptr ||
        dataLength <= 0 || dataLength > static_cast<int>(ESPNOW_MAX_PACKET_SIZE))
        return;

    ReceivedNetworkFrame frame{};
    memcpy(frame.sender.bytes, senderAddress, sizeof(frame.sender.bytes));
    frame.size = static_cast<uint16_t>(dataLength);
    memcpy(frame.data, data, frame.size);

    if (xQueueSend(receiveQueue, &frame, 0) != pdTRUE)
        ++droppedReceiveCount;
}

TransportAddress broadcastAddress()
{
    TransportAddress address{};
    memcpy(address.bytes, BROADCAST_ADDRESS_BYTES, sizeof(address.bytes));
    return address;
}
}

EspNowTransport multiplayerTransport;

bool EspNowTransport::begin()
{
    if (initialized) return true;

    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
    if (!WiFi.mode(WIFI_STA)) return false;

    if (receiveQueue == nullptr)
    {
        receiveQueue = xQueueCreateStatic(
            RECEIVE_QUEUE_LENGTH,
            sizeof(ReceivedNetworkFrame),
            receiveQueueStorage,
            &receiveQueueControl);
    }
    if (receiveQueue == nullptr) return false;

    if (esp_now_init() != ESP_OK) return false;
    if (esp_now_register_recv_cb(onEspNowReceive) != ESP_OK)
    {
        esp_now_deinit();
        return false;
    }
    if (esp_now_register_send_cb(onEspNowSend) != ESP_OK)
    {
        esp_now_deinit();
        return false;
    }

    uint8_t rawAddress[6] = {};
    if (esp_wifi_get_mac(WIFI_IF_STA, rawAddress) != ESP_OK)
    {
        esp_now_deinit();
        return false;
    }
    memcpy(localAddress.bytes, rawAddress, sizeof(localAddress.bytes));

    initialized = true;
    if (!addPeer(broadcastAddress()))
    {
        initialized = false;
        esp_now_deinit();
        return false;
    }
    return true;
}

bool EspNowTransport::isReady() const
{
    return initialized;
}

bool EspNowTransport::sendBroadcast(const uint8_t* data, size_t size)
{
    return sendTo(broadcastAddress(), data, size);
}

bool EspNowTransport::sendTo(
    const TransportAddress& destination,
    const uint8_t* data,
    size_t size)
{
    if (!initialized || data == nullptr || size == 0 ||
        size > ESPNOW_MAX_PACKET_SIZE || !addPeer(destination))
        return false;

    const esp_err_t result = esp_now_send(
        destination.bytes,
        data,
        static_cast<int>(size));
    if (result != ESP_OK) ++immediateSendFailureCount;
    return result == ESP_OK;
}

bool EspNowTransport::receive(ReceivedNetworkFrame& frame)
{
    return initialized && receiveQueue != nullptr &&
        xQueueReceive(receiveQueue, &frame, 0) == pdTRUE;
}

bool EspNowTransport::addPeer(const TransportAddress& address)
{
    if (!initialized) return false;
    if (esp_now_is_peer_exist(address.bytes)) return true;

    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, address.bytes, sizeof(peer.peer_addr));
    peer.channel = 0;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    return esp_now_add_peer(&peer) == ESP_OK;
}

void EspNowTransport::removePeer(const TransportAddress& address)
{
    if (initialized && esp_now_is_peer_exist(address.bytes))
        esp_now_del_peer(address.bytes);
}

TransportAddress EspNowTransport::getLocalAddress() const
{
    return localAddress;
}

uint32_t EspNowTransport::getDroppedReceiveCount() const
{
    return droppedReceiveCount;
}

uint32_t EspNowTransport::getImmediateSendFailureCount() const
{
    return immediateSendFailureCount;
}

uint32_t EspNowTransport::getSendCompletionSuccessCount() const
{
    return sendCompletionSuccessCount;
}

uint32_t EspNowTransport::getSendCompletionFailureCount() const
{
    return sendCompletionFailureCount;
}

uint8_t EspNowTransport::getReceiveQueueCapacity() const
{
    return RECEIVE_QUEUE_LENGTH;
}
