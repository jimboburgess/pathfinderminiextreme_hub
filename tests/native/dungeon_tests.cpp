#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "dungeon/dungeontopology.h"

namespace
{
struct Options
{
    uint32_t seed = 1;
    uint32_t count = 1;
    bool json = false;
    bool selfTest = false;
};

struct Failure
{
    uint32_t seed;
    std::string reason;
};

struct SimulationStats
{
    uint32_t requested = 0;
    uint32_t passed = 0;
    uint32_t failed = 0;
    uint32_t loopDungeons = 0;
    uint64_t roomTotal = 0;
    uint64_t bossDistanceTotal = 0;
    uint64_t branchTotal = 0;
    uint64_t loopTotal = 0;
    uint64_t puzzleTotal = 0;
    uint64_t attemptTotal = 0;
    uint64_t rejectedLayouts = 0;
    uint8_t minRooms = MAX_DUNGEON_ROOMS;
    uint8_t maxRooms = 0;
    uint8_t minBossDistance = NO_ROOM;
    uint8_t maxBossDistance = 0;
    uint8_t minBranches = MAX_ROOMS;
    uint8_t maxBranches = 0;
    uint8_t maxAttempts = 0;
    std::vector<Failure> failures;
};

void printUsage(const char* executable)
{
    std::cout
        << "Usage: " << executable
        << " [--seed N] [--count N] [--json] [--self-test]\n"
        << "  --seed N      First deterministic 32-bit seed (default: 1)\n"
        << "  --count N     Number of sequential seeds to generate (default: 1)\n"
        << "  --json        Print a machine-readable summary\n"
        << "  --self-test   Run fixed-seed and boundary regression tests\n";
}

bool parseUint32(const char* value, uint32_t& result)
{
    if (value == nullptr || *value == '\0' || *value == '-')
        return false;
    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' ||
        parsed > std::numeric_limits<uint32_t>::max())
        return false;
    result = static_cast<uint32_t>(parsed);
    return true;
}

bool parseOptions(int argc, char** argv, Options& options)
{
    for (int index = 1; index < argc; index++)
    {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h")
        {
            printUsage(argv[0]);
            std::exit(0);
        }
        if (argument == "--json")
        {
            options.json = true;
            continue;
        }
        if (argument == "--self-test")
        {
            options.selfTest = true;
            continue;
        }
        if ((argument == "--seed" || argument == "--count") &&
            index + 1 < argc)
        {
            uint32_t parsed = 0;
            if (!parseUint32(argv[++index], parsed))
                return false;
            if (argument == "--seed")
                options.seed = parsed;
            else
                options.count = parsed;
            continue;
        }
        return false;
    }
    return options.count > 0;
}

void hashByte(uint64_t& hash, uint8_t value)
{
    hash ^= value;
    hash *= 1099511628211ULL;
}

uint64_t topologyFingerprint(
    const DungeonTopology& dungeon,
    const DungeonGenerationReport& report)
{
    uint64_t hash = 14695981039346656037ULL;
    hashByte(hash, dungeon.roomCount);
    hashByte(hash, dungeon.bossRoom);
    hashByte(hash, dungeon.treasureRoom);
    hashByte(hash, dungeon.riddleRoom);
    hashByte(hash, report.attempts);
    hashByte(hash, static_cast<uint8_t>(report.riddleExitDirection));
    for (uint8_t roomIndex = 0;
         roomIndex < dungeon.roomCount;
         roomIndex++)
    {
        const DungeonTopologyRoom& room = dungeon.rooms[roomIndex];
        hashByte(hash, static_cast<uint8_t>(room.type));
        hashByte(hash, room.north);
        hashByte(hash, room.east);
        hashByte(hash, room.south);
        hashByte(hash, room.west);
        hashByte(hash, static_cast<uint8_t>(room.dungeonX));
        hashByte(hash, static_cast<uint8_t>(room.dungeonY));
    }
    return hash;
}

