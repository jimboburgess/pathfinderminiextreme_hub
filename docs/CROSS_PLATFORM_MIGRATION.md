# Cross-Platform Migration Audit and Plan

## Scope and safety constraints

This document is the Phase 1 architecture audit only. It does not move source
files, change gameplay, add a Linux server, add a browser client, alter the
ESP-NOW protocol, or modify the PlatformIO configuration.

The migration must preserve the ESP32-S3 handheld as a first-class target:

- the current 240x240 ST7789 presentation;
- the encoder and two-button controls;
- the current piezo/audio behavior and sprite assets;
- current single-player and ESP-NOW multiplayer behavior;
- all current PlatformIO environments and board settings;
- existing versioned character saves;
- independent handheld operation without a Linux server.

The long-term goal is one authoritative set of C++ gameplay rules used by the
handheld and a headless Linux server. The browser and Discord Activity should
be presentation/input clients of the authoritative server, not competing
implementations of the gameplay rules.

## Repository safety record

### State before the audit

- Branch: `master`
- HEAD: `da3e8141940c60811438afad54a2cccdd1acec8a`
- Subject: `stage two on multiplayer`
- Commit date: `2026-10-09T09:11:47-04:00`
- Requested tag `esp32-before-cross-platform`: absent
- Working tree: not clean

The pre-audit worktree already contained modifications in Bell playback,
combat, dungeon runtime, display/message handling, menus, multiplayer, and
tests, plus an untracked `docs/multiplayer-stage3.md`. None of that work was
discarded, stashed, reset, overwritten, or committed by this audit.

Because the tree was not clean, no annotated `esp32-before-cross-platform` tag
was created. Tagging the current HEAD as a known-good representation of the
dirty worktree would have been inaccurate. Create the tag only after the
existing work is reviewed, committed, and verified.

The worktree was switched in place to a new branch:

```text
feature/cross-platform
```

The branch change preserved every existing tracked and untracked file. No
history was rewritten and `master` was not modified.

## Verification baseline

Verification used the sole PlatformIO environment currently defined:

```text
waveshare_esp32s3_zero_n8r8
```

### Normal firmware

Command:

```text
platformio run -e waveshare_esp32s3_zero_n8r8
```

Result: success, release mode.

- Internal RAM: 292,704 / 327,680 bytes (89.3%)
- Internal RAM remaining: 34,976 bytes
- Flash: 1,048,765 / 4,128,768 bytes (25.4%)
- Flash remaining: 3,080,003 bytes
- Compiler warnings: none emitted
- Upload: not performed
- Serial access: not performed
- Hardware testing: not performed

`platformio.ini` does not define a debug build environment or `build_type =
debug`, so no separate debug firmware build is currently available. The debug
probe list printed by PlatformIO describes supported probes, not a configured
debug build variant.

### Embedded compile-test targets

Command:

```text
platformio test -e waveshare_esp32s3_zero_n8r8 \
  --without-uploading --without-testing
```

All 26 discovered embedded test targets compiled successfully. This command
intentionally compiled but did not upload or execute tests on hardware, so
PlatformIO correctly reported zero executed test cases.

Compiled targets:

- ability resolver, audio, Bell puzzle, Brazier puzzle;
- combat awareness, conditions, dungeon tools, dungeon graph/runtime;
- encoder, entity persistence, equipment, furniture, monster HP, movement;
- multiplayer protocol, NPCs, Number Tile puzzle, progression;
- puzzle-room selection, rations, riddles, Riddle puzzle;
- room connections, scroll learning, and traps.

## Current repository architecture

The project currently contains about 44,000 lines across the C++ headers and
sources under `src/`. PlatformIO is the only configured build system. Tests are
embedded Unity targets under `test/`; many tests compile production `.cpp`
files directly and replace external functions with local stubs.

