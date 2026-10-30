#include <cstring>
#include <iostream>
#include <string>

#include "session/party_registry.h"

namespace
{
using namespace pfme;
#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << __LINE__ << ": " << #expression << '\n'; return false; } } while (false)

PartyPlayerProfile profile(const char* name, CharacterClass characterClass = CLASS_FIGHTER,
                           uint8_t level = 5)
{
    PartyPlayerProfile result;
    std::strncpy(result.displayName, name, sizeof(result.displayName) - 1);
    result.characterClass = characterClass;
    result.level = level;
    return result;
}

class ScriptEntropy : public PartyEntropy
{
public:
    ScriptEntropy(const uint64_t* values, size_t count) : values_(values), count_(count) {}
    bool next(uint64_t& value) override
    {
        if (position_ >= count_) return false;
        value = values_[position_++];
        return true;
    }
private:
    const uint64_t* values_;
    size_t count_;
    size_t position_ = 0;
};

bool creationAndJoining()
{
    SeededPartyEntropy entropy(1001);
    PartyRegistry registry(entropy);
    PartyHandle party;
    PartyMembership host;
    CHECK(registry.createParty(profile("Alice", CLASS_FIGHTER, 6), party, host) == PartyError::Ok);
    CHECK(host.player == HOST_PLAYER_ID);
    PartySnapshot snapshot;
    CHECK(registry.snapshot(party, snapshot) == PartyError::Ok);
    CHECK(snapshot.memberCount == 1 && snapshot.connectedCount == 1);
    CHECK(snapshot.openSlotCount == 3 && !snapshot.locked);
    CHECK(snapshot.activity == PartyActivity::Town);
    CHECK(snapshot.members[0].leader && snapshot.members[0].connected);
    CHECK(snapshot.members[0].location == MultiplayerMemberLocation::TOWN);
    CHECK(std::strcmp(snapshot.members[0].profile.displayName, "Alice") == 0);
    CHECK(std::strlen(snapshot.joinCode) == PARTY_JOIN_CODE_LENGTH);
    const std::string alphabet = "23456789ABCDEFGHJKMNPQRSTUVWXYZ";
    for (size_t index = 0; index < PARTY_JOIN_CODE_LENGTH; ++index)
        CHECK(alphabet.find(snapshot.joinCode[index]) != std::string::npos);
    CHECK(std::strchr(snapshot.joinCode, '0') == nullptr);
    CHECK(std::strchr(snapshot.joinCode, 'O') == nullptr);
    CHECK(std::strchr(snapshot.joinCode, '1') == nullptr);
    CHECK(std::strchr(snapshot.joinCode, 'I') == nullptr);
    CHECK(std::strchr(snapshot.joinCode, 'L') == nullptr);
    const GameSession* game = registry.gameSession(party);
    CHECK(game != nullptr && game->initialized());
    CHECK(game->playerCount() == 1 && game->connectedCount() == 1);

    char lowercase[PARTY_JOIN_CODE_SIZE] = {};
    for (size_t index = 0; index < PARTY_JOIN_CODE_LENGTH; ++index)
    {
        const char value = snapshot.joinCode[index];
        lowercase[index] = value >= 'A' && value <= 'Z'
            ? static_cast<char>(value - 'A' + 'a') : value;
    }
    PartyMembership members[4];
    CHECK(registry.joinParty(lowercase, profile("Bob", CLASS_CLERIC), members[1]) == PartyError::Ok);
    CHECK(registry.joinParty(snapshot.joinCode, profile("Carol", CLASS_ROGUE), members[2]) == PartyError::Ok);
    CHECK(registry.joinParty(snapshot.joinCode, profile("Dave", CLASS_WIZARD), members[3]) == PartyError::Ok);
    CHECK(members[1].player == 1 && members[2].player == 2 && members[3].player == 3);
    CHECK(registry.joinParty(snapshot.joinCode, profile("Extra"), members[0]) == PartyError::PartyFull);
    CHECK(registry.snapshot(party, snapshot) == PartyError::Ok);
    CHECK(snapshot.memberCount == 4 && snapshot.openSlotCount == 0);
    CHECK(game->playerCount() == 4);
    for (PlayerID player = 0; player < MAX_MULTIPLAYER_PLAYERS; ++player)
    {
        SessionPlayer sessionPlayer;
        CHECK(game->player(player, sessionPlayer) == SessionError::Ok);
        CHECK(sessionPlayer.id == player);
        CHECK(std::strcmp(sessionPlayer.name, snapshot.members[player].profile.displayName) == 0);
    }
    return true;
}

bool codeValidationAndCollision()
{
    char code[PARTY_JOIN_CODE_SIZE] = {};
    PartyRegistry::joinCodeFromEntropy(0, code);
    CHECK(std::strcmp(code, "22222") == 0);
    char normalized[PARTY_JOIN_CODE_SIZE] = {};
    CHECK(PartyRegistry::normalizeJoinCode("abcde", normalized));
    CHECK(std::strcmp(normalized, "ABCDE") == 0);
    CHECK(!PartyRegistry::normalizeJoinCode("ABCD", normalized));
    CHECK(!PartyRegistry::normalizeJoinCode("ABCDEF", normalized));
    CHECK(!PartyRegistry::normalizeJoinCode("ABO23", normalized));
    CHECK(!PartyRegistry::normalizeJoinCode("AB123", normalized));
    CHECK(!PartyRegistry::normalizeJoinCode(nullptr, normalized));

    const uint64_t rerollValues[] = {
        1, 2, 0, 3, 4, 5,
        6, 7, 0, 1, 8, 9, 10
    };
    ScriptEntropy rerollEntropy(rerollValues,
        sizeof(rerollValues) / sizeof(rerollValues[0]));
    PartyRegistry rerollRegistry(rerollEntropy);
    PartyHandle first, second;
    PartyMembership firstHost, secondHost;
    CHECK(rerollRegistry.createParty(profile("First"), first, firstHost) == PartyError::Ok);
    CHECK(rerollRegistry.createParty(profile("Second"), second, secondHost) == PartyError::Ok);
    PartySnapshot firstSnapshot, secondSnapshot;
    CHECK(rerollRegistry.snapshot(first, firstSnapshot) == PartyError::Ok);
    CHECK(rerollRegistry.snapshot(second, secondSnapshot) == PartyError::Ok);
    CHECK(std::strcmp(firstSnapshot.joinCode, "22222") == 0);
    CHECK(std::strcmp(secondSnapshot.joinCode, "32222") == 0);

    uint64_t collisionValues[6 + 2 + PARTY_CODE_RETRY_LIMIT] = {};
    collisionValues[0] = 1; collisionValues[1] = 2;
    collisionValues[2] = 0; collisionValues[3] = 3;
    collisionValues[4] = 4; collisionValues[5] = 5;
    collisionValues[6] = 6; collisionValues[7] = 7;
    ScriptEntropy collisionEntropy(collisionValues,
        sizeof(collisionValues) / sizeof(collisionValues[0]));
    PartyRegistry collisionRegistry(collisionEntropy);
    CHECK(collisionRegistry.createParty(profile("First"), first, firstHost) == PartyError::Ok);
    CHECK(collisionRegistry.createParty(profile("Second"), second, secondHost) == PartyError::CodeCollision);
    CHECK(collisionRegistry.partyCount() == 1);

    SeededPartyEntropy validationEntropy(9);
    PartyRegistry validationRegistry(validationEntropy);
    PartyHandle found;
    CHECK(validationRegistry.findParty("bad", found) == PartyError::InvalidCode);
    CHECK(validationRegistry.findParty("OOOOO", found) == PartyError::InvalidCode);
    CHECK(validationRegistry.findParty("ABCDE", found) == PartyError::PartyNotFound);
    PartyMembership ignored;
    CHECK(validationRegistry.joinParty("bad", profile("A"), ignored) == PartyError::InvalidCode);
    CHECK(validationRegistry.createParty(profile(""), found, ignored) == PartyError::InvalidProfile);
    CHECK(validationRegistry.createParty(profile("Valid", static_cast<CharacterClass>(99)), found, ignored) == PartyError::InvalidProfile);
    CHECK(validationRegistry.createParty(profile("Valid", CLASS_FIGHTER, 0), found, ignored) == PartyError::InvalidProfile);
    return true;
}

bool lockingDisconnectReconnectAndLeave()
{
    SeededPartyEntropy entropy(2002);
    PartyRegistry registry(entropy);
    PartyHandle party;
    PartyMembership host, bob, carol, dave;
    CHECK(registry.createParty(profile("Host"), party, host) == PartyError::Ok);
    PartySnapshot snapshot;
    CHECK(registry.snapshot(party, snapshot) == PartyError::Ok);
    char code[PARTY_JOIN_CODE_SIZE] = {};
    std::strcpy(code, snapshot.joinCode);
    CHECK(registry.joinParty(code, profile("Bob"), bob) == PartyError::Ok);
    CHECK(registry.setLocked(bob.handle, true) == PartyError::NotLeader);
    CHECK(registry.setLocked(host.handle, true) == PartyError::Ok);
    CHECK(registry.joinParty(code, profile("Carol"), carol) == PartyError::PartyLocked);
    CHECK(registry.setLocked(host.handle, false) == PartyError::Ok);
    CHECK(registry.joinParty(code, profile("Carol"), carol) == PartyError::Ok);
    CHECK(registry.disconnect(bob.handle) == PartyError::Ok);
    CHECK(registry.snapshot(party, snapshot) == PartyError::Ok);
    CHECK(snapshot.memberCount == 3 && snapshot.connectedCount == 2);
    CHECK(snapshot.members[1].occupied && !snapshot.members[1].connected);
    CHECK(registry.joinParty(code, profile("Dave"), dave) == PartyError::Ok);
    CHECK(dave.player == 3);
    PartyMembership restored;
    CHECK(registry.reconnect(party, bob.player, carol.reconnectToken, restored) ==
          PartyError::InvalidReconnectToken);
    CHECK(registry.reconnect(party, bob.player, bob.reconnectToken, restored) == PartyError::Ok);
    CHECK(restored.player == bob.player);
    CHECK(registry.snapshot(party, snapshot) == PartyError::Ok && snapshot.connectedCount == 4);
    CHECK(registry.disconnect(restored.handle) == PartyError::Ok);
    const ReconnectToken oldBobToken = restored.reconnectToken;
    CHECK(registry.leave(restored.handle) == PartyError::Ok);
    CHECK(registry.reconnect(party, 1, oldBobToken, restored) == PartyError::InvalidMember);
    PartyMembership replacement;
    CHECK(registry.joinParty(code, profile("Erin"), replacement) == PartyError::Ok);
    CHECK(replacement.player == 1);
    CHECK(registry.leave(host.handle) == PartyError::Ok);
    CHECK(registry.snapshot(code, snapshot) == PartyError::PartyNotFound);
    CHECK(registry.reconnect(party, replacement.player, replacement.reconnectToken, restored) == PartyError::Closed);
    CHECK(registry.disconnect(replacement.handle) == PartyError::Closed);
    return true;
}

bool activityForEveryPartySize()
{
    SeededPartyEntropy entropy(3003);
    PartyRegistry registry(entropy);
    for (uint8_t size = 1; size <= MAX_MULTIPLAYER_PLAYERS; ++size)
    {
        PartyHandle party;
        PartyMembership members[MAX_MULTIPLAYER_PLAYERS];
        CHECK(registry.createParty(profile("Leader"), party, members[0]) == PartyError::Ok);
        PartySnapshot snapshot;
        CHECK(registry.snapshot(party, snapshot) == PartyError::Ok);
        for (uint8_t player = 1; player < size; ++player)
        {
            char name[8] = {};
            name[0] = 'P'; name[1] = static_cast<char>('0' + player); name[2] = '\0';
            CHECK(registry.joinParty(snapshot.joinCode, profile(name), members[player]) == PartyError::Ok);
        }
        if (size == 2)
        {
            CHECK(registry.startForest(members[1].handle) == PartyError::NotLeader);
            CHECK(registry.disconnect(members[1].handle) == PartyError::Ok);
        }
        const PartyActivity expected = size % 2 == 0
            ? PartyActivity::Dungeon : PartyActivity::Forest;
        const PartyError start = expected == PartyActivity::Forest
            ? registry.startForest(members[0].handle)
            : registry.startDungeon(members[0].handle);
        CHECK(start == PartyError::Ok);
        CHECK(registry.snapshot(party, snapshot) == PartyError::Ok);
        CHECK(snapshot.activity == expected);
        for (PlayerID player = 0; player < size; ++player)
        {
            const bool connected = !(size == 2 && player == 1);
            CHECK(snapshot.members[player].connected == connected);
            CHECK(snapshot.members[player].participating == connected);
            if (connected)
                CHECK(snapshot.members[player].location ==
                    (expected == PartyActivity::Forest
                        ? MultiplayerMemberLocation::FOREST
                        : MultiplayerMemberLocation::DUNGEON));
            else CHECK(snapshot.members[player].location == MultiplayerMemberLocation::DISCONNECTED);
        }
        CHECK(registry.startForest(members[0].handle) == PartyError::AlreadyInActivity);
        CHECK(registry.joinParty(snapshot.joinCode, profile("Late"), members[0]) == PartyError::NotInTown);
    }
    return true;
}

bool eventsAndMultipleParties()
{
    SeededPartyEntropy entropy(4004);
    PartyRegistry registry(entropy);
    PartyHandle first, second;
    PartyMembership firstHost, secondHost, member;
    CHECK(registry.createParty(profile("One"), first, firstHost) == PartyError::Ok);
    CHECK(registry.createParty(profile("Two"), second, secondHost) == PartyError::Ok);
    PartySnapshot firstSnapshot, secondSnapshot;
    CHECK(registry.snapshot(first, firstSnapshot) == PartyError::Ok);
    CHECK(registry.snapshot(second, secondSnapshot) == PartyError::Ok);
    CHECK(firstSnapshot.id.high != secondSnapshot.id.high || firstSnapshot.id.low != secondSnapshot.id.low);
    CHECK(std::strcmp(firstSnapshot.joinCode, secondSnapshot.joinCode) != 0);
    CHECK(registry.gameSession(first) != registry.gameSession(second));
    CHECK(registry.joinParty(firstSnapshot.joinCode, profile("Member"), member) == PartyError::Ok);
    CHECK(registry.disconnect(member.handle) == PartyError::Ok);
    PartyMembership restored;
    CHECK(registry.reconnect(first, member.player, member.reconnectToken, restored) == PartyError::Ok);
    CHECK(registry.setLocked(firstHost.handle, true) == PartyError::Ok);
    CHECK(registry.setLocked(firstHost.handle, false) == PartyError::Ok);
    CHECK(registry.leave(restored.handle) == PartyError::Ok);
    CHECK(registry.startForest(firstHost.handle) == PartyError::Ok);
    CHECK(registry.snapshot(second, secondSnapshot) == PartyError::Ok);
    CHECK(secondSnapshot.activity == PartyActivity::Town && secondSnapshot.memberCount == 1);
    CHECK(registry.closeParty(firstHost.handle) == PartyError::Ok);
    PartyEvents events;
    CHECK(registry.eventsSince(first, 0, events) == PartyError::Ok);
    const PartyEventType expected[] = {
        PartyEventType::PartyCreated, PartyEventType::MemberJoined,
        PartyEventType::MemberDisconnected, PartyEventType::MemberReconnected,
        PartyEventType::PartyLocked, PartyEventType::PartyUnlocked,
        PartyEventType::MemberLeft, PartyEventType::ActivityStarting,
        PartyEventType::ActivityStarted, PartyEventType::PartyClosed
    };
    CHECK(events.count == sizeof(expected) / sizeof(expected[0]));
    for (size_t index = 0; index < events.count; ++index)
    {
        CHECK(events.events[index].sequence == index + 1);
        CHECK(events.events[index].type == expected[index]);
    }

    PartyHandle bounded;
    PartyMembership boundedHost;
    CHECK(registry.createParty(profile("Bounded"), bounded, boundedHost) == PartyError::Ok);
    for (size_t index = 0; index < PARTY_EVENT_CAPACITY; ++index)
    {
        CHECK(registry.setLocked(boundedHost.handle, true) == PartyError::Ok);
        CHECK(registry.setLocked(boundedHost.handle, false) == PartyError::Ok);
    }
    CHECK(registry.eventsSince(bounded, 0, events) == PartyError::HistoryGap);
    CHECK(events.gap && events.count == PARTY_EVENT_CAPACITY);
    for (size_t index = 1; index < events.count; ++index)
        CHECK(events.events[index].sequence == events.events[index - 1].sequence + 1);
    CHECK(registry.eventsSince(bounded, events.oldest - 1, events) == PartyError::Ok);
    CHECK(!events.gap && events.count == PARTY_EVENT_CAPACITY);
    CHECK(registry.eventsSince(bounded, events.newest + 1, events) == PartyError::InvalidCursor);
    PartyList list;
    CHECK(registry.list(list) == PartyError::Ok);
    CHECK(list.count == registry.partyCount());
    return true;
}

bool registryCapacity()
{
    SeededPartyEntropy entropy(5005);
    PartyRegistry registry(entropy);
    PartyHandle parties[PARTY_REGISTRY_CAPACITY];
    PartyMembership hosts[PARTY_REGISTRY_CAPACITY];
    for (size_t index = 0; index < PARTY_REGISTRY_CAPACITY; ++index)
    {
        char name[NETWORK_PLAYER_NAME_SIZE] = {};
        name[0] = 'H';
        name[1] = static_cast<char>('A' + index);
        CHECK(registry.createParty(profile(name), parties[index], hosts[index]) == PartyError::Ok);
    }
    PartyHandle extraParty;
    PartyMembership extraHost;
    CHECK(registry.createParty(profile("Extra"), extraParty, extraHost) == PartyError::RegistryFull);
    CHECK(registry.partyCount() == PARTY_REGISTRY_CAPACITY);
    CHECK(registry.closeParty(hosts[3].handle) == PartyError::Ok);
    CHECK(registry.createParty(profile("Reuse"), extraParty, extraHost) == PartyError::Ok);
    PartySnapshot staleSnapshot;
    CHECK(registry.snapshot(parties[3], staleSnapshot) == PartyError::InvalidPartyHandle);
    return true;
}
}

int main()
{
    if (!creationAndJoining() || !codeValidationAndCollision() ||
        !lockingDisconnectReconnectAndLeave() || !activityForEveryPartySize() ||
        !eventsAndMultipleParties() || !registryCapacity()) return 1;
    std::cout << "PartyRegistry checks passed\n";
    return 0;
}
