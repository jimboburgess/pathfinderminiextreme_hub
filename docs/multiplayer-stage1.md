# Multiplayer Stage 1 Architecture

## Current Project Audit

### Game state

- `gameState`, the persistent `player` character, `dungeon`, and `combat` are global runtime objects.
- Map gameplay operates on either the forest entity array or the currently loaded dungeon room through `map/activemap.cpp`.
- Single-player remains the default. Failure to initialize ESP-NOW does not block startup or any existing gameplay path.

### Dungeon and rooms

- A dungeon contains 7–12 `DungeonRoom` records and a logical cardinal graph. Each room has graph coordinates and north/east/south/west neighbor indices.
- A room owns a 15x14 tile map plus connections, traps, suspicions, furniture, fountain state, NPC spawn state, and puzzle state.
- Only one room has full `Entity` objects loaded. Inactive rooms retain compact `PersistentEntity` snapshots in `DungeonRoomRuntime`.
- Room loading persists the current non-player entities, clears the active array, inflates the destination snapshot or initializes its spawns, and then attaches the global player character.
- The player is deliberately excluded from room persistence. Runtime character changes are copied back to the global `player` before room swaps or returning to town.

### Entities and player ownership

- `Entity` contains runtime pointers, a full `Character`, rendering state, perception caches, loot, and transient turn state. It is not network-safe.
- `Character` contains an Arduino `String` and large inventory/equipment/ability structures. It must never be transmitted as raw memory.
- The project previously found the first active `ENTITY_PLAYER` and treated that entity as the local player. `TEAM_PLAYER` represented allegiance and could not distinguish local and allied players.
- Stage 1 adds `ownerPlayerID` and owner-aware helpers while leaving existing first-player lookups unchanged for single-player compatibility.
- Stage 2 must change local input, facing cursors, camera behavior, and local-turn checks to use `isLocalPlayerEntity()` rather than treating every `ENTITY_PLAYER` as local.

### Movement

- Player movement currently validates and mutates the active player entity directly in `map/playermovement.cpp`.
- The same call also resolves collisions, terrain costs, traps, puzzles, furniture pushes, combat detection, and room transitions.
- Stage 2 should extract a host-callable authoritative move operation rather than duplicate or bypass these rules. Clients should send `PLAYER_MOVE_REQUEST`; only the host should call the authoritative operation.
- `PLAYER_POSITION` and `PLAYER_ROOM_CHANGE` are compact immediate corrections/results. They are not full entity snapshots and must not wait for periodic dungeon synchronization.

### Combat and initiative

- `Combat` stores direct `Entity*` pointers in a fixed initiative array and the active entity array owns those objects.
- Initiative rolls, monster AI, damage, conditions, and encounter completion are currently resolved locally.
- `isPlayerTurn()` currently returns true for any `ENTITY_PLAYER`. Shared combat must instead distinguish the current entity owner from the local `PlayerID`.
- The host will build and own the initiative order and resolve all requested actions. Clients will only submit actions for their owned character.

### Input, menus, and saves

- Buttons and encoder input are polled centrally, then dispatched by game state. Map input calls movement directly and menus invoke gameplay actions directly.
- The existing stack-based menu system is retained. Stage 1 adds a Multiplayer submenu without redesigning it.
- Saves use versioned `Preferences`/NVS records for the one global character. Dungeon runs and multiplayer sessions are runtime-only. The current saved character record does not persist a display name.

### Connectivity and memory

- Before Stage 1 the project had no Wi-Fi, Bluetooth, or ESP-NOW code.
- The board configuration enables 8 MB OPI PSRAM, but current gameplay storage does not allocate into PSRAM. PSRAM is only reported by optional diagnostics.
- The current full firmware build uses about 89% of internal RAM. Stage 1 networking therefore uses fixed compact records and a six-frame receive queue. Large future snapshots should use bounded chunking and deliberately evaluated PSRAM buffers rather than growing permanent DRAM globals.

## Layering

### Transport

`espnow_transport.*` owns Wi-Fi station mode, ESP-NOW initialization, MAC addresses, peers, sends, callbacks, and a fixed receive queue. Receive callbacks only copy frames; they never mutate session or gameplay state.

### Protocol

`network_protocol.*` owns the 14-byte common header, protocol version, packet types, sequence numbers, session ID, validation, and explicit little-endian serialization. No raw C++ object layout is transmitted.

### Session

`multiplayer_session.*` owns discovery, hosting/joining, `PlayerID` assignment, membership, heartbeats, timeouts, and session filtering. MAC addresses remain private transport/session routing details.

### Gameplay synchronization

`multiplayer_runtime.*` is the current integration seam. It refreshes the local discovery profile and availability and surfaces session notices. It does not synchronize dungeon gameplay yet.

## Session and Discovery Rules

- The host is always Player 0. Joining clients receive Player 1, 2, or 3.
- A SessionID is a nonzero 32-bit value mixed from the host device identifier, hardware RNG, and current time. Every non-discovery session packet is checked against the active SessionID.
- Devices broadcast a 24-byte discovery payload every 1.5 seconds. Entries expire after 6 seconds.
- Discovery contains a stable device-derived identifier, bounded display name, level, class, availability, hosted SessionID, and whether the party accepts players. The UI never shows MAC addresses.
- Heartbeats are sent every 2 seconds. A 7-second silence disconnects a client; loss of the host terminates the client session with `Host connection lost.` Host migration is intentionally deferred.

## Dungeon Synchronization Decision

Stage 2 should use authoritative host graph and room state, not seed-only regeneration.

The generator uses Arduino's shared global `random()` stream across graph construction, room geometry, encounters, traps, puzzles, loot, and other runtime systems. There is no isolated deterministic PRNG or complete generation-version contract. A seed alone could diverge after firmware or call-order changes. `DUNGEON_BEGIN` therefore declares `AUTHORITATIVE_GRAPH_AND_ROOM_STATE`; its generation seed/version remain useful diagnostics, but clients must receive the host graph and authoritative room snapshots/corrections.

Room synchronization should be chunked below ESP-NOW's 250-byte packet limit and incrementally cover geometry, connections, doors, entities, chests/loot, traps, puzzles, furniture/destruction, and environmental effects. Each snapshot needs an epoch/revision so stale chunks cannot overwrite newer authoritative events.

## Stage 1 Scope

Implemented:

- ESP-NOW transport and peer management
- Versioned, session-scoped packet framing
- Reserved extensible packet type set
- Automatic discovery cache and expiry
- Host/join handshake with PlayerID assignment
- Membership announcements, leave handling, heartbeat, and timeout
- Compact future movement and dungeon-begin payloads
- Multiplayer menu and status view
- Entity owner identity seam

Deferred:

- Spawning remote player entities
- Authoritative movement dispatch and correction application
- Dungeon graph/room snapshot transfer
- Shared combat, initiative, monster AI, puzzles, traps, loot, and quests
- Trading and cooperative quest contributions
- Reliability/acknowledgement for large snapshot chunks
- Host migration and multiplayer save/resume