uint8_t countBranches(const DungeonTopology& dungeon)
{
    uint8_t count = 0;
    for (uint8_t roomIndex = 0;
         roomIndex < dungeon.roomCount;
         roomIndex++)
        count += getRoomDegree(dungeon.rooms[roomIndex]) >= 3;
    return count;
}

uint8_t countIndependentLoops(const DungeonTopology& dungeon)
{
    uint8_t degreeTotal = 0;
    for (uint8_t roomIndex = 0;
         roomIndex < dungeon.roomCount;
         roomIndex++)
        degreeTotal += getRoomDegree(dungeon.rooms[roomIndex]);
    const uint8_t edgeCount = degreeTotal / 2;
    return edgeCount >= dungeon.roomCount
        ? static_cast<uint8_t>(edgeCount - dungeon.roomCount + 1)
        : 0;
}

void recordFailure(
    SimulationStats& stats, uint32_t seed, const std::string& reason)
{
    stats.failed++;
    if (stats.failures.size() < 32)
        stats.failures.push_back({seed, reason});
}

bool generateAndValidate(
    uint32_t seed,
    DungeonTopology& dungeon,
    DungeonGenerationReport& report,
    std::string& failureReason)
{
    SeededDungeonRandom randomSource(seed);
    if (!generateDungeonLayout(dungeon, randomSource, report))
    {
        const DungeonTopologyValidationError error =
            validateDungeonLayoutDetailed(dungeon);
        failureReason = error == DUNGEON_TOPOLOGY_VALID
            ? "generation exhausted its bounded attempts"
            : dungeonTopologyValidationErrorName(error);
        return false;
    }

    const DungeonTopologyValidationError error =
        validateDungeonLayoutDetailed(dungeon);
    if (error != DUNGEON_TOPOLOGY_VALID)
    {
        failureReason = dungeonTopologyValidationErrorName(error);
        return false;
    }
    return true;
}

SimulationStats runSimulation(const Options& options)
{
    SimulationStats stats;
    stats.requested = options.count;
    for (uint32_t index = 0; index < options.count; index++)
    {
        const uint32_t seed = options.seed + index;
        DungeonTopology dungeon;
        DungeonGenerationReport report;
        std::string failureReason;
        if (!generateAndValidate(seed, dungeon, report, failureReason))
        {
            stats.rejectedLayouts += report.attempts;
            recordFailure(stats, seed, failureReason);
            continue;
        }

        const uint8_t bossDistance = getRoomDistanceFromEntrance(
            dungeon, dungeon.bossRoom);
        const uint8_t branches = countBranches(dungeon);
        const uint8_t loops = countIndependentLoops(dungeon);
        stats.passed++;
        stats.roomTotal += dungeon.roomCount;
        stats.bossDistanceTotal += bossDistance;
        stats.branchTotal += branches;
        stats.loopTotal += loops;
        stats.puzzleTotal++;
        stats.attemptTotal += report.attempts;
        stats.rejectedLayouts += report.attempts - 1;
        stats.loopDungeons += loops > 0;
        if (dungeon.roomCount < stats.minRooms)
            stats.minRooms = dungeon.roomCount;
        if (dungeon.roomCount > stats.maxRooms)
            stats.maxRooms = dungeon.roomCount;
        if (bossDistance < stats.minBossDistance)
            stats.minBossDistance = bossDistance;
        if (bossDistance > stats.maxBossDistance)
            stats.maxBossDistance = bossDistance;
        if (branches < stats.minBranches)
            stats.minBranches = branches;
        if (branches > stats.maxBranches)
            stats.maxBranches = branches;
        if (report.attempts > stats.maxAttempts)
            stats.maxAttempts = report.attempts;
    }
    return stats;
}

double average(uint64_t total, uint32_t count)
{
    return count == 0 ? 0.0 : static_cast<double>(total) / count;
}