| Area | Current responsibility | Portability state |
| --- | --- | --- |
| `src/main.cpp` | Arduino lifecycle, hardware setup, update loop | ESP32-only composition root |
| `src/audio/` | command sequences, asynchronous single-channel player, LEDC output | engine portion is separable; hardware layer is ESP32-only |
| `src/characters/` | character rules, abilities, conditions, items, magic, character-sheet UI | rules are reusable; `Arduino::String`, Serial, sprites, display, and UI contaminate boundaries |
| `src/data/` | dice, entities, global game state, progression, NVS save/settings | rules are reusable; globals, Arduino RNG, TFT declaration, and Preferences are platform-specific |
| `src/dungeon/` | generation, rooms, persistence, combat, abilities, monsters, loot, traps, puzzles | largest reusable domain, but heavily coupled to globals, Arduino RNG/time, rendering, audio, messages, and menus |
| `src/forest/` | forest map/generation and entity runtime | gameplay-capable but global and coupled to UI/audio/Arduino RNG |
| `src/graphics/` | ST7789 rendering, dirty tiles, sprites, message pagination | ESP32 client presentation; assets may be shared as source material |
| `src/input/` | GPIO polling, encoder decoding, menus, inventory/riddle UI | GPIO/rendering remain ESP32; pure encoder decoder is reusable |
| `src/map/` | active-map facade, movement, LOS, awareness, interactions, effects | core concepts are reusable; active-map globals and presentation callbacks must be separated |
| `src/multiplayer/` | protocol, ESP-NOW transport/session, authoritative activity and combat sync | protocol helpers are portable; transport and current activity runtime are ESP32/global-state coupled |
| `src/town/` | shop rules/UI and timed resting | rules are reusable after UI/time separation |

### Runtime flow

The current executable is a single global game instance:

```text
Arduino setup/loop
  -> input polling and menu dispatch
  -> direct gameplay mutation
  -> multiplayer session/activity pump
  -> awareness/traps/idle/combat timers
  -> message/audio updates
  -> ST7789 redraw
```

Input, rules, effects, presentation, and transport frequently call one another
directly. This is workable for one handheld, but it prevents a headless engine
from being instantiated multiple times in one Linux process.

### Current multiplayer layering

The multiplayer code already contains an important reusable split:

```text
network_protocol.*          explicit bounded wire codecs
multiplayer_snapshot.cpp    bounded chunk and sequence helpers
espnow_transport.*          Wi-Fi/ESP-NOW/FreeRTOS transport
multiplayer_session.*       discovery, membership, heartbeats, routing
multiplayer_activity.cpp    authoritative activity, snapshots, rooms, combat,
                            game mutations, timing, UI messages, PSRAM policy
```

`network_protocol.*` uses explicit little-endian field serialization instead
of transmitting raw structs. That is the correct foundation for ESP-NOW,
WebSocket clients, mixed platforms, and golden-vector tests.

`multiplayer_activity.cpp` is approximately 4,900 lines and currently combines
portable authority rules with ESP32 allocation, the global game, transport
dispatch, snapshots, display invalidation, message UI, Arduino time/RNG, and
room inflation. It should be separated incrementally, not rewritten.

## Dependency map

### Direct platform dependencies

| Dependency | Current uses | Required boundary |
| --- | --- | --- |
| `Arduino.h` | integer aliases, `String`, `random`, `millis`, GPIO constants, Serial | replace incidental includes with standard headers; inject RNG/clock/logging; isolate `String` |
| Adafruit GFX/ST7789 | `game.h`, display, room drawing, menus, sheets, creation UI | ESP32 renderer only |
| SPI and GPIO | `main.cpp`, `buttons.cpp` | ESP32 device/input adapter |
| LEDC | `audio.cpp` | ESP32 audio sink; keep portable audio command builder/player state separate |
| Preferences/NVS | `savegame.cpp`, `settings.cpp` | ESP32 persistence adapter |
| Wi-Fi/ESP-NOW | `espnow_transport.cpp` | ESP32 transport implementation |
| FreeRTOS queues | ESP-NOW callback receive queue | ESP32 transport implementation |
| `esp_random` | session/activity IDs and startup seed | RNG/identity service |
| `ps_malloc`/PSRAM | multiplayer character, combat, and room-effect storage | ESP32-specific storage policy |
| `PROGMEM` | tile and sprite arrays | ESP32 asset-storage macro/renderer |

### Indirect dependency contamination

Several otherwise portable files become Arduino/TFT-dependent through common
headers:

