#ifndef PFME_PARTY_REGISTRY_H
#define PFME_PARTY_REGISTRY_H

#include <stddef.h>
#include <stdint.h>

#include "characters/character_types.h"
#include "session/game_session.h"

namespace pfme
{
constexpr size_t PARTY_REGISTRY_CAPACITY = 16;
constexpr size_t PARTY_JOIN_CODE_LENGTH = 5;
constexpr size_t PARTY_JOIN_CODE_SIZE = PARTY_JOIN_CODE_LENGTH + 1;
constexpr size_t PARTY_EVENT_CAPACITY = 32;
constexpr size_t PARTY_CODE_RETRY_LIMIT = 32;
constexpr size_t PARTY_ID_RETRY_LIMIT = 32;

enum class PartyError : uint8_t
{
    Ok,
    RegistryFull,
    PartyNotFound,
    PartyFull,
    PartyLocked,
    InvalidCode,
    InvalidProfile,
    InvalidPartyHandle,
    InvalidMember,
    NotLeader,
    AlreadyInActivity,
    NotInTown,
    InvalidReconnectToken,
    Disconnected,
    AlreadyConnected,
    Closed,
    CodeCollision,
    IdCollision,
    EntropyUnavailable,
    HistoryGap,
    InvalidCursor,
    Exhausted,
    InternalSessionError
};
const char* partyErrorName(PartyError error);

enum class PartyActivity : uint8_t
{
    Town,
    Forest,
    Dungeon
};
const char* partyActivityName(PartyActivity activity);
const char* partyLocationName(MultiplayerMemberLocation location);
const char* partyCharacterClassName(CharacterClass characterClass);

enum class PartyEventType : uint8_t
{
    PartyCreated,
    MemberJoined,
    MemberDisconnected,
    MemberReconnected,
    MemberLeft,
    PartyLocked,
    PartyUnlocked,
    ActivityStarting,
    ActivityStarted,
    PartyClosed
};
const char* partyEventName(PartyEventType type);

struct PartyID
{
    uint64_t high = 0;
    uint64_t low = 0;
};

struct PartyPlayerProfile
{
    char displayName[NETWORK_PLAYER_NAME_SIZE] = {};
    CharacterClass characterClass = CLASS_FIGHTER;
    uint8_t level = 1;
};

class PartyRegistry;

class PartyHandle
{
public:
    PartyHandle() = default;
private:
    friend class PartyRegistry;
    const PartyRegistry* owner_ = nullptr;
    uint8_t slot_ = UINT8_MAX;
    uint64_t generation_ = 0;
};

class PartyMemberHandle
{
public:
    PartyMemberHandle() = default;
private:
    friend class PartyRegistry;
    const PartyRegistry* owner_ = nullptr;
    uint8_t partySlot_ = UINT8_MAX;
    uint64_t partyGeneration_ = 0;
    PlayerID player_ = INVALID_PLAYER_ID;
    uint64_t memberGeneration_ = 0;
};

class ReconnectToken
{
public:
    ReconnectToken() = default;
    bool operator==(const ReconnectToken& other) const;
    bool operator!=(const ReconnectToken& other) const { return !(*this == other); }
private:
    friend class PartyRegistry;
    uint64_t high_ = 0;
    uint64_t low_ = 0;
};

struct PartyMembership
{
    PartyMemberHandle handle{};
    ReconnectToken reconnectToken{};
    PlayerID player = INVALID_PLAYER_ID;
};

struct PartyMemberSnapshot
{
    bool occupied = false;
    bool connected = false;
    bool leader = false;
    bool participating = false;
    PlayerID player = INVALID_PLAYER_ID;
    PartyPlayerProfile profile{};
    MultiplayerMemberLocation location = MultiplayerMemberLocation::DISCONNECTED;
};

struct PartySnapshot
{
    PartyID id{};
    char joinCode[PARTY_JOIN_CODE_SIZE] = {};
    PlayerID leader = HOST_PLAYER_ID;
    PartyActivity activity = PartyActivity::Town;
    PartyMemberSnapshot members[MAX_MULTIPLAYER_PLAYERS] = {};
    uint8_t memberCount = 0;
    uint8_t connectedCount = 0;
    uint8_t openSlotCount = MAX_MULTIPLAYER_PLAYERS;
    bool locked = false;
};

struct PartyList
{
    PartySnapshot parties[PARTY_REGISTRY_CAPACITY] = {};
    size_t count = 0;
};

struct PartyEvent
{
    uint64_t sequence = 0;
    PartyEventType type = PartyEventType::PartyCreated;
    PlayerID player = INVALID_PLAYER_ID;
    PartyActivity activity = PartyActivity::Town;
};

struct PartyEvents
{
    PartyEvent events[PARTY_EVENT_CAPACITY] = {};
    size_t count = 0;
    uint64_t oldest = 0;
    uint64_t newest = 0;
    bool gap = false;
};

class PartyEntropy
{
public:
    virtual ~PartyEntropy() {}
    virtual bool next(uint64_t& value) = 0;
};

// Deterministic entropy for tests and reproducible local tools. Production
// adapters must supply an operating-system-backed PartyEntropy implementation.
class SeededPartyEntropy : public PartyEntropy
{
public:
    explicit SeededPartyEntropy(uint64_t seed);
    bool next(uint64_t& value) override;
private:
    uint64_t state_;
};

class PartyRegistry
{
public:
    explicit PartyRegistry(PartyEntropy& entropy);
    PartyRegistry(const PartyRegistry&) = delete;
    PartyRegistry& operator=(const PartyRegistry&) = delete;

