#include "multiplayer/itemtrade.h"

namespace
{
bool validLine(const TradeOfferLine& line)
{
    return isValidItemInstance(line.item) && line.quantity > 0;
}

bool ownsLines(const Character& character,
               const TradeOfferLine* lines,
               uint8_t lineCount)
{
    for (uint8_t i = 0; i < lineCount; ++i)
    {
        uint16_t requested = 0;
        for (uint8_t j = 0; j < lineCount; ++j)
            if (lines[j].item == lines[i].item)
                requested += lines[j].quantity;
        if (requested > getItemQuantity(character, lines[i].item))
            return false;
    }
    return true;
}
}

const TradeOffer* TradeOfferBook::pendingFor(PlayerID player) const
{
    for (const TradeOffer& offer : offers_)
        if (offer.status == TradeOfferStatus::PENDING &&
            (offer.sender == player || offer.recipient == player))
            return &offer;
    return nullptr;
}

const TradeOffer* TradeOfferBook::find(uint32_t offerID) const
{
    if (offerID == 0) return nullptr;
    for (const TradeOffer& offer : offers_)
        if (offer.id == offerID) return &offer;
    return nullptr;
}

TradeResult TradeOfferBook::propose(
    PlayerID sender,
    PlayerID recipient,
    const TradeOfferLine* lines,
    uint8_t lineCount,
    const Character& senderCharacter,
    uint32_t& offerID)
{
    offerID = 0;
    if (!isValidPlayerID(sender) || !isValidPlayerID(recipient) ||
        sender == recipient)
        return TradeResult::INVALID_PLAYER;
    if (pendingFor(sender) != nullptr || pendingFor(recipient) != nullptr)
        return TradeResult::PLAYER_BUSY;
    if (lines == nullptr || lineCount == 0 ||
        lineCount > MAX_TRADE_OFFER_ITEMS)
        return TradeResult::INVALID_ITEMS;
    for (uint8_t i = 0; i < lineCount; ++i)
        if (!validLine(lines[i])) return TradeResult::INVALID_ITEMS;
    if (!ownsLines(senderCharacter, lines, lineCount))
        return TradeResult::NOT_OWNED;

    TradeOffer* slot = nullptr;
    for (TradeOffer& offer : offers_)
        if (offer.status != TradeOfferStatus::PENDING)
        {
            slot = &offer;
            break;
        }
    if (slot == nullptr) return TradeResult::PLAYER_BUSY;

    *slot = TradeOffer{};
    slot->id = nextID_++;
    if (slot->id == 0) slot->id = nextID_++;
    slot->sender = sender;
    slot->recipient = recipient;
    slot->lineCount = lineCount;
    slot->status = TradeOfferStatus::PENDING;
    for (uint8_t i = 0; i < lineCount; ++i)
        slot->lines[i] = lines[i];
    offerID = slot->id;
    return TradeResult::SUCCESS;
}

TradeResult TradeOfferBook::accept(
    uint32_t offerID,
    PlayerID recipient,
    Character& senderCharacter,
    Character& recipientCharacter)
{
    TradeOffer* offer = nullptr;
    for (TradeOffer& candidate : offers_)
        if (candidate.id == offerID)
        {
            offer = &candidate;
            break;
        }
    if (offer == nullptr || offer->status != TradeOfferStatus::PENDING)
        return TradeResult::NOT_FOUND;
    if (offer->recipient != recipient)
        return TradeResult::NOT_RECIPIENT;
    if (!ownsLines(senderCharacter, offer->lines, offer->lineCount))
    {
        offer->status = TradeOfferStatus::CANCELLED;
        return TradeResult::NOT_OWNED;
    }

    InventoryData recipientPreview = recipientCharacter.inventory;
    for (uint8_t i = 0; i < offer->lineCount; ++i)
        if (!addInventoryItem(
                recipientPreview,
                offer->lines[i].item,
                offer->lines[i].quantity))
        {
            offer->status = TradeOfferStatus::CANCELLED;
            return TradeResult::INVENTORY_FULL;
        }

    const InventoryData senderBefore = senderCharacter.inventory;
    const InventoryData recipientBefore = recipientCharacter.inventory;
    for (uint8_t i = 0; i < offer->lineCount; ++i)
    {
        const TradeOfferLine& line = offer->lines[i];
        if (!removeInventoryItem(
                senderCharacter.inventory, line.item, line.quantity) ||
            !addInventoryItem(
                recipientCharacter.inventory, line.item, line.quantity))
        {
            senderCharacter.inventory = senderBefore;
            recipientCharacter.inventory = recipientBefore;
            offer->status = TradeOfferStatus::CANCELLED;
            return TradeResult::NOT_OWNED;
        }
    }

    offer->status = TradeOfferStatus::ACCEPTED;
    return TradeResult::SUCCESS;
}

TradeResult TradeOfferBook::decline(uint32_t offerID, PlayerID recipient)
{
    for (TradeOffer& offer : offers_)
        if (offer.id == offerID && offer.status == TradeOfferStatus::PENDING)
        {
            if (offer.recipient != recipient)
                return TradeResult::NOT_RECIPIENT;
            offer.status = TradeOfferStatus::DECLINED;
            return TradeResult::SUCCESS;
        }
    return TradeResult::NOT_FOUND;
}

bool TradeOfferBook::cancel(uint32_t offerID)
{
    for (TradeOffer& offer : offers_)
        if (offer.id == offerID && offer.status == TradeOfferStatus::PENDING)
        {
            offer.status = TradeOfferStatus::CANCELLED;
            return true;
        }
    return false;
}

void TradeOfferBook::cancelForPlayer(PlayerID player)
{
    for (TradeOffer& offer : offers_)
        if (offer.status == TradeOfferStatus::PENDING &&
            (offer.sender == player || offer.recipient == player))
            offer.status = TradeOfferStatus::CANCELLED;
}