- `data/game.h` includes `Adafruit_ST7789.h` solely to expose global `tft`.
- `characters/characters.h` includes Arduino for `String` and integer types.
- `characters/items.h`, `graphics/tiles.h`, and `graphics/sprites.h` include
  Arduino despite mostly defining IDs, constants, and data declarations.
- `dungeon/dungeon.h` includes Arduino and `data/game.h`, spreading platform
  dependencies through most dungeon code.
- `data/entities.h` combines authoritative entity data with sprite pointers
  and includes graphics declarations.

Removing these incidental header dependencies is a high-value early task, but
must be done without changing layout, enum values, or ESP32 behavior.

## Code reusable on Linux without gameplay changes

### Native-ready now

The following code has no required Arduino, ESP-IDF, display, audio, GPIO, or
ESP-NOW implementation dependency and can be compiled in a native target with
little or no source change:

- `src/multiplayer/multiplayer_types.h`
- `src/multiplayer/network_protocol.h/.cpp`
- `src/multiplayer/multiplayer_activity.h`'s bounded snapshot data/helpers
- `src/multiplayer/multiplayer_snapshot.cpp`
- `src/dungeon/riddles.h/.cpp`
- `src/input/encoderdecoder.h`
- value-only headers such as `dungeon/turns.h`, `characters/abilities.h`, and
  `characters/conditions.h`, subject to their include graph

The existing multiplayer protocol test is already close to a native test: the
production codec and snapshot helper code are portable, while the test itself
uses embedded Unity/Arduino only because that is the current test harness.

### Portable in intent, but blocked by coupling

These systems contain substantial reusable rules and should eventually form
the shared engine, but do not currently compile as independent native modules:

- character stats, progression, conditions, equipment, inventory, spells;
- entity traits, spawning, compact entity persistence;
- dungeon graph and room geometry generation;
- monster definitions and behavior decisions;
- combat calculations, saving throws, damage, conditions, and ability effects;
- movement cost, collision, line of sight, map effects, traps;
- loot tables and item generation;
- Bell, Number Tile, Brazier, and Riddle puzzle state/rules;
- room state, furniture, fountains, NPC state, and completion;
- authoritative multiplayer movement, room transitions, messages, and combat.

The blockers are not the gameplay algorithms themselves. They are shared
headers, global state, direct presentation/audio callbacks, Arduino RNG/time,
raw pointers, and concrete transport/storage access.

## Global and singleton state audit

The following state prevents more than one simultaneous game session:

- `gameState`, `player`, movement direction, previous position, and redraw
  globals in `data/game.cpp`;
- global `Dungeon dungeon` and its active `entities` pointer;
- global `Combat combat` containing an initiative array of raw `Entity*`;
- global forest map, `forestEntities`, and entity count;
- global `activeMapEffects`;
- global active-map selection based on `gameState`;
- static awareness and monster-idle timers/flags;
- static interaction selections and raw object pointers;
- global/static menus, menu stack state, character-sheet state, inventory UI,
  shop buffers, and riddle-menu pointers;
- static message buffers and pagination timer;
- global display dirty-tile state;
- global audio playback state and runtime tone buffer;
- singleton `multiplayerTransport` and `multiplayerSession`;
- singleton multiplayer `ActivityRuntime` and three process-wide PSRAM-backed
  pointers for participant characters, combat, and room effects.

Large current symbols from the ESP32 firmware ELF demonstrate why the
handheld must never duplicate the existing world object:

| Symbol | Size |
| --- | ---: |
| `Dungeon dungeon` | 218,996 bytes |
| `forestEntities[16]` | 16,128 bytes |
| `Character player` | 908 bytes |
| `activeMapEffects[8]` | 704 bytes |
| `Combat combat` | 232 bytes |
| multiplayer activity runtime | 760 bytes |
| `MultiplayerSession` | 780 bytes |
| ESP-NOW receive queue storage | 1,548 bytes |
| runtime audio command buffer | 140 bytes |

The Linux server needs one explicit `GameSession`/`WorldSession` instance per
activity. The ESP32 adapter should continue constructing exactly one instance,
preferably in existing static storage, so the refactor does not duplicate
memory or require heap allocation.

## Data model and serialization audit

### Safe to share

