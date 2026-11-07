#ifndef PATHFINDERMINIEXTREME_025_ITEMTRADE_H
#define PATHFINDERMINIEXTREME_025_ITEMTRADE_H

#include <stdint.h>

#include "characters/characters.h"
#include "multiplayer/multiplayer_types.h"

constexpr uint8_t MAX_TRADE_OFFER_ITEMS = 4;
constexpr uint8_t MAX_PENDING_TRADE_OFFERS = MAX_MULTIPLAYER_PLAYERS;

enum class TradeOfferStatus : uint8_t
{
    EMPTY,
    PENDING,
    ACCEPTED,
    DECLINED,
    CANCELLED
};

enum class TradeResult : uint8_t
{
    SUCCESS,
    INVALID_PLAYER,
    INVALID_ITEMS,
    PLAYER_BUSY,
    NOT_FOUND,
    NOT_RECIPIENT,
    NOT_OWNED,
    INVENTORY_FULL
};

struct TradeOfferLine
{
    ItemInstance item = makeItemInstance(ITEM_NONE);
    uint8_t quantity = 0;
};

struct TradeOffer
{
    uint32_t id = 0;
    PlayerID sender = INVALID_PLAYER_ID;
    PlayerID recipient = INVALID_PLAYER_ID;
    TradeOfferLine lines[MAX_TRADE_OFFER_ITEMS] = {};
    uint8_t lineCount = 0;
    TradeOfferStatus status = TradeOfferStatus::EMPTY;
};

class TradeOfferBook
{
public:
    TradeResult propose(
        PlayerID sender,
        PlayerID recipient,
        const TradeOfferLine* lines,
        uint8_t lineCount,
        const Character& senderCharacter,
        uint32_t& offerID);

    TradeResult accept(
        uint32_t offerID,
        PlayerID recipient,
        Character& senderCharacter,
        Character& recipientCharacter);

    TradeResult decline(uint32_t offerID, PlayerID recipient);
    bool cancel(uint32_t offerID);
    void cancelForPlayer(PlayerID player);

    const TradeOffer* pendingFor(PlayerID player) const;
    const TradeOffer* find(uint32_t offerID) const;

private:
    TradeOffer offers_[MAX_PENDING_TRADE_OFFERS] = {};
    uint32_t nextID_ = 1;
};

#endif