    PartyError createParty(const PartyPlayerProfile& hostProfile,
                           PartyHandle& party, PartyMembership& host);
    PartyError findParty(const char* joinCode, PartyHandle& party) const;
    PartyError joinParty(const char* joinCode, const PartyPlayerProfile& profile,
                         PartyMembership& member);
    PartyError disconnect(const PartyMemberHandle& member);
    PartyError reconnect(const PartyHandle& party, PlayerID player,
                         const ReconnectToken& token, PartyMembership& member);
    PartyError leave(const PartyMemberHandle& member);
    PartyError setLocked(const PartyMemberHandle& caller, bool locked);
    PartyError startForest(const PartyMemberHandle& caller);
    PartyError startDungeon(const PartyMemberHandle& caller);
    PartyError closeParty(const PartyMemberHandle& caller);
    // Authoritative runtime observations, never deserialized browser claims.
    PartyError updateMember(const PartyMemberHandle&,const PartyPlayerProfile&,MultiplayerMemberLocation);
    PartyError returnToTown(const PartyMemberHandle&);

    PartyError snapshot(const PartyHandle& party, PartySnapshot& result) const;
    PartyError snapshot(const char* joinCode, PartySnapshot& result) const;
    PartyError list(PartyList& result) const;
    PartyError eventsSince(const PartyHandle& party, uint64_t sequence,
                           PartyEvents& result) const;
    const GameSession* gameSession(const PartyHandle& party) const;
    size_t partyCount() const;

    static bool normalizeJoinCode(const char* input,
                                  char output[PARTY_JOIN_CODE_SIZE]);
    static void joinCodeFromEntropy(uint64_t entropy,
                                    char output[PARTY_JOIN_CODE_SIZE]);

    // Player 0 is a social leader only. This layer never grants gameplay
    // authority: the NUC-owned GameSession remains authoritative for rules,
    // movement, RNG, combat, loot, monsters, and dungeon state.

private:
    enum class SlotState : uint8_t { Empty, Active, Closed };
    struct MemberSlot
    {
        PartyPlayerProfile profile{};
        PlayerHandle sessionHandle{};
        ReconnectToken reconnectToken{};
        uint64_t generation = 0;
        MultiplayerMemberLocation location = MultiplayerMemberLocation::DISCONNECTED;
        bool occupied = false;
        bool connected = false;
        bool participating = false;
    };
    struct PartySlot
    {
        GameSession gameSession{};
        PartyID id{};
        char joinCode[PARTY_JOIN_CODE_SIZE] = {};
        MemberSlot members[MAX_MULTIPLAYER_PLAYERS] = {};
        PartyEvent events[PARTY_EVENT_CAPACITY] = {};
        size_t eventStart = 0;
        size_t eventCount = 0;
        uint64_t nextSequence = 1;
        uint64_t generation = 0;
        PartyActivity activity = PartyActivity::Town;
        SlotState state = SlotState::Empty;
        bool locked = false;
    };

    PartyError validateParty(const PartyHandle& handle, bool allowClosed,
                             const PartySlot*& party) const;
    PartyError validateParty(const PartyHandle& handle, bool allowClosed,
                             PartySlot*& party);
    PartyError validateMember(const PartyMemberHandle& handle,
                              PartySlot*& party, MemberSlot*& member);
    PartyError startActivity(const PartyMemberHandle& caller,
                             PartyActivity activity);
    PartyError closeInternal(PartySlot& party, PlayerID actor);
    PartyError generatePartyID(PartyID& id);
    PartyError generateJoinCode(char code[PARTY_JOIN_CODE_SIZE]);
    PartyError generateReconnectToken(ReconnectToken& token);
    bool partyIDExists(const PartyID& id) const;
    bool joinCodeExists(const char* code) const;
    void resetSlot(PartySlot& party);
    void makePartyHandle(size_t slot, PartyHandle& handle) const;
    void makeMembership(size_t partySlot, PlayerID player,
                        PartyMembership& membership) const;
    bool canEmit(const PartySlot& party, uint64_t count = 1) const;
    void emit(PartySlot& party, PartyEventType type, PlayerID player,
              PartyActivity activity);

    PartyEntropy& entropy_;
    PartySlot parties_[PARTY_REGISTRY_CAPACITY] = {};
    uint64_t nextPartyGeneration_ = 1;
    uint64_t nextMemberGeneration_ = 1;
};
}

#endif