- Stable IDs and explicitly sized protocol fields are suitable shared data.
- `network_protocol.*` explicitly writes and reads little-endian values,
  validates lengths and enum ranges, and caps ESP-NOW packets at 250 bytes.
- Protocol version 4 and snapshot/generation versions provide migration gates.
- `PlayerID`, room IDs, movement sequences, stable combatant references, and
  authoritative room revisions are appropriate cross-platform identifiers.
- Puzzle states and compact room-owned state use bounded fixed-size arrays.
- `PersistentEntity` is bounded and already separates inactive state from
  runtime pointers, although its in-memory layout should not become a wire
  format without an explicit codec.

### Not safe to share as raw bytes

- `Character` contains Arduino `String`.
- `Entity` contains pointers to `Monster` and sprite data plus transient
  rendering/visibility fields.
- `Combat` contains raw `Entity*` initiative and targeting pointers.
- `Dungeon` contains an active-entity pointer and a large union with runtime
  storage assumptions.
- Native enum size, struct padding, pointer width, and endianness differ from
  ESP32 assumptions.

### Existing character saves

ESP32 character saves use `Preferences` and write a raw `SavedCharacter` blob.
The format is guarded by magic, byte length, and versions 1 through 9, with
explicit migration structs for earlier layouts. The saved subset deliberately
excludes runtime pointers and the Arduino `String` name, but it still depends
on ESP32 compiler layout and enum sizes.

Preservation rules:

1. Keep the current ESP32 NVS reader and all legacy layouts intact.
2. Do not reorder or renumber persisted enums or item/tile/ability IDs.
3. Keep append-only ID policies where already documented.
4. Add compile-time size checks and golden saved blobs before changing common
   value types.
5. If a portable save format is later required, add a new explicit field-wise
   codec alongside the NVS format. Do not silently reinterpret old NVS bytes on
   Linux or replace the handheld format without separate approval.

## Proposed shared-engine boundaries

### 1. Portable value model

Own IDs, enums, compact value structs, fixed capacities, dungeon/room state,
character stats, items, abilities, conditions, turn state, and stable entity
references. It must not include TFT, sprites, GPIO, audio, transport, NVS, or
Arduino `String`.

Avoid changing runtime layout merely to make types aesthetically cleaner. In
particular, replace `String` only through a compatibility plan with size and
save regression tests. A bounded name value used by the protocol is safer than
putting `std::string` into every embedded `Character`.

### 2. Session-owned authoritative state

Introduce an explicit aggregate over time, initially as a facade around the
existing globals:

```text
GameSession
  character/player records
  dungeon and forest state
  active map/room selection
  combat state
  map effects
  authoritative message sequence/events
  per-session RNG and clock references
```

The first ESP32 version should bind the facade to existing global storage. Only
after tests cover behavior should functions accept `GameSession&` directly.
The Linux server can then allocate one independent instance per activity.

UI state, audio playback state, renderer caches, GPIO state, nearby ESP-NOW
discovery, and browser connection state do not belong in `GameSession`.

### 3. Platform services

Use narrow services rather than platform calls from rule code:

- `RandomSource`: bounded integer rolls;
- `MonotonicClock`: current monotonic milliseconds;
- `GameEventSink`: gameplay messages, sound cues, dirty/redraw hints;
- `Storage`: platform save/settings operations;
- `Transport`: bounded inbound/outbound frames or messages;
- optional `Logger`: diagnostics excluded or compiled out on ESP32 release.

On ESP32, prefer a single static service table or lightweight references over
per-object virtual interfaces, `std::function`, dynamic allocation, or STL
containers. Existing Arduino wrappers should remain the default adapter so RNG
call order and timing behavior do not change during extraction.

### 4. Commands and events

The authoritative engine should consume validated commands and emit bounded
events/state deltas:

```text
MoveCommand, InteractCommand, CombatActionCommand, Menu-independent choice
  -> authoritative rule execution
  -> PositionChanged, RoomChanged, DamageApplied, TurnAdvanced,
     PuzzleChanged, MessagePublished, SoundRequested
```

ESP32 input maps buttons/menus to commands. The Linux server maps WebSocket and
ESP-NOW requests to the same commands. Renderers consume state/events but do
not decide outcomes.

