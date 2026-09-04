#include <Arduino.h>
#include <unity.h>

#include "../../src/map/movement.h"
#include "../../src/dungeon/roomgen.cpp"

static void addFourTestConnections(DungeonRoom& room)
{
    TEST_ASSERT_TRUE(addRoomConnection(
        room, DIR_NORTH, ROOM_CONNECTION_MIN, 0));
    TEST_ASSERT_TRUE(addRoomConnection(
        room,
        DIR_SOUTH,
        ROOM_CONNECTION_MAX,
        ROOM_SIZE - 1));
    TEST_ASSERT_TRUE(addRoomConnection(
        room, DIR_WEST, 0, ROOM_CONNECTION_MIN + 1));
    TEST_ASSERT_TRUE(addRoomConnection(
        room,
        DIR_EAST,
        ROOM_SIZE - 1,
        ROOM_CONNECTION_MAX - 1));
}

static void addHorizontalTestConnections(
    DungeonRoom& room,
    uint8_t westY = ROOM_CONNECTION_MIN + 1,
    uint8_t eastY = ROOM_CONNECTION_MAX - 1)
{
    TEST_ASSERT_TRUE(addRoomConnection(room, DIR_WEST, 0, westY));
    TEST_ASSERT_TRUE(addRoomConnection(
        room, DIR_EAST, ROOM_SIZE - 1, eastY));
}

static uint16_t countTiles(const DungeonRoom& room, TileType tile)
{
    uint16_t count = 0;

    for (uint8_t y = 0; y < ROOM_SIZE; y++)
    {
        for (uint8_t x = 0; x < ROOM_SIZE; x++)
        {
            if (room.map.tiles[y][x] == tile)
                count++;
        }
    }

    return count;
}

static uint16_t countInteriorTiles(
    const DungeonRoom& room,
    TileType tile)
{
    uint16_t count = 0;

    for (uint8_t y = 1; y < ROOM_SIZE - 1; y++)
    {
        for (uint8_t x = 1; x < ROOM_SIZE - 1; x++)
        {
            if (room.map.tiles[y][x] == tile)
                count++;
        }
    }

    return count;
}

static void assertBoundaryContainsOnlyWallsAndDoors(
    const DungeonRoom& room)
{
    for (uint8_t offset = 0; offset < ROOM_SIZE; offset++)
    {
        const TileType north = room.map.tiles[0][offset];
        const TileType south = room.map.tiles[ROOM_SIZE - 1][offset];
        const TileType west = room.map.tiles[offset][0];
        const TileType east = room.map.tiles[offset][ROOM_SIZE - 1];

        TEST_ASSERT_TRUE(north == TILE_WALL || north == TILE_DOOR);
        TEST_ASSERT_TRUE(south == TILE_WALL || south == TILE_DOOR);
        TEST_ASSERT_TRUE(west == TILE_WALL || west == TILE_DOOR);
        TEST_ASSERT_TRUE(east == TILE_WALL || east == TILE_DOOR);
    }
}

static void assertAllTestEntriesAreFloor(const DungeonRoom& room)
{
    static constexpr RoomEntry entries[] = {
        ENTRY_NORTH,
        ENTRY_EAST,
        ENTRY_SOUTH,
        ENTRY_WEST
    };

    for (RoomEntry entry : entries)
    {
        uint8_t x = 0;
        uint8_t y = 0;

        TEST_ASSERT_TRUE(getRoomEntryPosition(room, entry, x, y));
        TEST_ASSERT_EQUAL(TILE_FLOOR, room.map.tiles[y][x]);
    }
}

static void assertEveryConnectionEntryIsFloor(const DungeonRoom& room)
{
    const uint8_t count = room.connectionCount < MAX_ROOM_CONNECTIONS
        ? room.connectionCount
        : MAX_ROOM_CONNECTIONS;

    for (uint8_t i = 0; i < count; i++)
    {
        int x = 0;
        int y = 0;

        TEST_ASSERT_TRUE(getConnectionInteriorPosition(
            room.connections[i], x, y));
        TEST_ASSERT_EQUAL(TILE_FLOOR, room.map.tiles[y][x]);
    }
}

void test_value_initialized_room_has_no_connections()
{
    DungeonRoom room{};

    TEST_ASSERT_EQUAL_UINT8(0, room.connectionCount);
    TEST_ASSERT_NULL(getRoomConnection(room, DIR_NORTH));
}

void test_cardinal_edge_connections_are_accepted()
{
    DungeonRoom room{};

    TEST_ASSERT_TRUE(addRoomConnection(
        room, DIR_NORTH, ROOM_CONNECTION_MIN, 0));
    TEST_ASSERT_TRUE(addRoomConnection(
        room, DIR_SOUTH, ROOM_CONNECTION_MAX, ROOM_SIZE - 1));
    TEST_ASSERT_TRUE(addRoomConnection(
        room, DIR_WEST, 0, ROOM_CONNECTION_MIN));
    TEST_ASSERT_TRUE(addRoomConnection(
        room, DIR_EAST, ROOM_SIZE - 1, ROOM_CONNECTION_MAX));
    TEST_ASSERT_EQUAL_UINT8(MAX_ROOM_CONNECTIONS, room.connectionCount);
}

void test_connections_on_wrong_edges_are_rejected()
{
    DungeonRoom room{};
    const uint8_t center = ROOM_SIZE / 2;

    TEST_ASSERT_FALSE(addRoomConnection(room, DIR_NORTH, center, 1));
    TEST_ASSERT_FALSE(addRoomConnection(
        room, DIR_SOUTH, center, ROOM_SIZE - 2));
    TEST_ASSERT_FALSE(addRoomConnection(room, DIR_WEST, 1, center));
    TEST_ASSERT_FALSE(addRoomConnection(
        room, DIR_EAST, ROOM_SIZE - 2, center));
    TEST_ASSERT_FALSE(addRoomConnection(room, DIR_NORTHEAST, center, 0));
    TEST_ASSERT_FALSE(addRoomConnection(room, DIR_NORTH, ROOM_SIZE, 0));
    TEST_ASSERT_FALSE(addRoomConnection(room, DIR_NORTH, 0, 0));
    TEST_ASSERT_FALSE(addRoomConnection(room, DIR_NORTH, 1, 0));
    TEST_ASSERT_FALSE(addRoomConnection(
        room, DIR_SOUTH, ROOM_SIZE - 2, ROOM_SIZE - 1));
    TEST_ASSERT_FALSE(addRoomConnection(room, DIR_WEST, 0, 1));
    TEST_ASSERT_FALSE(addRoomConnection(
        room, DIR_EAST, ROOM_SIZE - 1, ROOM_SIZE - 1));
    TEST_ASSERT_EQUAL_UINT8(0, room.connectionCount);
}