void printHumanSummary(const Options& options, const SimulationStats& stats)
{
    std::cout << "Dungeon topology simulation\n"
              << "  seeds: " << options.seed << ".."
              << static_cast<uint32_t>(
                     options.seed + options.count - 1) << '\n'
              << "  generated: " << stats.requested << '\n'
              << "  passed: " << stats.passed << '\n'
              << "  failed: " << stats.failed << '\n';
    if (stats.passed > 0)
    {
        std::cout << std::fixed << std::setprecision(2)
                  << "  rooms min/max/avg: "
                  << static_cast<unsigned>(stats.minRooms) << '/'
                  << static_cast<unsigned>(stats.maxRooms) << '/'
                  << average(stats.roomTotal, stats.passed) << '\n'
                  << "  boss distance min/max/avg: "
                  << static_cast<unsigned>(stats.minBossDistance) << '/'
                  << static_cast<unsigned>(stats.maxBossDistance) << '/'
                  << average(stats.bossDistanceTotal, stats.passed) << '\n'
                  << "  branch rooms min/max/avg: "
                  << static_cast<unsigned>(stats.minBranches) << '/'
                  << static_cast<unsigned>(stats.maxBranches) << '/'
                  << average(stats.branchTotal, stats.passed) << '\n'
                  << "  loop dungeons: " << stats.loopDungeons << " ("
                  << (100.0 * stats.loopDungeons / stats.passed) << "%)\n"
                  << "  independent loops: " << stats.loopTotal << '\n'
                  << "  designated Riddleman rooms: "
                  << stats.puzzleTotal << '\n'
                  << "  generation attempts avg/max: "
                  << average(stats.attemptTotal, stats.passed) << '/'
                  << static_cast<unsigned>(stats.maxAttempts) << '\n'
                  << "  rejected layouts: "
                  << stats.rejectedLayouts << '\n'
                  << "  room geometry: not included in Phase 3A\n";
        if (options.count == 1)
        {
            DungeonTopology dungeon;
            DungeonGenerationReport report;
            std::string reason;
            if (generateAndValidate(
                    options.seed, dungeon, report, reason))
                std::cout << "  fingerprint: 0x" << std::hex
                          << topologyFingerprint(dungeon, report)
                          << std::dec << '\n';
        }
    }
    for (const Failure& failure : stats.failures)
        std::cerr << "seed " << failure.seed << ": "
                  << failure.reason << '\n';
}

void printJsonSummary(const Options& options, const SimulationStats& stats)
{
    std::cout << std::fixed << std::setprecision(4)
              << '{'
              << "\"start_seed\":" << options.seed << ','
              << "\"count\":" << stats.requested << ','
              << "\"passed\":" << stats.passed << ','
              << "\"failed\":" << stats.failed << ','
              << "\"room_min\":"
              << (stats.passed ? static_cast<unsigned>(stats.minRooms) : 0)
              << ','
              << "\"room_max\":"
              << static_cast<unsigned>(stats.maxRooms) << ','
              << "\"room_average\":"
              << average(stats.roomTotal, stats.passed) << ','
              << "\"boss_distance_min\":"
              << (stats.passed
                    ? static_cast<unsigned>(stats.minBossDistance) : 0)
              << ','
              << "\"boss_distance_max\":"
              << static_cast<unsigned>(stats.maxBossDistance) << ','
              << "\"boss_distance_average\":"
              << average(stats.bossDistanceTotal, stats.passed) << ','
              << "\"branch_average\":"
              << average(stats.branchTotal, stats.passed) << ','
              << "\"loop_dungeons\":" << stats.loopDungeons << ','
              << "\"independent_loops\":" << stats.loopTotal << ','
              << "\"puzzle_rooms\":" << stats.puzzleTotal << ','
              << "\"attempt_average\":"
              << average(stats.attemptTotal, stats.passed) << ','
              << "\"attempt_max\":"
              << static_cast<unsigned>(stats.maxAttempts) << ','
              << "\"rejected_layouts\":"
              << stats.rejectedLayouts << ','
              << "\"geometry_included\":false,";
    std::cout << "\"failure_seeds\":[";
    for (std::size_t index = 0; index < stats.failures.size(); index++)
    {
        if (index != 0)
            std::cout << ',';
        std::cout << stats.failures[index].seed;
    }
    std::cout << "]}\n";
}