### 5. Stable references

Cross-room multiplayer cannot depend on raw pointers. The existing
`MultiplayerCombatantRef` design—kind, player ID, room ID, entity slot—is the
right model. Shared combat and interaction code should resolve a stable handle
through the owning `GameSession` only for the duration of an operation.

## Platform-specific responsibilities

### ESP32 handheld

Keep these platform-specific:

- Arduino `setup()`/`loop()` composition and update scheduling;
- ST7789/Adafruit rendering, dirty tiles, 240x240 menus, and sprite drawing;
- encoder/button GPIO and debounce;
- LEDC piezo output and current sound queue policy;
- Preferences/NVS saves and settings;
- Wi-Fi station mode, ESP-NOW callbacks, peers, and FreeRTOS receive queue;
- PSRAM allocation decisions and hardware diagnostics.

### Linux authoritative server

The server should own:

- process lifecycle, configuration, logging, metrics, and shutdown;
- a registry of independent `GameSession` instances;
- monotonic scheduling and session timeouts;
- secure WebSocket connection management and identity/session admission;
- persistence appropriate to the server, separate from ESP32 NVS;
- transport bridging and protocol-version/capability negotiation;
- authoritative command validation and event broadcast.

Turn-based gameplay does not require a high-frequency simulation loop. A small
event pump using monotonic deadlines is sufficient and avoids tying rule
resolution to a render frame rate.

### Browser and Discord Activity

The browser should initially be a TypeScript/JavaScript client, not a second
game engine. It should:

- render a larger pixel-art view from authoritative snapshots/deltas;
- map keyboard, pointer, controller, and touch input to commands;
- show menus, messages, combat order, and per-player room state;
- connect to the Linux server over secure WebSocket;
- maintain no authority over dice, movement validation, attacks, spells,
  damage, puzzles, loot, or monster behavior.

Use one web application bundle for standalone browser and Discord Activity
deployment. Put Discord SDK/bootstrap, participant identity, invite/deep-link,
and Activity lifecycle code behind a thin host adapter. The core web renderer
and network client should not import Discord APIs directly. Select exact SDK
versions and authentication details during the implementation phase against
the then-current official Discord documentation.

WebAssembly is optional later for deterministic previews or offline tools, but
is not required for the first browser client and must not become a second
authoritative simulation.

## Multiplayer preservation plan

The current Stage 3 design must remain the behavioral contract:

- every player retains independent room, position, visibility, movement, and
  interaction state;
- a room transition moves only the requesting player;
- combat initiative is activity-wide and includes participating players in
  other rooms without teleporting them;
- stable player/room/entity identities replace pointers on the wire;
- only the authority resolves movement, attacks, spells, damage, dice, traps,
  puzzles, loot, and interactions;
- shared gameplay messages are ordered and synchronized;
- only the local room is inflated/rendered on a handheld;
- the host/server persists authoritative inactive-room state;
- ESP-NOW remains supported, while WebSocket is another transport/client path.

Do not make the common engine assume reliable ordered delivery merely because
WebSocket provides it. Keep protocol sequencing, idempotency, snapshot epochs,
and authoritative corrections because ESP-NOW and future mixed-platform relay
paths still need them.

The current `MultiplayerSession` is coupled to `EspNowTransport` and MAC-like
six-byte addresses. Split transport only after the wire codecs are under native
golden-vector tests. Session membership and gameplay authority can then consume
an opaque connection identifier while the ESP32 adapter retains peer/MAC
routing.

## Recommended directory structure

Do not move the current tree in the first implementation stage. Preserve
PlatformIO's existing `src/` and `test/` behavior while adding native build
infrastructure beside it.

### Transitional layout

```text
platformio.ini                 unchanged
src/                           existing authoritative ESP32 source tree
test/                          existing embedded compile-test targets
CMakeLists.txt                 native build entry, initially leaf modules only
cmake/                         native build helpers
tests/native/                  native unit/golden-vector tests
server/                        empty until shared session seams exist
web/                           empty until server protocol is stable
platform/esp32/                new adapters only; no mass move
docs/
```

### Eventual layout

After modules are genuinely independent and dual-built:

```text
shared/include/pfme/           portable public headers
shared/src/                    portable value model, rules, sessions, codecs
platform/esp32/                hardware, renderer, audio, input, NVS, ESP-NOW
server/                        Linux composition, sessions, WebSocket, storage
web/                           browser renderer/client and Discord adapter
tests/native/                  portable tests, replays, protocol vectors
test/                          PlatformIO embedded tests retained
src/                           transitional wrappers/entry until final cleanup
```

Move one module only after both PlatformIO and native builds consume the same
file successfully. A repository-wide rename would create review noise, break
include paths, and make ESP32 regressions difficult to isolate.

## Native Linux build strategy

1. Add CMake without altering PlatformIO.
2. Start with a `pfme_protocol`/`pfme_portable_leaf` library containing only
   the already native-ready protocol, snapshot, riddle, and encoder code.
3. Compile shared code at the oldest language level required by the ESP32
   toolchain, or enforce an explicitly tested common subset. Server-only code
   may use a newer standard behind the boundary.
4. Build tests as normal linked translation units; do not depend on direct
   inclusion of production `.cpp` files in the long term.
5. Introduce fake RNG, clock, event sink, map, and transport implementations
   for deterministic native tests.
6. Add the headless executable only after a session-owned rules slice exists.
7. Keep the WebSocket library behind a server transport adapter so the shared
   engine does not depend on a networking framework.
8. Enable strict warnings on native code, but do not globally change ESP32
   compiler flags until warning differences are reviewed.

No native target should include Adafruit, Arduino, Preferences, Wi-Fi,
ESP-NOW, FreeRTOS, sprite arrays, or menu/rendering code.

## Testing and regression strategy

### Continuous build gates

Every migration stage should require:

- the normal Waveshare PlatformIO firmware build;
- all PlatformIO embedded compile-test targets;
- native Linux configure/build/unit tests;
- `git diff --check`;
- reported ESP32 RAM and flash deltas against this baseline;
- no upload or hardware dependency in CI.

Add a configured debug environment only in a separately reviewed change; do
not infer debug coverage from probe availability.

### Portable rule tests

- deterministic fake RNG for exact dice and generation paths;
- fake monotonic clock for combat, trap, message, idle, and timeout behavior;
- command/event tests for movement, interactions, combat, spells, loot, and
  every puzzle;
- multi-session tests proving two `GameSession` instances cannot affect one
  another;
- cross-room multiplayer tests preserving independent player locations and
  activity-wide initiative;
- persistence tests for room leave/re-entry and activity resume;
- replay tests that run a recorded command stream to an expected state hash.

### Serialization tests

- golden byte vectors for every network packet on embedded C++, Linux C++, and
  TypeScript;
- malformed/truncated/oversized decoder tests and native fuzzing;
- protocol-version mismatch tests;
- snapshot chunk ordering, duplication, stale epoch, and retry tests;
- checked-in legacy NVS save fixtures for versions 1-9 before touching common
  data layouts;
- compile-time checks for critical enum values and embedded structure sizes.

### Browser tests

- codec compatibility against the C++ golden vectors;
- reducer/state-view tests for snapshots and events;
- input-to-command tests;
- WebSocket reconnect/resnapshot tests;
- end-to-end standalone and Discord-host-adapter smoke tests using the same web
  bundle.

## ESP32 memory and performance risks

Internal RAM is already at 89.3%, so RAM—not flash—is the primary constraint.

High-risk changes include:

- creating a second `Dungeon` or full entity world;
- replacing fixed arrays with `std::vector`, `std::string`, maps, queues, or
  hidden allocator use on the handheld;
- adding per-entity virtual interfaces or `std::function` callbacks;
- retaining multiple network snapshots or retransmission packets;
- moving PSRAM-backed multiplayer state into internal RAM;
- increasing ESP-NOW receive queue depth without measuring the cost;
- expanding `Entity`, `Character`, `PersistentEntity`, `DungeonRoomRuntime`, or
  per-room effect storage;
- adding server/web dependencies to the PlatformIO link;
- changing header structure in a way that duplicates large `const` tables;
- broad wrappers/templates that cause flash-heavy duplicate instantiations;
- increasing stack buffers in combat, snapshots, rendering, or generation.