void test_connection_capacity_fails_without_overwriting_entries()
{
    DungeonRoom room{};
    const uint8_t center = ROOM_SIZE / 2;

    TEST_ASSERT_TRUE(addRoomConnection(room, DIR_NORTH, center, 0));
    TEST_ASSERT_TRUE(addRoomConnection(
        room, DIR_SOUTH, center, ROOM_SIZE - 1));
    TEST_ASSERT_TRUE(addRoomConnection(room, DIR_WEST, 0, center));
    TEST_ASSERT_TRUE(addRoomConnection(
        room, DIR_EAST, ROOM_SIZE - 1, center));

    TEST_ASSERT_FALSE(addRoomConnection(room, DIR_NORTH, center - 1, 0));
    TEST_ASSERT_EQUAL_UINT8(MAX_ROOM_CONNECTIONS, room.connectionCount);
    TEST_ASSERT_EQUAL_UINT8(
        ROOM_SIZE - 1,
        room.connections[MAX_ROOM_CONNECTIONS - 1].x);
}

void test_clear_removes_existing_connections()
{
    DungeonRoom room{};

    TEST_ASSERT_TRUE(addRoomConnection(room, DIR_NORTH, ROOM_SIZE / 2, 0));
    clearRoomConnections(room);

    TEST_ASSERT_EQUAL_UINT8(0, room.connectionCount);
    TEST_ASSERT_NULL(getRoomConnection(room, DIR_NORTH));
}

void test_random_connection_offsets_stay_in_safe_range()
{
    for (uint16_t sample = 0; sample < 256; sample++)
    {
        const uint8_t offset = randomRoomConnectionOffset();
        TEST_ASSERT_GREATER_OR_EQUAL_UINT8(ROOM_CONNECTION_MIN, offset);
        TEST_ASSERT_LESS_OR_EQUAL_UINT8(ROOM_CONNECTION_MAX, offset);
    }
}

void test_neighbors_populate_safe_variable_wall_coordinates()
{
    DungeonRoom room{};
    room.north = 1;
    room.south = 2;
    room.east = 3;
    room.west = 4;

    populateRoomConnections(room);

    const RoomConnection* north = getRoomConnection(room, DIR_NORTH);
    const RoomConnection* south = getRoomConnection(room, DIR_SOUTH);
    const RoomConnection* east = getRoomConnection(room, DIR_EAST);
    const RoomConnection* west = getRoomConnection(room, DIR_WEST);

    TEST_ASSERT_NOT_NULL(north);
    TEST_ASSERT_NOT_NULL(south);
    TEST_ASSERT_NOT_NULL(east);
    TEST_ASSERT_NOT_NULL(west);
    TEST_ASSERT_EQUAL_UINT8(0, north->y);
    TEST_ASSERT_EQUAL_UINT8(ROOM_SIZE - 1, south->y);
    TEST_ASSERT_EQUAL_UINT8(ROOM_SIZE - 1, east->x);
    TEST_ASSERT_EQUAL_UINT8(0, west->x);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(ROOM_CONNECTION_MIN, north->x);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(ROOM_CONNECTION_MAX, north->x);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(ROOM_CONNECTION_MIN, south->x);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(ROOM_CONNECTION_MAX, south->x);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(ROOM_CONNECTION_MIN, east->y);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(ROOM_CONNECTION_MAX, east->y);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(ROOM_CONNECTION_MIN, west->y);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(ROOM_CONNECTION_MAX, west->y);
}

void test_room_generation_places_doors_from_connections()
{
    DungeonRoom room{};
    room.type = ROOM_PUZZLE;
    room.shape = SHAPE_SQUARE;

    TEST_ASSERT_TRUE(addRoomConnection(
        room, DIR_NORTH, ROOM_CONNECTION_MIN, 0));
    TEST_ASSERT_TRUE(addRoomConnection(
        room,
        DIR_SOUTH,
        ROOM_CONNECTION_MAX,
        ROOM_SIZE - 1));
    TEST_ASSERT_TRUE(addRoomConnection(
        room, DIR_WEST, 0, ROOM_CONNECTION_MIN + 1));
    TEST_ASSERT_TRUE(addRoomConnection(
        room,
        DIR_EAST,
        ROOM_SIZE - 1,
        ROOM_CONNECTION_MAX - 1));

    generateRoom(room);

    TEST_ASSERT_EQUAL(
        TILE_DOOR,
        room.map.tiles[0][ROOM_CONNECTION_MIN]);
    TEST_ASSERT_EQUAL(
        TILE_DOOR,
        room.map.tiles[ROOM_SIZE - 1][ROOM_CONNECTION_MAX]);
    TEST_ASSERT_EQUAL(
        TILE_DOOR,
        room.map.tiles[ROOM_CONNECTION_MIN + 1][0]);
    TEST_ASSERT_EQUAL(
        TILE_DOOR,
        room.map.tiles[ROOM_CONNECTION_MAX - 1][ROOM_SIZE - 1]);
}

void test_entry_positions_use_destination_connections_and_walkable_tiles()
{
    DungeonRoom room{};
    room.type = ROOM_PUZZLE;
    room.shape = SHAPE_SQUARE;

    TEST_ASSERT_TRUE(addRoomConnection(
        room, DIR_NORTH, ROOM_CONNECTION_MIN, 0));
    TEST_ASSERT_TRUE(addRoomConnection(
        room,
        DIR_SOUTH,
        ROOM_CONNECTION_MAX,
        ROOM_SIZE - 1));
    TEST_ASSERT_TRUE(addRoomConnection(
        room, DIR_WEST, 0, ROOM_CONNECTION_MIN + 1));
    TEST_ASSERT_TRUE(addRoomConnection(
        room,
        DIR_EAST,
        ROOM_SIZE - 1,
        ROOM_CONNECTION_MAX - 1));

    generateRoom(room);

    uint8_t x = 0;
    uint8_t y = 0;

    TEST_ASSERT_TRUE(getRoomEntryPosition(room, ENTRY_NORTH, x, y));
    TEST_ASSERT_EQUAL_UINT8(ROOM_CONNECTION_MIN, x);
    TEST_ASSERT_EQUAL_UINT8(1, y);
    TEST_ASSERT_EQUAL(TILE_FLOOR, room.map.tiles[y][x]);

    TEST_ASSERT_TRUE(getRoomEntryPosition(room, ENTRY_SOUTH, x, y));
    TEST_ASSERT_EQUAL_UINT8(ROOM_CONNECTION_MAX, x);
    TEST_ASSERT_EQUAL_UINT8(ROOM_SIZE - 2, y);
    TEST_ASSERT_EQUAL(TILE_FLOOR, room.map.tiles[y][x]);

    TEST_ASSERT_TRUE(getRoomEntryPosition(room, ENTRY_WEST, x, y));
    TEST_ASSERT_EQUAL_UINT8(1, x);
    TEST_ASSERT_EQUAL_UINT8(ROOM_CONNECTION_MIN + 1, y);
    TEST_ASSERT_EQUAL(TILE_FLOOR, room.map.tiles[y][x]);

    TEST_ASSERT_TRUE(getRoomEntryPosition(room, ENTRY_EAST, x, y));
    TEST_ASSERT_EQUAL_UINT8(ROOM_SIZE - 2, x);
    TEST_ASSERT_EQUAL_UINT8(ROOM_CONNECTION_MAX - 1, y);
    TEST_ASSERT_EQUAL(TILE_FLOOR, room.map.tiles[y][x]);
}