bool expect(bool condition, const char* message)
{
    if (condition)
    {
        std::cout << "[       OK ] " << message << '\n';
        return true;
    }
    std::cerr << "[  FAILED  ] " << message << '\n';
    return false;
}

bool runSelfTests()
{
    bool passed = true;

    SeededDungeonRandom invalidRandom(1);
    DungeonTopology invalidDungeon;
    passed &= expect(
        !generateDungeonTopology(
            invalidDungeon,
            MIN_DUNGEON_ROOMS - 1,
            invalidRandom) &&
        !generateDungeonTopology(
            invalidDungeon,
            MAX_DUNGEON_ROOMS + 1,
            invalidRandom),
        "invalid room-count boundaries are rejected");

    DungeonTopology first;
    DungeonTopology replay;
    DungeonGenerationReport firstReport;
    DungeonGenerationReport replayReport;
    std::string reason;
    std::string replayReason;
    const bool firstGenerated = generateAndValidate(
        12345, first, firstReport, reason);
    const bool replayGenerated = generateAndValidate(
        12345, replay, replayReport, replayReason);
    passed &= expect(
        firstGenerated && replayGenerated &&
        topologyFingerprint(first, firstReport) ==
            topologyFingerprint(replay, replayReport),
        "the same seed reproduces the same topology");

    if (firstGenerated)
    {
        DungeonTopology broken = first;
        broken.rooms[0].north = broken.roomCount;
        passed &= expect(
            validateDungeonLayoutDetailed(broken) ==
                DUNGEON_TOPOLOGY_INVALID_NEIGHBOR,
            "invalid neighbor indices are diagnosed");

        broken = first;
        broken.rooms[1].dungeonX = broken.rooms[0].dungeonX;
        broken.rooms[1].dungeonY = broken.rooms[0].dungeonY;
        passed &= expect(
            validateDungeonLayoutDetailed(broken) ==
                DUNGEON_TOPOLOGY_DUPLICATE_COORDINATE,
            "duplicate logical coordinates are diagnosed");

        broken = first;
        broken.riddleRoom = NO_ROOM;
        passed &= expect(
            validateDungeonLayoutDetailed(broken) ==
                DUNGEON_TOPOLOGY_INVALID_RIDDLE_ROOM,
            "invalid Riddleman placement is diagnosed");
    }

    struct RegressionCase
    {
        uint32_t seed;
        uint64_t expectedFingerprint;
    };
    const RegressionCase regressionCases[] = {
        {1, 0x2087fa41dc9e5db7ULL},
        {77, 0xbd9d7d1a8ea3d0b3ULL},
        {12345, 0x86dd599397da814aULL}
    };
    for (const RegressionCase& regression : regressionCases)
    {
        DungeonTopology dungeon;
        DungeonGenerationReport report;
        std::string failureReason;
        const bool generated = generateAndValidate(
            regression.seed, dungeon, report, failureReason);
        const uint64_t fingerprint = generated
            ? topologyFingerprint(dungeon, report) : 0;
        if (!generated || fingerprint != regression.expectedFingerprint)
        {
            std::cerr << "seed " << regression.seed
                      << " regression fingerprint: expected 0x"
                      << std::hex << regression.expectedFingerprint
                      << ", got 0x" << fingerprint << std::dec << '\n';
            passed = false;
        }
    }
    passed &= expect(passed, "fixed-seed topology fingerprints match");
    return passed;
}
}

int main(int argc, char** argv)
{
    Options options;
    if (!parseOptions(argc, argv, options))
    {
        printUsage(argv[0]);
        return 2;
    }
    if (options.selfTest)
        return runSelfTests() ? 0 : 1;

    const SimulationStats stats = runSimulation(options);
    if (options.json)
        printJsonSummary(options, stats);
    else
        printHumanSummary(options, stats);
    return stats.failed == 0 ? 0 : 1;
}
