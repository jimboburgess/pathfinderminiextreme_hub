#include "party_registry.h"

#include <string.h>

namespace pfme
{
namespace
{
const char JOIN_CODE_ALPHABET[] = "23456789ABCDEFGHJKMNPQRSTUVWXYZ";
constexpr size_t JOIN_CODE_ALPHABET_LENGTH = sizeof(JOIN_CODE_ALPHABET) - 1;

bool sameID(const PartyID& left, const PartyID& right)
{
    return left.high == right.high && left.low == right.low;
}

bool validName(const char* name)
{
    if (name == nullptr || name[0] == '\0') return false;
    size_t length = 0;
    while (length < NETWORK_PLAYER_NAME_SIZE && name[length] != '\0')
    {
        const unsigned char character = static_cast<unsigned char>(name[length]);
        if (character < 33 || character > 126) return false;
        ++length;
    }
    return length < NETWORK_PLAYER_NAME_SIZE;
}

bool validClass(CharacterClass characterClass)
{
    return characterClass == CLASS_FIGHTER || characterClass == CLASS_ROGUE ||
           characterClass == CLASS_WIZARD || characterClass == CLASS_CLERIC;
}
}

const char* partyErrorName(PartyError error)
{
    switch (error)
    {
#define PFME_PARTY_ERROR(value) case PartyError::value: return #value
        PFME_PARTY_ERROR(Ok); PFME_PARTY_ERROR(RegistryFull);
        PFME_PARTY_ERROR(PartyNotFound); PFME_PARTY_ERROR(PartyFull);
        PFME_PARTY_ERROR(PartyLocked); PFME_PARTY_ERROR(InvalidCode);
        PFME_PARTY_ERROR(InvalidProfile); PFME_PARTY_ERROR(InvalidPartyHandle);
        PFME_PARTY_ERROR(InvalidMember); PFME_PARTY_ERROR(NotLeader);
        PFME_PARTY_ERROR(AlreadyInActivity); PFME_PARTY_ERROR(NotInTown);
        PFME_PARTY_ERROR(InvalidReconnectToken); PFME_PARTY_ERROR(Disconnected);
        PFME_PARTY_ERROR(AlreadyConnected); PFME_PARTY_ERROR(Closed);
        PFME_PARTY_ERROR(CodeCollision); PFME_PARTY_ERROR(IdCollision);
        PFME_PARTY_ERROR(EntropyUnavailable); PFME_PARTY_ERROR(HistoryGap);
        PFME_PARTY_ERROR(InvalidCursor); PFME_PARTY_ERROR(Exhausted);
        PFME_PARTY_ERROR(InternalSessionError);
#undef PFME_PARTY_ERROR
    }
    return "Unknown";
}

const char* partyActivityName(PartyActivity activity)
{
    switch (activity)
    {
        case PartyActivity::Town: return "town";
        case PartyActivity::Forest: return "forest";
        case PartyActivity::Dungeon: return "dungeon";
    }
    return "unknown";
}

const char* partyLocationName(MultiplayerMemberLocation location)
{
    switch (location)
    {
        case MultiplayerMemberLocation::TOWN: return "town";
        case MultiplayerMemberLocation::CONNECTING: return "connecting";
        case MultiplayerMemberLocation::FOREST: return "forest";
        case MultiplayerMemberLocation::DUNGEON: return "dungeon";
        case MultiplayerMemberLocation::DISCONNECTED: return "disconnected";
    }
    return "unknown";
}

const char* partyCharacterClassName(CharacterClass characterClass)
{
    switch (characterClass)
    {
        case CLASS_FIGHTER: return "fighter";
        case CLASS_ROGUE: return "rogue";
        case CLASS_WIZARD: return "wizard";
        case CLASS_CLERIC: return "cleric";
    }
    return "unknown";
}

const char* partyEventName(PartyEventType type)
{
    switch (type)
    {
        case PartyEventType::PartyCreated: return "party-created";
        case PartyEventType::MemberJoined: return "member-joined";
        case PartyEventType::MemberDisconnected: return "member-disconnected";
        case PartyEventType::MemberReconnected: return "member-reconnected";
        case PartyEventType::MemberLeft: return "member-left";
        case PartyEventType::PartyLocked: return "party-locked";
        case PartyEventType::PartyUnlocked: return "party-unlocked";
        case PartyEventType::ActivityStarting: return "activity-starting";
        case PartyEventType::ActivityStarted: return "activity-started";
        case PartyEventType::PartyClosed: return "party-closed";
    }
    return "unknown";
}

bool ReconnectToken::operator==(const ReconnectToken& other) const
{
    return high_ == other.high_ && low_ == other.low_;
}

SeededPartyEntropy::SeededPartyEntropy(uint64_t seed)
    : state_(seed == 0 ? UINT64_C(0x9e3779b97f4a7c15) : seed)
{
}

bool SeededPartyEntropy::next(uint64_t& value)
{
    uint64_t state = state_;
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    state_ = state;
    value = state * UINT64_C(2685821657736338717);
    return true;
}

PartyRegistry::PartyRegistry(PartyEntropy& entropy) : entropy_(entropy) {}

bool PartyRegistry::normalizeJoinCode(const char* input,
                                      char output[PARTY_JOIN_CODE_SIZE])
{
    if (input == nullptr) return false;
    for (size_t index = 0; index < PARTY_JOIN_CODE_LENGTH; ++index)
    {
        char character = input[index];
        if (character >= 'a' && character <= 'z')
            character = static_cast<char>(character - 'a' + 'A');
        bool valid = false;
        for (size_t alphabet = 0; alphabet < JOIN_CODE_ALPHABET_LENGTH; ++alphabet)
            if (character == JOIN_CODE_ALPHABET[alphabet]) valid = true;
        if (!valid) return false;
        output[index] = character;
    }
    if (input[PARTY_JOIN_CODE_LENGTH] != '\0') return false;
    output[PARTY_JOIN_CODE_LENGTH] = '\0';
    return true;
}

void PartyRegistry::joinCodeFromEntropy(uint64_t entropy,
                                        char output[PARTY_JOIN_CODE_SIZE])
{
    for (size_t index = 0; index < PARTY_JOIN_CODE_LENGTH; ++index)
    {
        output[index] = JOIN_CODE_ALPHABET[entropy % JOIN_CODE_ALPHABET_LENGTH];
        entropy /= JOIN_CODE_ALPHABET_LENGTH;
    }
    output[PARTY_JOIN_CODE_LENGTH] = '\0';
}

bool PartyRegistry::partyIDExists(const PartyID& id) const
{
    for (const PartySlot& party : parties_)
        if (party.state != SlotState::Empty && sameID(party.id, id)) return true;
    return false;
}

bool PartyRegistry::joinCodeExists(const char* code) const
{
    for (const PartySlot& party : parties_)
        if (party.state == SlotState::Active && strcmp(party.joinCode, code) == 0)
            return true;
    return false;
}

PartyError PartyRegistry::generatePartyID(PartyID& id)
{
    for (size_t attempt = 0; attempt < PARTY_ID_RETRY_LIMIT; ++attempt)
    {
        if (!entropy_.next(id.high) || !entropy_.next(id.low))
            return PartyError::EntropyUnavailable;
        if ((id.high != 0 || id.low != 0) && !partyIDExists(id))
            return PartyError::Ok;
    }
    return PartyError::IdCollision;
}

PartyError PartyRegistry::generateJoinCode(char code[PARTY_JOIN_CODE_SIZE])
{
    for (size_t attempt = 0; attempt < PARTY_CODE_RETRY_LIMIT; ++attempt)
    {
        uint64_t value = 0;
        if (!entropy_.next(value)) return PartyError::EntropyUnavailable;
        joinCodeFromEntropy(value, code);
        if (!joinCodeExists(code)) return PartyError::Ok;
    }
    return PartyError::CodeCollision;
}

PartyError PartyRegistry::generateReconnectToken(ReconnectToken& token)
{
    for (size_t attempt = 0; attempt < PARTY_ID_RETRY_LIMIT; ++attempt)
    {
        if (!entropy_.next(token.high_) || !entropy_.next(token.low_))
            return PartyError::EntropyUnavailable;
        if (token.high_ != 0 || token.low_ != 0) return PartyError::Ok;
    }
    return PartyError::EntropyUnavailable;
}

void PartyRegistry::resetSlot(PartySlot& party)
{
    party.id = PartyID{};
    memset(party.joinCode, 0, sizeof(party.joinCode));
    for (MemberSlot& member : party.members) member = MemberSlot{};
    for (PartyEvent& event : party.events) event = PartyEvent{};
    party.eventStart = 0;
    party.eventCount = 0;
    party.nextSequence = 1;
    party.activity = PartyActivity::Town;
    party.locked = false;
    party.state = SlotState::Empty;
}

void PartyRegistry::makePartyHandle(size_t slot, PartyHandle& handle) const
{
    handle.owner_ = this;
    handle.slot_ = static_cast<uint8_t>(slot);
    handle.generation_ = parties_[slot].generation;
}

void PartyRegistry::makeMembership(size_t partySlot, PlayerID player,
                                   PartyMembership& membership) const
{
    const PartySlot& party = parties_[partySlot];
    const MemberSlot& member = party.members[player];
    membership = PartyMembership{};
    membership.player = player;
    membership.reconnectToken = member.reconnectToken;
    membership.handle.owner_ = this;
    membership.handle.partySlot_ = static_cast<uint8_t>(partySlot);
    membership.handle.partyGeneration_ = party.generation;
    membership.handle.player_ = player;
    membership.handle.memberGeneration_ = member.generation;
}

bool PartyRegistry::canEmit(const PartySlot& party, uint64_t count) const
{
    return count > 0 && party.nextSequence <= UINT64_MAX - count;
}

void PartyRegistry::emit(PartySlot& party, PartyEventType type, PlayerID player,
                         PartyActivity activity)
{
    const size_t index = (party.eventStart + party.eventCount) % PARTY_EVENT_CAPACITY;
    PartyEvent event;
    event.sequence = party.nextSequence++;
    event.type = type;
    event.player = player;
    event.activity = activity;
    party.events[index] = event;
    if (party.eventCount < PARTY_EVENT_CAPACITY) ++party.eventCount;
    else party.eventStart = (party.eventStart + 1) % PARTY_EVENT_CAPACITY;
}

PartyError PartyRegistry::createParty(const PartyPlayerProfile& hostProfile,
                                      PartyHandle& handle,
                                      PartyMembership& host)
{
    handle = PartyHandle{};
    host = PartyMembership{};
    if (!validName(hostProfile.displayName) || !validClass(hostProfile.characterClass) ||
        hostProfile.level == 0 || hostProfile.level > MAX_CHARACTER_LEVEL)
        return PartyError::InvalidProfile;
    size_t slot = PARTY_REGISTRY_CAPACITY;
    for (size_t index = 0; index < PARTY_REGISTRY_CAPACITY; ++index)
        if (parties_[index].state != SlotState::Active) { slot = index; break; }
    if (slot == PARTY_REGISTRY_CAPACITY) return PartyError::RegistryFull;
    if (nextPartyGeneration_ == UINT64_MAX || nextMemberGeneration_ == UINT64_MAX)
        return PartyError::Exhausted;

    PartyID id;
    PartyError error = generatePartyID(id);
    if (error != PartyError::Ok) return error;
    char code[PARTY_JOIN_CODE_SIZE] = {};
    error = generateJoinCode(code);
    if (error != PartyError::Ok) return error;
    uint64_t seedValue = 0;
    if (!entropy_.next(seedValue)) return PartyError::EntropyUnavailable;
    ReconnectToken token;
    error = generateReconnectToken(token);
    if (error != PartyError::Ok) return error;

    PartySlot& party = parties_[slot];
    resetSlot(party);
    party.generation = nextPartyGeneration_++;
    party.id = id;
    memcpy(party.joinCode, code, sizeof(code));
    if (party.gameSession.initialize(static_cast<uint32_t>(seedValue)) != SessionError::Ok)
    { resetSlot(party); return PartyError::InternalSessionError; }
    PlayerHandle sessionHandle;
    if (party.gameSession.join(hostProfile.displayName, sessionHandle) != SessionError::Ok)
    { resetSlot(party); return PartyError::InternalSessionError; }
    SessionPlayer sessionPlayer;
    if (party.gameSession.player(sessionHandle, sessionPlayer) != SessionError::Ok ||
        sessionPlayer.id != HOST_PLAYER_ID)
    {
        party.gameSession.leave(sessionHandle);
        resetSlot(party);
        return PartyError::InternalSessionError;
    }
    MemberSlot& member = party.members[HOST_PLAYER_ID];
    member.profile = hostProfile;
    member.sessionHandle = sessionHandle;
    member.reconnectToken = token;
    member.generation = nextMemberGeneration_++;
    member.location = MultiplayerMemberLocation::TOWN;
    member.occupied = true;
    member.connected = true;
    party.state = SlotState::Active;
    emit(party, PartyEventType::PartyCreated, HOST_PLAYER_ID, PartyActivity::Town);
    makePartyHandle(slot, handle);
    makeMembership(slot, HOST_PLAYER_ID, host);
    return PartyError::Ok;
}

PartyError PartyRegistry::validateParty(const PartyHandle& handle, bool allowClosed,
                                        const PartySlot*& party) const
{
    party = nullptr;
    if (handle.owner_ != this || handle.slot_ >= PARTY_REGISTRY_CAPACITY)
        return PartyError::InvalidPartyHandle;
    const PartySlot& candidate = parties_[handle.slot_];
    if (candidate.state == SlotState::Empty || candidate.generation != handle.generation_)
        return PartyError::InvalidPartyHandle;
    if (candidate.state == SlotState::Closed && !allowClosed) return PartyError::Closed;
    party = &candidate;
    return PartyError::Ok;
}

PartyError PartyRegistry::validateParty(const PartyHandle& handle, bool allowClosed,
                                        PartySlot*& party)
{
    const PartySlot* constant = nullptr;
    const PartyError error = static_cast<const PartyRegistry*>(this)->validateParty(
        handle, allowClosed, constant);
    party = const_cast<PartySlot*>(constant);
    return error;
}

PartyError PartyRegistry::validateMember(const PartyMemberHandle& handle,
                                         PartySlot*& party, MemberSlot*& member)
{
    party = nullptr;
    member = nullptr;
    if (handle.owner_ != this || handle.partySlot_ >= PARTY_REGISTRY_CAPACITY)
        return PartyError::InvalidMember;
    PartySlot& candidate = parties_[handle.partySlot_];
    if (candidate.state == SlotState::Closed &&
        candidate.generation == handle.partyGeneration_) return PartyError::Closed;
    if (candidate.state != SlotState::Active ||
        candidate.generation != handle.partyGeneration_ ||
        !isValidPlayerID(handle.player_)) return PartyError::InvalidMember;
    MemberSlot& candidateMember = candidate.members[handle.player_];
    if (!candidateMember.occupied ||
        candidateMember.generation != handle.memberGeneration_)
        return PartyError::InvalidMember;
    party = &candidate;
    member = &candidateMember;
    return PartyError::Ok;
}

PartyError PartyRegistry::findParty(const char* joinCode, PartyHandle& handle) const
{
    handle = PartyHandle{};
    char normalized[PARTY_JOIN_CODE_SIZE] = {};
    if (!normalizeJoinCode(joinCode, normalized)) return PartyError::InvalidCode;
    for (size_t index = 0; index < PARTY_REGISTRY_CAPACITY; ++index)
        if (parties_[index].state == SlotState::Active &&
            strcmp(parties_[index].joinCode, normalized) == 0)
        {
            makePartyHandle(index, handle);
            return PartyError::Ok;
        }
    return PartyError::PartyNotFound;
}

PartyError PartyRegistry::joinParty(const char* joinCode,
                                    const PartyPlayerProfile& profile,
                                    PartyMembership& membership)
{
    membership = PartyMembership{};
    if (!validName(profile.displayName) || !validClass(profile.characterClass) ||
        profile.level == 0 || profile.level > MAX_CHARACTER_LEVEL)
        return PartyError::InvalidProfile;
    PartyHandle handle;
    PartyError error = findParty(joinCode, handle);
    if (error != PartyError::Ok) return error;
    PartySlot* party = nullptr;
    error = validateParty(handle, false, party);
    if (error != PartyError::Ok) return error;
    if (party->locked) return PartyError::PartyLocked;
    if (party->activity != PartyActivity::Town) return PartyError::NotInTown;
    PlayerID player = INVALID_PLAYER_ID;
    for (PlayerID id = 0; id < MAX_MULTIPLAYER_PLAYERS; ++id)
        if (!party->members[id].occupied) { player = id; break; }
    if (!isValidPlayerID(player)) return PartyError::PartyFull;
    if (!canEmit(*party) || nextMemberGeneration_ == UINT64_MAX)
        return PartyError::Exhausted;
    ReconnectToken token;
    error = generateReconnectToken(token);
    if (error != PartyError::Ok) return error;
    PlayerHandle sessionHandle;
    if (party->gameSession.join(profile.displayName, sessionHandle) != SessionError::Ok)
        return PartyError::InternalSessionError;
    SessionPlayer sessionPlayer;
    if (party->gameSession.player(sessionHandle, sessionPlayer) != SessionError::Ok ||
        sessionPlayer.id != player)
    {
        party->gameSession.leave(sessionHandle);
        return PartyError::InternalSessionError;
    }
    MemberSlot& member = party->members[player];
    member.profile = profile;
    member.sessionHandle = sessionHandle;
    member.reconnectToken = token;
    member.generation = nextMemberGeneration_++;
    member.location = MultiplayerMemberLocation::TOWN;
    member.occupied = true;
    member.connected = true;
    emit(*party, PartyEventType::MemberJoined, player, party->activity);
    makeMembership(handle.slot_, player, membership);
    return PartyError::Ok;
}

PartyError PartyRegistry::disconnect(const PartyMemberHandle& handle)
{
    PartySlot* party = nullptr;
    MemberSlot* member = nullptr;
    PartyError error = validateMember(handle, party, member);
    if (error != PartyError::Ok) return error;
    if (!member->connected) return PartyError::Disconnected;
    if (!canEmit(*party)) return PartyError::Exhausted;
    const SessionError sessionError = party->gameSession.disconnect(member->sessionHandle);
    if (sessionError != SessionError::Ok) return PartyError::InternalSessionError;
    member->connected = false;
    member->participating = false;
    member->location = MultiplayerMemberLocation::DISCONNECTED;
    emit(*party, PartyEventType::MemberDisconnected, handle.player_, party->activity);
    return PartyError::Ok;
}

PartyError PartyRegistry::reconnect(const PartyHandle& handle, PlayerID player,
                                    const ReconnectToken& token,
                                    PartyMembership& membership)
{
    membership = PartyMembership{};
    PartySlot* party = nullptr;
    PartyError error = validateParty(handle, false, party);
    if (error != PartyError::Ok) return error;
    if (!isValidPlayerID(player) || !party->members[player].occupied)
        return PartyError::InvalidMember;
    MemberSlot& member = party->members[player];
    if (member.reconnectToken != token) return PartyError::InvalidReconnectToken;
    if (member.connected) return PartyError::AlreadyConnected;
    if (!canEmit(*party)) return PartyError::Exhausted;
    if (party->gameSession.reconnect(member.sessionHandle) != SessionError::Ok)
        return PartyError::InternalSessionError;
    member.connected = true;
    member.participating = false;
    member.location = party->activity == PartyActivity::Town
        ? MultiplayerMemberLocation::TOWN : MultiplayerMemberLocation::CONNECTING;
    emit(*party, PartyEventType::MemberReconnected, player, party->activity);
    makeMembership(handle.slot_, player, membership);
    return PartyError::Ok;
}

PartyError PartyRegistry::closeInternal(PartySlot& party, PlayerID actor)
{
    if (!canEmit(party)) return PartyError::Exhausted;
    bool sessionFailure = false;
    for (MemberSlot& member : party.members)
    {
        if (!member.occupied) continue;
        if (party.gameSession.leave(member.sessionHandle) != SessionError::Ok)
            sessionFailure = true;
        member = MemberSlot{};
    }
    emit(party, PartyEventType::PartyClosed, actor, party.activity);
    party.state = SlotState::Closed;
    party.locked = true;
    return sessionFailure ? PartyError::InternalSessionError : PartyError::Ok;
}

PartyError PartyRegistry::leave(const PartyMemberHandle& handle)
{
    PartySlot* party = nullptr;
    MemberSlot* member = nullptr;
    PartyError error = validateMember(handle, party, member);
    if (error != PartyError::Ok) return error;
    const uint64_t eventCount = handle.player_ == HOST_PLAYER_ID ? 2 : 1;
    if (!canEmit(*party, eventCount)) return PartyError::Exhausted;
    if (party->gameSession.leave(member->sessionHandle) != SessionError::Ok)
        return PartyError::InternalSessionError;
    emit(*party, PartyEventType::MemberLeft, handle.player_, party->activity);
    *member = MemberSlot{};
    if (handle.player_ == HOST_PLAYER_ID)
        return closeInternal(*party, HOST_PLAYER_ID);
    return PartyError::Ok;
}

PartyError PartyRegistry::setLocked(const PartyMemberHandle& caller, bool locked)
{
    PartySlot* party = nullptr;
    MemberSlot* member = nullptr;
    PartyError error = validateMember(caller, party, member);
    if (error != PartyError::Ok) return error;
    if (caller.player_ != HOST_PLAYER_ID) return PartyError::NotLeader;
    if (!member->connected) return PartyError::Disconnected;
    if (party->locked == locked) return PartyError::Ok;
    if (!canEmit(*party)) return PartyError::Exhausted;
    party->locked = locked;
    emit(*party, locked ? PartyEventType::PartyLocked : PartyEventType::PartyUnlocked,
         caller.player_, party->activity);
    return PartyError::Ok;
}

PartyError PartyRegistry::startActivity(const PartyMemberHandle& caller,
                                        PartyActivity activity)
{
    PartySlot* party = nullptr;
    MemberSlot* member = nullptr;
    PartyError error = validateMember(caller, party, member);
    if (error != PartyError::Ok) return error;
    if (caller.player_ != HOST_PLAYER_ID) return PartyError::NotLeader;
    if (!member->connected) return PartyError::Disconnected;
    if (party->activity != PartyActivity::Town) return PartyError::AlreadyInActivity;
    if (!party->gameSession.initialized()) return PartyError::InternalSessionError;
    if (party->gameSession.connectedCount() == 0) return PartyError::Disconnected;
    if (!canEmit(*party, 2)) return PartyError::Exhausted;
    emit(*party, PartyEventType::ActivityStarting, caller.player_, activity);
    for (MemberSlot& slot : party->members)
        if (slot.occupied && slot.connected)
        {
            slot.location = MultiplayerMemberLocation::CONNECTING;
            slot.participating = true;
        }
    party->activity = activity;
    const MultiplayerMemberLocation location = activity == PartyActivity::Forest
        ? MultiplayerMemberLocation::FOREST : MultiplayerMemberLocation::DUNGEON;
    for (MemberSlot& slot : party->members)
        if (slot.occupied && slot.connected) slot.location = location;
    emit(*party, PartyEventType::ActivityStarted, caller.player_, activity);
    return PartyError::Ok;
}

PartyError PartyRegistry::startForest(const PartyMemberHandle& caller)
{
    return startActivity(caller, PartyActivity::Forest);
}

PartyError PartyRegistry::startDungeon(const PartyMemberHandle& caller)
{
    return startActivity(caller, PartyActivity::Dungeon);
}

PartyError PartyRegistry::closeParty(const PartyMemberHandle& caller)
{
    PartySlot* party = nullptr;
    MemberSlot* member = nullptr;
    PartyError error = validateMember(caller, party, member);
    if (error != PartyError::Ok) return error;
    if (caller.player_ != HOST_PLAYER_ID) return PartyError::NotLeader;
    if (!member->connected) return PartyError::Disconnected;
    return closeInternal(*party, caller.player_);
}

PartyError PartyRegistry::snapshot(const PartyHandle& handle,
                                   PartySnapshot& result) const
{
    result = PartySnapshot{};
    const PartySlot* party = nullptr;
    const PartyError error = validateParty(handle, false, party);
    if (error != PartyError::Ok) return error;
    result.id = party->id;
    memcpy(result.joinCode, party->joinCode, sizeof(result.joinCode));
    result.activity = party->activity;
    result.locked = party->locked;
    for (PlayerID player = 0; player < MAX_MULTIPLAYER_PLAYERS; ++player)
    {
        const MemberSlot& member = party->members[player];
        PartyMemberSnapshot& snapshot = result.members[player];
        if (!member.occupied) continue;
        snapshot.occupied = true;
        snapshot.connected = member.connected;
        snapshot.leader = player == HOST_PLAYER_ID;
        snapshot.participating = member.participating;
        snapshot.player = player;
        snapshot.profile = member.profile;
        snapshot.location = member.location;
        ++result.memberCount;
        if (member.connected) ++result.connectedCount;
    }
    result.openSlotCount = static_cast<uint8_t>(MAX_MULTIPLAYER_PLAYERS - result.memberCount);
    return PartyError::Ok;
}

PartyError PartyRegistry::snapshot(const char* joinCode, PartySnapshot& result) const
{
    PartyHandle handle;
    const PartyError error = findParty(joinCode, handle);
    return error == PartyError::Ok ? snapshot(handle, result) : error;
}

PartyError PartyRegistry::list(PartyList& result) const
{
    result = PartyList{};
    for (size_t index = 0; index < PARTY_REGISTRY_CAPACITY; ++index)
    {
        if (parties_[index].state != SlotState::Active) continue;
        PartyHandle handle;
        makePartyHandle(index, handle);
        const PartyError error = snapshot(handle, result.parties[result.count]);
        if (error != PartyError::Ok) return error;
        ++result.count;
    }
    return PartyError::Ok;
}

PartyError PartyRegistry::eventsSince(const PartyHandle& handle, uint64_t sequence,
                                      PartyEvents& result) const
{
    result = PartyEvents{};
    const PartySlot* party = nullptr;
    const PartyError error = validateParty(handle, true, party);
    if (error != PartyError::Ok) return error;
    result.newest = party->nextSequence - 1;
    if (sequence > result.newest) return PartyError::InvalidCursor;
    if (party->eventCount == 0) return PartyError::Ok;
    result.oldest = party->events[party->eventStart].sequence;
    result.gap = sequence + 1 < result.oldest;
    for (size_t index = 0; index < party->eventCount; ++index)
    {
        const PartyEvent& event = party->events[
            (party->eventStart + index) % PARTY_EVENT_CAPACITY];
        if (event.sequence > sequence) result.events[result.count++] = event;
    }
    return result.gap ? PartyError::HistoryGap : PartyError::Ok;
}

const GameSession* PartyRegistry::gameSession(const PartyHandle& handle) const
{
    const PartySlot* party = nullptr;
    return validateParty(handle, false, party) == PartyError::Ok
        ? &party->gameSession : nullptr;
}

size_t PartyRegistry::partyCount() const
{
    size_t count = 0;
    for (const PartySlot& party : parties_)
        if (party.state == SlotState::Active) ++count;
    return count;
}
}