void test_full_rectangle_remains_connected_and_full_sized()
{
    DungeonRoom room{};
    room.type = ROOM_PUZZLE;
    room.shape = SHAPE_SQUARE;
    addFourTestConnections(room);

    generateRoom(room);

    TEST_ASSERT_EQUAL(SHAPE_SQUARE, room.shape);
    TEST_ASSERT_TRUE(validateRoomConnectivity(room));
    TEST_ASSERT_EQUAL_UINT16(
        (ROOM_SIZE - 2) * (ROOM_SIZE - 2),
        countTiles(room, TILE_FLOOR));
    assertBoundaryContainsOnlyWallsAndDoors(room);
    assertAllTestEntriesAreFloor(room);
}

void test_small_rectangles_stay_bounded_connected_and_playable()
{
    for (uint8_t sample = 0; sample < 16; sample++)
    {
        DungeonRoom room{};
        room.type = ROOM_PUZZLE;
        room.shape = SHAPE_SMALL_RECTANGLE;
        addFourTestConnections(room);

        generateRoom(room);

        TEST_ASSERT_EQUAL(SHAPE_SMALL_RECTANGLE, room.shape);
        TEST_ASSERT_TRUE(validateRoomConnectivity(room));
        TEST_ASSERT_TRUE(
            countTiles(room, TILE_FLOOR) >=
            SMALL_ROOM_MIN_SIZE * SMALL_ROOM_MIN_SIZE);
        TEST_ASSERT_TRUE(countInteriorTiles(room, TILE_WALL) > 0);
        assertBoundaryContainsOnlyWallsAndDoors(room);
        assertAllTestEntriesAreFloor(room);
    }
}

void test_l_rooms_stay_bounded_connected_and_playable()
{
    const uint16_t minimumLArea =
        L_ROOM_MIN_ARM * L_ROOM_MIN_SIZE +
        L_ROOM_MIN_SIZE * L_ROOM_MIN_ARM -
        L_ROOM_MIN_ARM * L_ROOM_MIN_ARM;

    for (uint8_t sample = 0; sample < 16; sample++)
    {
        DungeonRoom room{};
        room.type = ROOM_PUZZLE;
        room.shape = SHAPE_L;
        addFourTestConnections(room);

        generateRoom(room);

        TEST_ASSERT_EQUAL(SHAPE_L, room.shape);
        TEST_ASSERT_TRUE(validateRoomConnectivity(room));
        TEST_ASSERT_TRUE(countTiles(room, TILE_FLOOR) >= minimumLArea);
        TEST_ASSERT_TRUE(countInteriorTiles(room, TILE_WALL) > 0);
        assertBoundaryContainsOnlyWallsAndDoors(room);
        assertAllTestEntriesAreFloor(room);
    }
}

void test_connection_helper_carves_a_reachable_manhattan_approach()
{
    DungeonRoom room{};
    fillRoom(room, TILE_WALL);

    TEST_ASSERT_TRUE(addRoomConnection(
        room, DIR_NORTH, ROOM_CONNECTION_MIN, 0));
    TEST_ASSERT_TRUE(carveRectangle(room, 6, 6, 4, 4));
    TEST_ASSERT_TRUE(connectRoomConnectionToFloor(
        room,
        room.connections[0],
        7,
        7));

    room.map.tiles[0][ROOM_CONNECTION_MIN] = TILE_DOOR;

    TEST_ASSERT_EQUAL(
        TILE_FLOOR,
        room.map.tiles[1][ROOM_CONNECTION_MIN]);
    TEST_ASSERT_EQUAL(
        TILE_FLOOR,
        room.map.tiles[7][ROOM_CONNECTION_MIN]);
    TEST_ASSERT_EQUAL(TILE_FLOOR, room.map.tiles[7][7]);
    TEST_ASSERT_TRUE(validateRoomConnectivity(room));
}

void test_invalid_shape_falls_back_to_full_rectangle()
{
    DungeonRoom room{};
    room.type = ROOM_PUZZLE;
    room.shape = static_cast<RoomShape>(255);
    addFourTestConnections(room);

    generateRoom(room);

    TEST_ASSERT_EQUAL(SHAPE_SQUARE, room.shape);
    TEST_ASSERT_TRUE(validateRoomConnectivity(room));
    TEST_ASSERT_EQUAL_UINT16(
        (ROOM_SIZE - 2) * (ROOM_SIZE - 2),
        countTiles(room, TILE_FLOOR));
    assertAllTestEntriesAreFloor(room);
}

void test_production_shape_selection_is_conservative_and_bounded()
{
    DungeonRoom eligible{};
    eligible.type = ROOM_PUZZLE;
    addHorizontalTestConnections(eligible);

    uint8_t squareCount = 0;
    uint8_t smallRectangleCount = 0;
    uint8_t lRoomCount = 0;
    uint8_t caveCount = 0;
    uint8_t corridorCount = 0;

    for (uint8_t roll = 0; roll < 100; roll++)
    {
        const RoomShape shape = selectProductionRoomShape(eligible, roll);

        TEST_ASSERT_TRUE(
            shape == SHAPE_SQUARE ||
            shape == SHAPE_SMALL_RECTANGLE ||
            shape == SHAPE_L ||
            shape == SHAPE_WINDING_CORRIDOR ||
            shape == SHAPE_CAVE);

        if (roll < 25)
            TEST_ASSERT_EQUAL(SHAPE_SQUARE, shape);
        else if (roll < 50)
            TEST_ASSERT_EQUAL(SHAPE_SMALL_RECTANGLE, shape);
        else if (roll < 70)
            TEST_ASSERT_EQUAL(SHAPE_L, shape);
        else if (roll < 90)
        {
            TEST_ASSERT_EQUAL(SHAPE_CAVE, shape);
        }
        else
        {
            TEST_ASSERT_EQUAL(SHAPE_WINDING_CORRIDOR, shape);
        }

        switch (shape)
        {
            case SHAPE_SQUARE:
                squareCount++;
                break;

            case SHAPE_SMALL_RECTANGLE:
                smallRectangleCount++;
                break;

            case SHAPE_L:
                lRoomCount++;
                break;

            case SHAPE_CAVE:
                caveCount++;
                break;

            case SHAPE_WINDING_CORRIDOR:
                corridorCount++;
                break;

            default:
                break;
        }
    }

    TEST_ASSERT_EQUAL_UINT8(25, squareCount);
    TEST_ASSERT_EQUAL_UINT8(25, smallRectangleCount);
    TEST_ASSERT_EQUAL_UINT8(20, lRoomCount);
    TEST_ASSERT_EQUAL_UINT8(20, caveCount);
    TEST_ASSERT_EQUAL_UINT8(10, corridorCount);

    DungeonRoom windingIneligible{};
    windingIneligible.type = ROOM_PUZZLE;
    TEST_ASSERT_TRUE(addRoomConnection(
        windingIneligible, DIR_WEST, 0, ROOM_SIZE / 2));

    TEST_ASSERT_EQUAL(
        SHAPE_CAVE,
        selectProductionRoomShape(windingIneligible, 70));
    TEST_ASSERT_EQUAL(
        SHAPE_SMALL_RECTANGLE,
        selectProductionRoomShape(windingIneligible, 90));
    TEST_ASSERT_EQUAL(
        SHAPE_L,
        selectProductionRoomShape(windingIneligible, 95));

    DungeonRoom noConnections{};
    noConnections.type = ROOM_PUZZLE;
    TEST_ASSERT_EQUAL(
        SHAPE_SQUARE,
        selectProductionRoomShape(noConnections, 70));
}