Mitigations:

- retain fixed capacities and bounded buffers on ESP32;
- keep one static handheld `GameSession` backed by existing storage;
- pass references/views instead of copying rooms, dungeons, or entities;
- use stable compact handles rather than pointer-rich duplicates;
- preserve current PSRAM allocation points until separately profiled;
- measure ELF symbols and PlatformIO size after every stage;
- fail builds if internal RAM exceeds an agreed ceiling or grows unexpectedly;
- preserve `PROGMEM` asset placement and avoid loading browser assets into the
  firmware.

## Small, independently testable migration stages

### Stage 0 - Safety baseline

Completed by this audit: preserve the dirty worktree, create
`feature/cross-platform`, record HEAD/build/memory, and document boundaries.
Create the safety tag only after current changes are committed and reverified.

### Stage 1 - Native leaf build

Add CMake and native tests for `network_protocol`, snapshot helpers, riddles,
and encoder decoding without moving source or modifying PlatformIO. Add golden
protocol vectors. No gameplay behavior changes.

### Stage 2 - Header hygiene

Replace incidental Arduino includes with `<cstdint>/<cstddef>` in leaf/value
headers. Remove the TFT declaration from `data/game.h` behind an ESP32 display
header. Preserve every enum value and measured structure size.

### Stage 3 - RNG, time, logging, and event seams

Introduce narrow adapters with Arduino-backed defaults. Convert one subsystem
at a time, beginning with dice/generation helpers already covered by tests.
Do not change the RNG algorithm/call order on ESP32.

### Stage 4 - Session facade over existing globals

Define `GameSession` views/references over current `dungeon`, `combat`, player,
forest, and map-effect storage. Keep compatibility wrappers so existing calls
and the handheld layout remain unchanged.

### Stage 5 - Extract pure rules vertically

Move one tested slice at a time: character/equipment, movement/LOS, puzzles,
loot, ability resolution, then combat. Replace direct display/audio/message
calls with bounded events while ESP32 adapters reproduce existing behavior.

### Stage 6 - Stable handles and authoritative command API

Generalize existing `PlayerID` and combatant references inside the engine.
Expose authoritative movement, interaction, and combat commands without
changing current ESP-NOW behavior.

### Stage 7 - Separate session logic from ESP-NOW

Keep `network_protocol` shared. Introduce an opaque transport interface and
adapt the current ESP-NOW implementation. Preserve packet bounds,
acknowledgements, retries, independent rooms, and global initiative.

### Stage 8 - Headless Linux server

Create a process that owns multiple independent sessions, accepts commands,
runs authority rules, and emits snapshots/events. Add WebSocket only at this
composition layer.

### Stage 9 - Browser frontend

Implement the larger pixel-art renderer, menus, command input, protocol codec,
and reconnect/resnapshot flow against the server. Keep rules server-side.

### Stage 10 - Discord Activity and mixed clients

Wrap the same web bundle with Discord-specific bootstrap/authentication. Add
capability/version negotiation and routing so browser and future handheld
gateway clients can participate without changing authority rules.

### Stage 11 - Optional directory consolidation

Only after both targets compile and tests are stable, move proven portable
modules into `shared/` and hardware adapters into `platform/esp32/` in small
commits. Preserve compatibility forwarding headers while includes migrate.

## Recommended first implementation step

Implement Stage 1 only:

1. add a root `CMakeLists.txt` that does not affect PlatformIO;
2. build the existing unmodified `network_protocol.cpp`,
   `multiplayer_snapshot.cpp`, and `riddles.cpp` as native libraries;
3. port their tests to a native test executable;
4. add checked-in golden protocol byte vectors shared with a minimal
   TypeScript codec test fixture;
5. run the existing firmware build and all 26 embedded compile-test targets in
   the same validation script.

This proves dual-toolchain operation and wire compatibility without touching
gameplay state, display, saves, ESP-NOW behavior, or source layout. It creates
the safest foundation for reviewing the first actual portability changes.

## Phase 1 stop point

No implementation beyond this documentation should begin until the boundaries,
staging order, save policy, and first implementation step are reviewed and
approved.