void test_winding_corridor_eligibility_requires_two_safe_connections()
{
    DungeonRoom room{};
    room.type = ROOM_PUZZLE;

    TEST_ASSERT_FALSE(isWindingCorridorEligible(room));
    TEST_ASSERT_TRUE(addRoomConnection(
        room, DIR_WEST, 0, ROOM_CONNECTION_MIN));
    TEST_ASSERT_FALSE(isWindingCorridorEligible(room));
    TEST_ASSERT_TRUE(addRoomConnection(
        room,
        DIR_EAST,
        ROOM_SIZE - 1,
        ROOM_CONNECTION_MAX));
    TEST_ASSERT_TRUE(isWindingCorridorEligible(room));
    TEST_ASSERT_TRUE(addRoomConnection(
        room, DIR_NORTH, ROOM_SIZE / 2, 0));
    TEST_ASSERT_FALSE(isWindingCorridorEligible(room));

    DungeonRoom entrance{};
    entrance.type = ROOM_ENTRANCE;
    addHorizontalTestConnections(entrance);
    TEST_ASSERT_FALSE(isWindingCorridorEligible(entrance));
}

void test_corridor_width_selection_and_segment_carving()
{
    TEST_ASSERT_EQUAL_UINT8(1, selectCorridorWidth(0));
    TEST_ASSERT_EQUAL_UINT8(1, selectCorridorWidth(1));
    TEST_ASSERT_EQUAL_UINT8(1, selectCorridorWidth(2));
    TEST_ASSERT_EQUAL_UINT8(2, selectCorridorWidth(3));

    DungeonRoom widthOne{};
    fillRoom(widthOne, TILE_WALL);
    TEST_ASSERT_TRUE(carveCorridorSegment(
        widthOne, 2, 4, 10, 4, 1));
    TEST_ASSERT_EQUAL_UINT16(9, countTiles(widthOne, TILE_FLOOR));

    DungeonRoom widthTwo{};
    fillRoom(widthTwo, TILE_WALL);
    TEST_ASSERT_TRUE(carveCorridorSegment(
        widthTwo, 2, 4, 10, 4, 2));
    TEST_ASSERT_EQUAL_UINT16(20, countTiles(widthTwo, TILE_FLOOR));
    TEST_ASSERT_FALSE(carveCorridorSegment(
        widthTwo, 2, 2, 5, 5, 1));
    assertBoundaryContainsOnlyWallsAndDoors(widthTwo);
}

void test_winding_paths_have_meaningful_orthogonal_bends()
{
    const RoomConnection west = {
        DIR_WEST, 0, ROOM_CONNECTION_MIN + 1};
    const RoomConnection east = {
        DIR_EAST, ROOM_SIZE - 1, ROOM_CONNECTION_MAX - 1};
    CorridorPath oppositePath{};

    TEST_ASSERT_TRUE(generateWindingCorridorPath(
        west, east, 1, oppositePath));
    TEST_ASSERT_TRUE(validateCorridorPath(oppositePath));
    TEST_ASSERT_EQUAL_UINT8(4, getCorridorBendCount(oppositePath));

    const RoomConnection north = {
        DIR_NORTH, ROOM_CONNECTION_MIN + 2, 0};
    CorridorPath adjacentPath{};
    bool adjacentGenerated = false;

    for (uint8_t attempt = 0;
         attempt < MAX_CORRIDOR_GENERATION_ATTEMPTS;
         attempt++)
    {
        if (generateWindingCorridorPath(
              north, east, 2, adjacentPath))
        {
            adjacentGenerated = true;
            break;
        }
    }

    TEST_ASSERT_TRUE(adjacentGenerated);
    TEST_ASSERT_TRUE(validateCorridorPath(adjacentPath));
    TEST_ASSERT_EQUAL_UINT8(3, getCorridorBendCount(adjacentPath));

    CorridorPath selfIntersecting{};
    selfIntersecting.width = 1;
    selfIntersecting.pointCount = 5;
    selfIntersecting.points[0] = {2, 2};
    selfIntersecting.points[1] = {8, 2};
    selfIntersecting.points[2] = {8, 8};
    selfIntersecting.points[3] = {4, 8};
    selfIntersecting.points[4] = {4, 2};
    TEST_ASSERT_FALSE(validateCorridorPath(selfIntersecting));
}

void test_winding_corridors_connect_stored_doors_and_entries()
{
    for (uint8_t sample = 0; sample < 24; sample++)
    {
        DungeonRoom room{};
        room.type = ROOM_PUZZLE;
        room.shape = SHAPE_WINDING_CORRIDOR;
        addHorizontalTestConnections(room);

        generateRoom(room);

        TEST_ASSERT_EQUAL(SHAPE_WINDING_CORRIDOR, room.shape);
        TEST_ASSERT_TRUE(validateRoomConnectivity(room));
        TEST_ASSERT_EQUAL(
            TILE_DOOR,
            room.map.tiles[ROOM_CONNECTION_MIN + 1][0]);
        TEST_ASSERT_EQUAL(
            TILE_DOOR,
            room.map.tiles[ROOM_CONNECTION_MAX - 1][ROOM_SIZE - 1]);
        TEST_ASSERT_TRUE(countInteriorTiles(room, TILE_WALL) > 0);
        TEST_ASSERT_TRUE(
            countTiles(room, TILE_FLOOR) <
            (ROOM_SIZE - 2) * (ROOM_SIZE - 2));
        assertBoundaryContainsOnlyWallsAndDoors(room);
        assertEveryConnectionEntryIsFloor(room);

        uint8_t x = 0;
        uint8_t y = 0;
        TEST_ASSERT_TRUE(getRoomEntryPosition(room, ENTRY_WEST, x, y));
        TEST_ASSERT_EQUAL_UINT8(1, x);
        TEST_ASSERT_EQUAL_UINT8(ROOM_CONNECTION_MIN + 1, y);
        TEST_ASSERT_TRUE(getRoomEntryPosition(room, ENTRY_EAST, x, y));
        TEST_ASSERT_EQUAL_UINT8(ROOM_SIZE - 2, x);
        TEST_ASSERT_EQUAL_UINT8(ROOM_CONNECTION_MAX - 1, y);
    }
}

void test_invalid_winding_corridor_requests_fall_back_safely()
{
    DungeonRoom oneConnection{};
    oneConnection.type = ROOM_PUZZLE;
    oneConnection.shape = SHAPE_WINDING_CORRIDOR;
    TEST_ASSERT_TRUE(addRoomConnection(
        oneConnection, DIR_WEST, 0, ROOM_SIZE / 2));

    generateRoom(oneConnection);

    TEST_ASSERT_EQUAL(SHAPE_SQUARE, oneConnection.shape);
    TEST_ASSERT_TRUE(validateRoomConnectivity(oneConnection));

    DungeonRoom threeConnections{};
    threeConnections.type = ROOM_PUZZLE;
    threeConnections.shape = SHAPE_WINDING_CORRIDOR;
    addHorizontalTestConnections(threeConnections);
    TEST_ASSERT_TRUE(addRoomConnection(
        threeConnections, DIR_NORTH, ROOM_SIZE / 2, 0));

    generateRoom(threeConnections);

    TEST_ASSERT_EQUAL(SHAPE_SQUARE, threeConnections.shape);
    TEST_ASSERT_TRUE(validateRoomConnectivity(threeConnections));
    assertEveryConnectionEntryIsFloor(threeConnections);
}

void test_winding_room_content_markers_remain_on_connected_interior_floor()
{
    DungeonRoom room{};
    room.type = ROOM_COMBAT;
    room.shape = SHAPE_WINDING_CORRIDOR;
    addHorizontalTestConnections(room);

    generateRoom(room);

    TEST_ASSERT_EQUAL(SHAPE_WINDING_CORRIDOR, room.shape);
    TEST_ASSERT_TRUE(validateRoomConnectivity(room));
    TEST_ASSERT_EQUAL_UINT16(2, countTiles(room, TILE_ENEMY_START));

    for (uint8_t y = 0; y < ROOM_SIZE; y++)
    {
        for (uint8_t x = 0; x < ROOM_SIZE; x++)
        {
            if (room.map.tiles[y][x] != TILE_ENEMY_START)
                continue;

            TEST_ASSERT_TRUE(x > 0 && x < ROOM_SIZE - 1);
            TEST_ASSERT_TRUE(y > 0 && y < ROOM_SIZE - 1);
            TEST_ASSERT_FALSE(isReservedContentTile(room, x, y));
        }
    }

    assertEveryConnectionEntryIsFloor(room);
}

void test_oversized_encounter_templates_now_place_two_monsters()
{
    static constexpr RoomType largeEncounterTypes[] = {
        ROOM_COMBAT,
        ROOM_AMBUSH
    };

    for (RoomType type : largeEncounterTypes)
    {
        DungeonRoom room{};
        room.type = type;
        room.shape = SHAPE_SQUARE;
        addHorizontalTestConnections(room);

        generateRoom(room);

        TEST_ASSERT_TRUE(validateRoomConnectivity(room));
        TEST_ASSERT_EQUAL_UINT16(2, countTiles(room, TILE_ENEMY_START));
    }

    // The boss template has exactly two Skeleton guards and one Skeleton Mage.
    DungeonRoom boss{};
    boss.type = ROOM_BOSS;
    boss.shape = SHAPE_SQUARE;
    addHorizontalTestConnections(boss);
    generateRoom(boss);
    TEST_ASSERT_EQUAL_UINT16(0, countTiles(boss, TILE_ENEMY_START));
    TEST_ASSERT_EQUAL_UINT16(2, countTiles(boss, TILE_SKELETON_START));
    TEST_ASSERT_EQUAL_UINT16(
        1, countTiles(boss, TILE_SKELETON_MAGE_START));
}

void test_empty_room_has_no_encounter_markers()
{
    DungeonRoom room{};
    room.type = ROOM_EMPTY;
    room.shape = SHAPE_SQUARE;
    addHorizontalTestConnections(room);

    generateRoom(room);

    TEST_ASSERT_TRUE(validateRoomConnectivity(room));
    TEST_ASSERT_EQUAL_UINT16(0, countTiles(room, TILE_ENEMY_START));
}

void test_combat_and_ambush_markers_use_distinct_placement_biases()
{
    DungeonRoom combat{};
    combat.type = ROOM_COMBAT;
    combat.encounterTheme = ENCOUNTER_UNDEAD;
    combat.shape = SHAPE_SQUARE;
    addHorizontalTestConnections(combat);
    generateRoom(combat);

    DungeonRoom ambush{};
    ambush.type = ROOM_AMBUSH;
    ambush.encounterTheme = ENCOUNTER_GOBLIN;
    ambush.shape = SHAPE_SQUARE;
    addHorizontalTestConnections(ambush);
    generateRoom(ambush);

    const int center = ROOM_SIZE / 2;
    int combatDistance = 0;
    int ambushDistance = 0;

    for (uint8_t y = 0; y < ROOM_SIZE; y++)
    {
        for (uint8_t x = 0; x < ROOM_SIZE; x++)
        {
            const int distance = abs(static_cast<int>(x) - center) +
                abs(static_cast<int>(y) - center);

            if (combat.map.tiles[y][x] == TILE_ENEMY_START)
                combatDistance += distance;
            if (ambush.map.tiles[y][x] == TILE_ENEMY_START)
                ambushDistance += distance;
        }
    }

    TEST_ASSERT_EQUAL(ENCOUNTER_UNDEAD, combat.encounterTheme);
    TEST_ASSERT_EQUAL(ENCOUNTER_GOBLIN, ambush.encounterTheme);
    TEST_ASSERT_TRUE(combatDistance < ambushDistance);
}

void test_cave_parameter_selection_and_eligibility_are_bounded()
{
    TEST_ASSERT_EQUAL_UINT8(1, selectCaveChamberCount(0));
    TEST_ASSERT_EQUAL_UINT8(1, selectCaveChamberCount(34));
    TEST_ASSERT_EQUAL_UINT8(2, selectCaveChamberCount(35));
    TEST_ASSERT_EQUAL_UINT8(2, selectCaveChamberCount(69));
    TEST_ASSERT_EQUAL_UINT8(3, selectCaveChamberCount(70));
    TEST_ASSERT_EQUAL_UINT8(3, selectCaveChamberCount(89));
    TEST_ASSERT_EQUAL_UINT8(4, selectCaveChamberCount(90));
    TEST_ASSERT_EQUAL_UINT8(4, selectCaveChamberCount(99));

    bool foundMinimumCoverage = false;
    bool foundMaximumCoverage = false;
    for (uint16_t roll = 0; roll < 256; roll++)
    {
        const uint8_t coverage = selectCaveTargetCoverage(
            static_cast<uint8_t>(roll));
        TEST_ASSERT_TRUE(coverage >= CAVE_MIN_TARGET_COVERAGE);
        TEST_ASSERT_TRUE(coverage <= CAVE_MAX_TARGET_COVERAGE);
        foundMinimumCoverage |= coverage == CAVE_MIN_TARGET_COVERAGE;
        foundMaximumCoverage |= coverage == CAVE_MAX_TARGET_COVERAGE;
    }

    TEST_ASSERT_TRUE(foundMinimumCoverage);
    TEST_ASSERT_TRUE(foundMaximumCoverage);
    TEST_ASSERT_TRUE(
        caveBlobBaseRadius(1, CAVE_MIN_TARGET_COVERAGE) <
        caveBlobBaseRadius(1, CAVE_MAX_TARGET_COVERAGE));

    DungeonRoom room{};
    room.type = ROOM_PUZZLE;
    TEST_ASSERT_FALSE(isCaveEligible(room));
    TEST_ASSERT_TRUE(addRoomConnection(
        room, DIR_NORTH, ROOM_CONNECTION_MIN, 0));
    TEST_ASSERT_TRUE(isCaveEligible(room));
    TEST_ASSERT_TRUE(addRoomConnection(
        room,
        DIR_EAST,
        ROOM_SIZE - 1,
        ROOM_CONNECTION_MIN + 1));
    TEST_ASSERT_TRUE(addRoomConnection(
        room,
        DIR_SOUTH,
        ROOM_CONNECTION_MAX,
        ROOM_SIZE - 1));
    TEST_ASSERT_TRUE(addRoomConnection(
        room,
        DIR_WEST,
        0,
        ROOM_CONNECTION_MAX - 1));
    TEST_ASSERT_TRUE(isCaveEligible(room));

    room.connectionCount = MAX_ROOM_CONNECTIONS + 1;
    TEST_ASSERT_FALSE(isCaveEligible(room));
}

void test_caves_support_one_through_four_connected_entries()
{
    for (uint8_t connectionCount = 1;
         connectionCount <= MAX_ROOM_CONNECTIONS;
         connectionCount++)
    {
        for (uint8_t sample = 0; sample < 8; sample++)
        {
            DungeonRoom room{};
            room.type = ROOM_PUZZLE;
            room.shape = SHAPE_CAVE;

            TEST_ASSERT_TRUE(addRoomConnection(
                room,
                DIR_NORTH,
                static_cast<uint8_t>(
                    ROOM_CONNECTION_MIN + sample % 4),
                0));

            if (connectionCount >= 2)
            {
                TEST_ASSERT_TRUE(addRoomConnection(
                    room,
                    DIR_EAST,
                    ROOM_SIZE - 1,
                    static_cast<uint8_t>(
                        ROOM_CONNECTION_MIN + (sample + 2) % 7)));
            }

            if (connectionCount >= 3)
            {
                TEST_ASSERT_TRUE(addRoomConnection(
                    room,
                    DIR_SOUTH,
                    static_cast<uint8_t>(
                        ROOM_CONNECTION_MAX - sample % 4),
                    ROOM_SIZE - 1));
            }

            if (connectionCount >= 4)
            {
                TEST_ASSERT_TRUE(addRoomConnection(
                    room,
                    DIR_WEST,
                    0,
                    static_cast<uint8_t>(
                        ROOM_CONNECTION_MAX - (sample + 1) % 7)));
            }

            generateRoom(room);

            TEST_ASSERT_EQUAL(SHAPE_CAVE, room.shape);
            TEST_ASSERT_TRUE(validateRoomConnectivity(room));
            TEST_ASSERT_TRUE(caveHasOpenCombatArea(room));
            TEST_ASSERT_FALSE(caveHasLongOneTileTunnel(room));
            TEST_ASSERT_TRUE(
                getRoomFloorCoveragePercent(room) >=
                CAVE_MIN_ACCEPTED_COVERAGE);
            TEST_ASSERT_TRUE(
                getRoomFloorCoveragePercent(room) <=
                CAVE_MAX_ACCEPTED_COVERAGE);
            assertEveryConnectionEntryIsFloor(room);
            assertBoundaryContainsOnlyWallsAndDoors(room);
        }
    }
}

void test_cave_choke_validator_allows_a_short_neck_only()
{
    DungeonRoom shortNeck{};
    fillRoom(shortNeck, TILE_WALL);
    TEST_ASSERT_TRUE(carveRectangle(shortNeck, 2, 3, 4, 5));
    TEST_ASSERT_TRUE(carveRectangle(shortNeck, 7, 3, 4, 5));
    TEST_ASSERT_TRUE(carveFloorTile(shortNeck, 6, 5));
    TEST_ASSERT_FALSE(caveHasLongOneTileTunnel(shortNeck));

    DungeonRoom longTunnel{};
    fillRoom(longTunnel, TILE_WALL);
    TEST_ASSERT_TRUE(carveRectangle(longTunnel, 1, 3, 4, 5));
    TEST_ASSERT_TRUE(carveRectangle(longTunnel, 9, 3, 4, 5));
    TEST_ASSERT_TRUE(carveCorridorSegment(
        longTunnel, 5, 5, 8, 5, 1));
    TEST_ASSERT_TRUE(caveHasLongOneTileTunnel(longTunnel));
}

void test_invalid_cave_request_uses_existing_full_room_fallback()
{
    DungeonRoom room{};
    room.type = ROOM_PUZZLE;
    room.shape = SHAPE_CAVE;

    generateRoom(room);

    TEST_ASSERT_EQUAL(SHAPE_SQUARE, room.shape);
    TEST_ASSERT_TRUE(validateRoomConnectivity(room));
    TEST_ASSERT_EQUAL_UINT16(
        ROOM_INTERIOR_AREA,
        countTiles(room, TILE_FLOOR));
}

void test_entrance_uses_fixed_hallway_and_paired_alcoves()
{
    DungeonRoom room{};
    room.type = ROOM_ENTRANCE;
    room.shape = SHAPE_CAVE; // Entrance generation must ignore random shapes.
    room.east = 1;

    populateRoomConnections(room);
    generateRoom(room);

    TEST_ASSERT_EQUAL(SHAPE_ENTRANCE, room.shape);
    TEST_ASSERT_EQUAL_UINT8(1, room.connectionCount);
    const RoomConnection* east = getRoomConnection(room, DIR_EAST);
    TEST_ASSERT_NOT_NULL(east);
    TEST_ASSERT_EQUAL_UINT8(ROOM_SIZE - 1, east->x);
    TEST_ASSERT_EQUAL_UINT8(ENTRANCE_EAST_CONNECTION_Y, east->y);
    TEST_ASSERT_EQUAL(
        TILE_DOOR,
        room.map.tiles[ENTRANCE_EAST_CONNECTION_Y][ROOM_SIZE - 1]);

    for (uint8_t y = ENTRANCE_HALL_Y;
         y < ENTRANCE_HALL_Y + ENTRANCE_HALL_HEIGHT;
         y++)
    {
        for (uint8_t x = ENTRANCE_HALL_X;
             x < ENTRANCE_HALL_X + ENTRANCE_HALL_WIDTH;
             x++)
            TEST_ASSERT_EQUAL(TILE_FLOOR, room.map.tiles[y][x]);
    }

    for (uint8_t y = 0; y < ENTRANCE_ALCOVE_HEIGHT; y++)
    {
        for (uint8_t x = 0; x < ENTRANCE_ALCOVE_WIDTH; x++)
        {
            TEST_ASSERT_EQUAL(
                TILE_FLOOR,
                room.map.tiles[ENTRANCE_FOUNTAIN_ALCOVE_Y + y]
                              [ENTRANCE_ALCOVE_X + x]);
            TEST_ASSERT_EQUAL(
                TILE_FLOOR,
                room.map.tiles[ENTRANCE_SERVICE_ALCOVE_Y + y]
                              [ENTRANCE_ALCOVE_X + x]);
        }
    }

    uint8_t startX = 0;
    uint8_t startY = 0;
    TEST_ASSERT_TRUE(getRoomEntryPosition(
        room, ENTRY_START, startX, startY));
    TEST_ASSERT_EQUAL_UINT8(ENTRANCE_PLAYER_START_X, startX);
    TEST_ASSERT_EQUAL_UINT8(ENTRANCE_PLAYER_START_Y, startY);
    TEST_ASSERT_EQUAL(TILE_FLOOR, room.map.tiles[startY][startX]);
    TEST_ASSERT_TRUE(isReservedContentTile(room, startX, startY));

    uint8_t exitX = 0;
    uint8_t exitY = 0;
    TEST_ASSERT_TRUE(getRoomEntryPosition(
        room, ENTRY_EAST, exitX, exitY));
    TEST_ASSERT_EQUAL(TILE_FLOOR, room.map.tiles[exitY][exitX]);
    TEST_ASSERT_TRUE(validateRoomConnectivity(room));

    TEST_ASSERT_EQUAL_UINT16(0, countTiles(room, TILE_ENEMY_START));
    TEST_ASSERT_EQUAL_UINT16(0, countTiles(room, TILE_CHEST_SPAWN));
    TEST_ASSERT_EQUAL_UINT16(0, countTiles(room, TILE_LOOT_SPAWN));
    TEST_ASSERT_EQUAL_UINT16(0, countTiles(room, TILE_TRAP));
}

void test_deeper_giant_spider_marker_has_a_two_by_two_floor_footprint()
{
    static constexpr RoomShape shapes[] = {
        SHAPE_SMALL_RECTANGLE,
        SHAPE_L,
        SHAPE_CAVE
    };

    for (RoomShape shape : shapes)
    {
        DungeonRoom room{};
        room.type = ROOM_TREASURE;
        room.shape = shape;
        TEST_ASSERT_TRUE(addRoomConnection(
            room, DIR_WEST, 0, ROOM_CONNECTION_MIN + 2));

        generateRoom(room);
        TEST_ASSERT_TRUE(placeGiantSpiderEncounter(room));

        int markerX = -1;
        int markerY = -1;
        uint8_t markerCount = 0;

        for (uint8_t y = 1; y < ROOM_SIZE - 1; y++)
        {
            for (uint8_t x = 1; x < ROOM_SIZE - 1; x++)
            {
                if (room.map.tiles[y][x] == TILE_GIANT_SPIDER_START)
                {
                    markerX = x;
                    markerY = y;
                    markerCount++;
                }
            }
        }

        TEST_ASSERT_EQUAL_UINT8(1, markerCount);
        TEST_ASSERT_TRUE(markerX > 0 && markerX < ROOM_SIZE - 2);
        TEST_ASSERT_TRUE(markerY > 0 && markerY < ROOM_SIZE - 2);
        TEST_ASSERT_EQUAL(TILE_FLOOR, room.map.tiles[markerY][markerX + 1]);
        TEST_ASSERT_EQUAL(TILE_FLOOR, room.map.tiles[markerY + 1][markerX]);
        TEST_ASSERT_EQUAL(
            TILE_FLOOR,
            room.map.tiles[markerY + 1][markerX + 1]);
        TEST_ASSERT_TRUE(validateRoomConnectivity(room));
        assertEveryConnectionEntryIsFloor(room);
    }

}

void test_rubble_patch_is_walkable_connected_and_avoids_entry_tiles()
{
    DungeonRoom room{};
    room.type = ROOM_EMPTY;
    room.shape = SHAPE_SQUARE;
    addHorizontalTestConnections(room, 7, 7);
    TEST_ASSERT_TRUE(buildRoomGeometry(room, SHAPE_SQUARE));

    const uint8_t placed = placeRubblePatch(room, 7, 7, 5);
    TEST_ASSERT_TRUE(placed > 0);
    TEST_ASSERT_TRUE(placed <= 5);
    TEST_ASSERT_EQUAL_UINT16(placed, countTiles(room, TILE_RUBBLE));
    TEST_ASSERT_TRUE(validateRoomConnectivity(room));
    assertEveryConnectionEntryIsFloor(room);
}

void test_boss_rubble_is_mandatory_deliberate_and_connected()
{
    DungeonRoom room{};
    room.type = ROOM_BOSS;
    room.shape = SHAPE_SQUARE;
    addHorizontalTestConnections(room, 7, 7);
    TEST_ASSERT_TRUE(buildRoomGeometry(room, SHAPE_SQUARE));

    const uint8_t placed = populateBossRubbleTerrain(room);
    TEST_ASSERT_TRUE(placed >= 2);
    TEST_ASSERT_EQUAL_UINT16(placed, countTiles(room, TILE_RUBBLE));
    TEST_ASSERT_TRUE(validateRoomConnectivity(room));
    assertEveryConnectionEntryIsFloor(room);
}

void test_pillar_is_wall_like_not_difficult_terrain()
{
    TEST_ASSERT_FALSE(isDungeonFloorTerrain(TILE_PILLAR));
    TEST_ASSERT_TRUE(isWallLikeDungeonTile(TILE_PILLAR));
    TEST_ASSERT_TRUE(isTileBlockingSight(TILE_PILLAR));
    TEST_ASSERT_EQUAL_UINT8(1, getTerrainMovementCost(TILE_PILLAR));
}

void test_square_and_row_pillar_layouts_are_exact_and_connected()
{
    DungeonRoom square{};
    square.type = ROOM_EMPTY;
    square.shape = SHAPE_SQUARE;
    addHorizontalTestConnections(square, 7, 7);
    TEST_ASSERT_TRUE(buildRoomGeometry(square, SHAPE_SQUARE));
    TEST_ASSERT_EQUAL_UINT8(
        4, placePillarLayout(square, PILLAR_LAYOUT_SQUARE, 7, 7));
    TEST_ASSERT_EQUAL_UINT16(4, countTiles(square, TILE_PILLAR));
    TEST_ASSERT_TRUE(validateRoomConnectivity(square));
    assertEveryConnectionEntryIsFloor(square);

    DungeonRoom horizontal{};
    horizontal.type = ROOM_EMPTY;
    horizontal.shape = SHAPE_SQUARE;
    addHorizontalTestConnections(horizontal, 5, 9);
    TEST_ASSERT_TRUE(buildRoomGeometry(horizontal, SHAPE_SQUARE));
    TEST_ASSERT_EQUAL_UINT8(4, placePillarLayout(
        horizontal, PILLAR_LAYOUT_ROW_HORIZONTAL, 7, 7));
    TEST_ASSERT_EQUAL(TILE_PILLAR, horizontal.map.tiles[7][4]);
    TEST_ASSERT_EQUAL(TILE_PILLAR, horizontal.map.tiles[7][10]);
    TEST_ASSERT_TRUE(validateRoomConnectivity(horizontal));

    DungeonRoom vertical{};
    vertical.type = ROOM_EMPTY;
    vertical.shape = SHAPE_SQUARE;
    addHorizontalTestConnections(vertical, 5, 9);
    TEST_ASSERT_TRUE(buildRoomGeometry(vertical, SHAPE_SQUARE));
    TEST_ASSERT_EQUAL_UINT8(4, placePillarLayout(
        vertical, PILLAR_LAYOUT_ROW_VERTICAL, 7, 7));
    TEST_ASSERT_EQUAL(TILE_PILLAR, vertical.map.tiles[4][7]);
    TEST_ASSERT_EQUAL(TILE_PILLAR, vertical.map.tiles[10][7]);
    TEST_ASSERT_TRUE(validateRoomConnectivity(vertical));
}

void test_hexagonal_pillar_layout_has_six_open_centered_points()
{
    DungeonRoom room{};
    room.type = ROOM_BOSS;
    room.shape = SHAPE_SQUARE;
    addHorizontalTestConnections(room, 7, 7);
    TEST_ASSERT_TRUE(buildRoomGeometry(room, SHAPE_SQUARE));

    TEST_ASSERT_EQUAL_UINT8(
        6, placePillarLayout(room, PILLAR_LAYOUT_HEXAGON, 7, 7));
    TEST_ASSERT_EQUAL_UINT16(6, countTiles(room, TILE_PILLAR));
    TEST_ASSERT_EQUAL(TILE_FLOOR, room.map.tiles[7][7]);
    TEST_ASSERT_TRUE(validateRoomConnectivity(room));
}

void test_pillar_layout_rejects_features_rubble_and_disconnection()
{
    DungeonRoom featureRoom{};
    featureRoom.type = ROOM_EMPTY;
    featureRoom.shape = SHAPE_SQUARE;
    TEST_ASSERT_TRUE(buildRoomGeometry(featureRoom, SHAPE_SQUARE));
    featureRoom.map.tiles[4][4] = TILE_RUBBLE;
    TEST_ASSERT_EQUAL_UINT8(
        0, placePillarLayout(featureRoom, PILLAR_LAYOUT_SQUARE, 7, 7));
    TEST_ASSERT_EQUAL(TILE_RUBBLE, featureRoom.map.tiles[4][4]);

    DungeonRoom corridor{};
    corridor.type = ROOM_EMPTY;
    corridor.shape = SHAPE_SQUARE;
    fillRoom(corridor, TILE_WALL);
    TEST_ASSERT_TRUE(carveCorridorSegment(corridor, 1, 7, 13, 7, 1));
    TEST_ASSERT_TRUE(validateRoomConnectivity(corridor));
    TEST_ASSERT_EQUAL_UINT8(0, placePillarLayout(
        corridor, PILLAR_LAYOUT_ROW_HORIZONTAL, 7, 7));
    TEST_ASSERT_EQUAL_UINT16(0, countTiles(corridor, TILE_PILLAR));
    TEST_ASSERT_TRUE(validateRoomConnectivity(corridor));
}

void test_pillar_eligibility_and_probability_are_conservative()
{
    DungeonRoom large{};
    large.type = ROOM_COMBAT;
    large.shape = SHAPE_SQUARE;
    addHorizontalTestConnections(large, 7, 7);
    TEST_ASSERT_TRUE(buildRoomGeometry(large, SHAPE_SQUARE));
    TEST_ASSERT_TRUE(isRoomEligibleForPillars(large));
    TEST_ASSERT_EQUAL_UINT8(0, populatePillarTerrain(
        large, PILLAR_ROOM_CHANCE_PERCENT, PILLAR_LAYOUT_SQUARE));
    TEST_ASSERT_EQUAL_UINT8(4, populatePillarTerrain(
        large, PILLAR_ROOM_CHANCE_PERCENT - 1, PILLAR_LAYOUT_SQUARE));

    DungeonRoom small{};
    small.type = ROOM_EMPTY;
    small.shape = SHAPE_SMALL_RECTANGLE;
    fillRoom(small, TILE_WALL);
    TEST_ASSERT_TRUE(carveRectangle(small, 4, 4, 6, 6));
    TEST_ASSERT_FALSE(isRoomEligibleForPillars(small));
    TEST_ASSERT_EQUAL_UINT8(
        0, populatePillarTerrain(small, 0, PILLAR_LAYOUT_SQUARE));
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_value_initialized_room_has_no_connections);
    RUN_TEST(test_cardinal_edge_connections_are_accepted);
    RUN_TEST(test_connections_on_wrong_edges_are_rejected);
    RUN_TEST(test_connection_capacity_fails_without_overwriting_entries);
    RUN_TEST(test_clear_removes_existing_connections);
    RUN_TEST(test_random_connection_offsets_stay_in_safe_range);
    RUN_TEST(test_neighbors_populate_safe_variable_wall_coordinates);
    RUN_TEST(test_room_generation_places_doors_from_connections);
    RUN_TEST(test_entry_positions_use_destination_connections_and_walkable_tiles);
    RUN_TEST(test_full_rectangle_remains_connected_and_full_sized);
    RUN_TEST(test_small_rectangles_stay_bounded_connected_and_playable);
    RUN_TEST(test_l_rooms_stay_bounded_connected_and_playable);
    RUN_TEST(test_connection_helper_carves_a_reachable_manhattan_approach);
    RUN_TEST(test_invalid_shape_falls_back_to_full_rectangle);
    RUN_TEST(test_production_shape_selection_is_conservative_and_bounded);
    RUN_TEST(test_winding_corridor_eligibility_requires_two_safe_connections);
    RUN_TEST(test_corridor_width_selection_and_segment_carving);
    RUN_TEST(test_winding_paths_have_meaningful_orthogonal_bends);
    RUN_TEST(test_winding_corridors_connect_stored_doors_and_entries);
    RUN_TEST(test_invalid_winding_corridor_requests_fall_back_safely);
    RUN_TEST(test_winding_room_content_markers_remain_on_connected_interior_floor);
    RUN_TEST(test_oversized_encounter_templates_now_place_two_monsters);
    RUN_TEST(test_empty_room_has_no_encounter_markers);
    RUN_TEST(test_combat_and_ambush_markers_use_distinct_placement_biases);
    RUN_TEST(test_cave_parameter_selection_and_eligibility_are_bounded);
    RUN_TEST(test_caves_support_one_through_four_connected_entries);
    RUN_TEST(test_cave_choke_validator_allows_a_short_neck_only);
    RUN_TEST(test_invalid_cave_request_uses_existing_full_room_fallback);
    RUN_TEST(test_entrance_uses_fixed_hallway_and_paired_alcoves);
    RUN_TEST(test_deeper_giant_spider_marker_has_a_two_by_two_floor_footprint);
    RUN_TEST(test_rubble_patch_is_walkable_connected_and_avoids_entry_tiles);
    RUN_TEST(test_boss_rubble_is_mandatory_deliberate_and_connected);
    RUN_TEST(test_pillar_is_wall_like_not_difficult_terrain);
    RUN_TEST(test_square_and_row_pillar_layouts_are_exact_and_connected);
    RUN_TEST(test_hexagonal_pillar_layout_has_six_open_centered_points);
    RUN_TEST(test_pillar_layout_rejects_features_rubble_and_disconnection);
    RUN_TEST(test_pillar_eligibility_and_probability_are_conservative);
    UNITY_END();
}

void loop()
{
}
