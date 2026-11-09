# Native portable build

The native build coexists with PlatformIO and compiles the original production
sources directly. It does not replace or modify the ESP32 firmware target.

## Current scope

The `pfme_portable_leaf` library contains modules with no Arduino, ESP-IDF,
display, audio, GPIO, Wi-Fi, or ESP-NOW implementation dependency:

- multiplayer packet codecs from `src/multiplayer/network_protocol.cpp`;
- bounded snapshot helpers from `src/multiplayer/multiplayer_snapshot.cpp`;
- riddle definitions and answer rules from `src/dungeon/riddles.cpp`;
- deterministic dungeon topology generation from
  `src/dungeon/dungeontopology.cpp` and the shared
  `src/dungeon/dungeongraph_core.h` algorithm;
- character progression and deterministic combat calculations from
  `src/characters/combat_rules.cpp`;
- portable session ownership and topology navigation from
  `src/session/game_session.cpp`;
- fixed-capacity server-side party ownership, join codes, reconnect credentials,
  snapshots, and bounded lobby events from `src/session/party_registry.cpp`;
- the header-only quadrature decoder from `src/input/encoderdecoder.h`.

No source is copied into a native-only implementation. PlatformIO and CMake
consume the same authoritative C++ files.

## Windows prerequisites

Install CMake and one supported C++ toolchain:

- CLion's bundled MinGW toolchain;
- standalone MinGW GCC;
- Visual Studio 2022 with Desktop development with C++;
- Clang with a compatible build generator.

Ninja is recommended when it is available. From a terminal where CMake and
the selected compiler are on `PATH`:

```powershell
cmake -S . -B build/native-windows -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native-windows
ctest --test-dir build/native-windows --output-on-failure
.\build\native-windows\pfme_headless.exe
```

With CLion, the same root `CMakeLists.txt` can be configured through a normal
CMake profile and `pfme_headless` selected as the run target. No Windows paths
are stored in the project CMake files.

## Ubuntu prerequisites

Install a C++ compiler and CMake. Ninja is optional:

```sh
sudo apt-get update
sudo apt-get install cmake g++ ninja-build
```

GCC and Clang are both supported. The portable leaf target uses standard C++11,
matching the conservative language baseline supported by the ESP32 toolchain.

## Ubuntu configure, build, and test

From the repository root:

```sh
cmake -S . -B build/native-linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/native-linux
ctest --test-dir build/native-linux --output-on-failure
./build/native-linux/pfme_headless
```

If Ninja is unavailable, omit `-G Ninja` and let CMake select the default
generator. To use Clang, add `-DCMAKE_CXX_COMPILER=clang++` while configuring.

The native tests and headless application are dependency-free host executables.
They execute protocol
golden vectors and validation, bounded snapshot behavior, deterministic spawn
selection, riddle state rules, and encoder decoding.

An optional local installation places only the headless executable under the
selected prefix:

```sh
cmake --install build/native-linux --prefix "$HOME/.local/pfme"
"$HOME/.local/pfme/bin/pfme_headless"
```

## Headless smoke application

`pfme_headless` is a server foundation smoke test, not a playable server. It:

- reports the shared engine API, multiplayer protocol, and dungeon-generation
  compatibility versions;
- generates and validates a dungeon through `dungeontopology.cpp`;
- constructs portable Fighter rule inputs using the production enums and
  compact `ItemInstance` type;
- resolves one attack and damage result through `combat_rules.cpp`;
- exits nonzero if dungeon generation or validation fails.

The complete Arduino `Character`, room tile geometry, entity runtime,
initiative controller, spells, persistence, rendering, audio, ESP-NOW, and
network transport are intentionally reported as unavailable rather than
imitated.

The playable runtime follows the shared PF1e combat action economy. After a
combatant starts its first move, **Double Move** spends the unused standard
action and adds one current effective-speed allowance to any movement still
remaining. Searching a corpse during combat uses the existing loot UI and
spends a standard action. Initiative changes emit a named bottom message and
draw a cyan outline around the active combatant's full footprint.

Player weapon accuracy is derived from the production character rules: melee
uses BAB + Strength modifier, while ranged uses BAB + Dexterity modifier. Both
then apply applicable weapon, fighter weapon-group, condition, and situational
bonuses or penalties. The character sheet displays MP as current/max.

Phase 3D verified Windows MinGW GCC 13.1 and the actual Ubuntu NUC with GCC
15.2 and CMake 4.2.3, including all native tests and the installed executable.

## Interactive session (Phase 3E)

Run `pfme_headless --interactive`. Without arguments, the original smoke
behavior and output are unchanged. This CLI is a trusted local administrator:
numeric player IDs are display/lookup values, not authentication credentials.
It keeps the private handles returned by `GameSession::join`; reconnect uses
the retained handle, not an identity assertion made by a numeric ID.

Commands:

```text
help
new <uint32-seed>
join <name>
players
map
where <player-id>
move <player-id> <north|east|south|west>
disconnect <player-id>
reconnect <player-id>
leave <player-id>
events [last-seen-sequence]
status
quit
```

`new` replaces this CLI's session and resets its players and event cursor.
Names are labels (not character classes or identities), limited to 15 printable
non-space ASCII characters. Commands are limited to 160 characters; oversized
lines are discarded, invalid input reports an error, and EOF exits cleanly.

Example using the actual seed-12345 graph (entrance 0 -> east 1 -> north 2):

```text
new 12345
join Fighter
join Wizard
join Rogue
move 0 east
move 2 east
move 2 north
players
events
quit
```

The resulting locations are Fighter=1, Wizard=0, Rogue=2. Navigation is
**topology-only**: cardinal adjacency does not prove tile walkability, locked
door access, puzzle completion, boss defeat, or room-content restrictions.
Those embedded systems have not been ported, and this CLI does not enforce
or claim to simulate them. No combat or network listener is started.

`src/session/game_session.h` exposes the production session API. One session
owns one generated topology and four participant slots using the existing
`PlayerID` and capacity definitions. Only session methods mutate owned state;
queries return copies or a const topology reference. Rejected commands leave
state and history unchanged. Commands are serialized by one caller; the session
does not add threads or locks. Disconnect retains name and location and reserves
capacity. Reconnect requires the retained in-process `PlayerHandle`, rejects
an already active connection, and preserves location. Explicit leave releases
the slot. Slot IDs can be reused after leave, but generation-checked handles
cannot operate on the replacement player. `new` invalidates old handles too.
Keep a session alive at a stable address while retaining its handles; copying
and moving sessions is disabled. This is a capability boundary inside one
trusted process, not cryptographic authentication or a wire token.

History retains the newest 32 events, with bounded 96-byte text buffers and
uint64 sequence numbers starting at 1 per session. `eventsSince(N)` returns
events strictly after N, plus oldest/newest sequence metadata. An expired
cursor returns `HistoryGap` with the retained suffix: a future client must
refresh full state before resuming. A cursor beyond the newest event returns
`InvalidCursor`. There are no wall-clock timestamps. Successful lifecycle and
movement operations emit events; rejected operations emit none. A new session
starts a new sequence namespace, so clients must discard previous cursors.

## Server-side party registry (Online Stage 1)

`PartyRegistry` owns at most 16 simultaneous parties in fixed arrays. Each party
owns one initialized `GameSession` and maps its party slots directly to the
existing `PlayerID` values 0 through 3. Player 0 is the social leader, while the
server-side `GameSession` remains gameplay authority. A party can start Forest or
Dungeon with any connected size from one through four; all connected members are
selected automatically and disconnected reserved members do not block travel.

Join codes are four numeric digits (including leading zeroes) using
`0123456789`. Collision retries are bounded and tested.
Opaque 128-bit reconnect tokens preserve the same Player ID across disconnect,
are invalidated by leave/party closure, and are distinct from in-process
`PlayerHandle` capabilities. Production adapters must provide operating-system
entropy; deterministic entropy is available for tests. The join code is a
discovery secret, not authentication.

Additional trusted local commands are available in `pfme_headless --interactive`:

```text
party-create <name> [fighter|rogue|wizard|cleric <level>]
party-list
party-status <code>
party-join <code> <name> [fighter|rogue|wizard|cleric <level>]
party-disconnect <code> <player-id>
party-reconnect <code> <player-id>
party-leave <code> <player-id>
party-lock <code>
party-unlock <code>
party-start-forest <code>
party-start-dungeon <code>
party-close <code>
party-events <code> [last-seen-sequence]
```

For example, create a party, copy the returned code, and inspect it:

```text
party-create Alice fighter 5
party-status K7M4Q
party-join K7M4Q Bob cleric 5
party-disconnect K7M4Q 1
party-reconnect K7M4Q 1
party-start-dungeon K7M4Q
party-events K7M4Q
```

The CLI retains reconnect tokens internally and deliberately does not print full
tokens. It is a local development/admin interface, not a player protocol. This
stage opens no network port and adds no socket, HTTP, WebSocket, TLS, browser, or
Discord dependency. **There is no Internet transport for `PartyRegistry` yet.**
Future work is a versioned party protocol and in-process transport double, then
an authenticated WebSocket lifecycle, Bulletin Board clients, and finally
authoritative gameplay commands/snapshots over that transport.

CTest includes production session/CLI checks and a separate scripted invocation
of the actual executable. `pfme_party_tests` covers creation, code validation and
collision exhaustion, capacity, locking, disconnect/reconnect/leave, all four
party sizes, activity start, event history gaps, multiple parties, and backing
session isolation.

The actual Phase 3E NUC verification workspace can be launched over SSH:

```sh
ssh jimbo@nuc-server
cd /home/jimbo/pathfinder-headless-phase3e.523mDBSI
./build/pfme_headless --interactive
```

For a new checkout, substitute its dedicated build directory. No system
service, container, package installation, or port is required.

## Dungeon topology simulation

The `pfme_dungeon_tests` executable runs the same graph-generation algorithm
used by the ESP32 `dungeongraph.cpp` wrapper. The native adapter supplies a
small deterministic seeded RNG. The ESP32 wrapper continues to call Arduino
`random()` in the original order and retains its existing distribution.

The simulator validates room bounds, unique logical coordinates, reachability,
reciprocal cardinal links, branching, boss distance and topology, final
treasure placement, and the designated Riddleman room on the boss route.

Run one seed and print its stable topology fingerprint:

```sh
./build/native/tests/native/pfme_dungeon_tests --seed 12345 --count 1
```

Run a large sequential batch:

```sh
./build/native/tests/native/pfme_dungeon_tests --seed 1 --count 10000
```

Print machine-readable summary output:

```sh
./build/native/tests/native/pfme_dungeon_tests \
  --seed 1 --count 10000 --json
```

Reproduce a reported failure by rerunning its seed with `--count 1`. The
executable returns a nonzero status if generation or an invariant fails and
prints the failing seed and reason. `--self-test` runs fixed-seed fingerprints,
deterministic replay, invalid-boundary checks, and validator regression cases.

The loop chance is applied to each eligible adjacent-room opportunity. The
reported percentage of whole dungeons containing a loop is therefore not
expected to equal exactly 25 percent.

Phase 3A intentionally stops at the real logical graph. The 15x14 tile geometry
and content generator in `roomgen.cpp` still depends on full `DungeonRoom`,
entities, puzzles, traps, furniture, global player level, and Arduino RNG. The
simulator reports `geometry_included=false` until that boundary is extracted
without duplicating or weakening the handheld generator.

## Character and combat rules

`pfme_combat_tests` runs production ability-modifier, class progression,
saving-throw, Fighter feature, equipment modifier, iterative attack, attack
roll, critical confirmation, Power Attack, Armor Class, and physical weapon
damage rules. The ESP32 character and combat wrappers call these same
functions.

Run the deterministic rule regression cases:

```sh
./build/native/tests/native/pfme_combat_tests --self-test
```

Run 10,000 attacks from seed 12345:

```sh
./build/native/tests/native/pfme_combat_tests --seed 12345 --attacks 10000
```

Request machine-readable output:

```sh
./build/native/tests/native/pfme_combat_tests \
  --seed 12345 --attacks 10000 --json
```

Rerun the same seed and attack count to reproduce the exact summary and
fingerprints. The statistical simulation compares a level-five Fighter using
a +1 Keen longsword against the current Bugbear Armor Class of 18, with Power
Attack off and on. Both scenarios receive identical pre-generated attack,
confirmation, and weapon-damage rolls. This paired stream is intentionally a
native analysis technique; the ESP32 continues to consume Arduino random
values only when its existing controller requests them.

The simulation covers physical weapon damage only. Energy damage, sneak
attack positioning, conditions, spells, HP defeat transitions, initiative,
menus, rendering, audio, and multiplayer authority remain in the embedded
runtime and are not imitated by the native executable.

## Private WebSocket adapter (Phase 3F)

Enable `PFME_BUILD_SERVER=ON` to build `pfme_server` alongside the existing
headless CLI and all native tests. This optional adapter uses the same
`GameSession`; the engine stays C++11 and has no network/JSON dependency.
Server-only dependencies are acquired by pinned hashes into the ignored build
tree, with no system package installation. Server code requires C++17; its
integration-test driver uses Python 3.8+ without third-party modules.

See [WEBSOCKET_PROTOCOL.md](WEBSOCKET_PROTOCOL.md) for complete build commands,
environment-secret setup, localhost-only startup, bounded version-1 JSON
messages, ownership/reconnect rules, SSH local forwarding, shutdown, limits
and security assumptions. `ctest --test-dir build/native-server --output-on-failure`
includes the original eight targets plus the real loopback security test when
the adapter is enabled. No browser, Discord client, or permanent service is added.

## ESP32 build

The handheld remains a separate, unchanged PlatformIO build:

```sh
platformio run -e waveshare_esp32s3_zero_n8r8
platformio test -e waveshare_esp32s3_zero_n8r8 \
  --without-uploading --without-testing
```

The second command compiles embedded test targets only; it does not upload or
execute them on hardware.

## Character names and save slots

Character creation now ends with a bounded name step. Names contain at most 15
visible letters, numbers, or spaces plus a null terminator; an empty name becomes
`Adventurer`. Browser play accepts letters/numbers for the name,
Escape/Backspace to delete or cancel, and Space to finish (encoder click).
The handheld uses an encoder character wheel, A to append,
B to backspace, and encoder press to finish.

Save format version 10 adds that character name and provides exactly three
fixed slots. Each slot summary reads name, class, and level without replacing
the active character. Selecting an occupied or corrupt slot for saving opens an
overwrite prompt whose default is No. Versions 1-9 remain readable: the former
single `player` record appears as Slot 1 with the fallback name and is copied to
the new Slot 1 record only after a successful load/write. The legacy source is
removed only after that new record is safely installed.

ESP32 builds store the three versioned blobs in the existing `pathfinder`
Preferences/NVS namespace under `player0`, `player1`, and `player2`. Native
builds use atomic temporary-file replacement and these paths:

- Linux: `${XDG_DATA_HOME:-~/.local/share}/pathfinder-mini-extreme/`
- Windows: `%LOCALAPPDATA%\PathfinderMiniExtreme\`

The filenames are `save_slot_1.dat`, `save_slot_2.dat`, and
`save_slot_3.dat`. Native tests set `PFME_SAVE_DIR` to a temporary directory so
they never touch a player's real saves.

The payload remains the pre-existing character-only save coverage: class,
level/XP, ability scores, HP, equipment, inventory/gold, current MP, known
abilities, active spell study, and Fighter weapon specialization, now plus the
name. Dungeon/forest runs, combat, position, multiplayer sessions, transient
conditions, and runtime class-resource uses are not persisted.

## Phase 3G playable native composition

`PFME_BUILD_PLAYABLE=ON` now compiles the original handheld game loop, input
handlers, character creation, room geometry, movement, AI, combat and rendering
through native-only adapters. Combined with `PFME_BUILD_SERVER=ON`, the NUC can
serve a private authenticated browser view of its actual 240x240 display.

See [PLAYABLE_BROWSER.md](PLAYABLE_BROWSER.md) for build/start/SSH-tunnel commands,
manual playtest, dependency map, protocol limits and known differences. This first
playable composition has one controlling participant per process. GameSession
retains ownership/reconnect capabilities; the original global handheld runtime
contains the actual world. Independent multiplayer views and shared multi-room
combat still require extracting world state and per-player UI from those globals.
No browser gameplay simulation, public service, firmware framebuffer or persistent
NUC service was introduced. The earlier portable-leaf and topology-only descriptions
above still describe those independent targets, not this optional runtime.

## Playtest Batch 3 condition presentation

Poison damage now reports the affected creature's display name and active poison
is rendered as a green runtime tint; no duplicate sprites are stored. Giant Spider
Web is authoritative per creature and becomes unavailable for that spider's next
three turns after a successful cast. Escaping or cutting free spends the existing
standard action but immediately restores an otherwise-unused move allowance.

Monster turns retain immediate authoritative decisions while presentation advances
at 200 ms per visible tile step. A separate deterministic cosmetic hash gives
Intelligence-weighted 300-600 ms thinking pauses without consuming gameplay RNG.
No blocking delay is used.

PlatformIO pins Adafruit GFX 1.12.6 and Adafruit ST7735/ST7789 1.11.0 exactly.
This avoids caret-compatible upgrades changing firmware memory use, and a clean
isolated `libdeps` resolution must select those versions before a release build is
considered reproducible.

## Playtest Batch 4 spell usability

Supported spells now use the production ability resolver during dungeon and
forest exploration. Healing, self-buffs, and allied touch buffs spend MP only on
confirmation and leave exploration active. Confirming a hostile spell establishes
combat, resolves that spell once as the opening hostile action, then rolls normal
initiative. The opening cast consumes the caster's first combat standard action,
so that turn retains movement but cannot cast or attack again. Canceling targeting
spends nothing and starts no encounter.

Targeting uses shared, allocation-free tile predicates. The existing Chebyshev
grid convention treats each square as five feet, so 30 feet is six tiles. Target
range marks legal origins separately from the purple affected-area preview.
Radius, cone, and line previews call the same geometry used by resolution and are
clipped to the active map. A selected-origin burst first validates placement from
the caster, then propagates line of effect from its chosen origin; sight-blocking
tiles and tiles hidden from that origin are excluded from both preview and
resolution. Persistent selected-origin areas use the same footprint helper.
Channel Energy remains a wall-independent caster-centered six-tile radius. Turn
Undead remains caster-centered at six tiles and retains caster line of sight.
Directional cones and lines retain their caster/source line-of-effect rules.

ESP-NOW protocol 7 uses one generic 16-byte committed exploration-ability command
and a 7-byte result. Preview and cursor movement remain local. The host derives the
actor from the sender, validates ability access, resources, room, target, range,
line of effect and hostility, then runs the production resolver. Non-hostile casts
refresh authoritative character/world snapshots and remain in exploration.
Hostile casts begin combat, resolve once as the opening action before normal
initiative, and preserve the spent Standard Action on the caster's actual
initiative turn. Scroll casting remains unavailable over ESP-NOW because its
inventory consumption is not yet represented by this command.

## Creature action poses

Every player class and production monster has one generic action sprite style:
weapon, ranged, casting, or natural. A successfully committed melee, ranged, or
natural attack, spell, Channel Energy, Turn Undead, thrown potion, or active
monster ability displays that pose for 1000 ms, regardless of hit or miss.
Targeting previews, cancellation, and rejected actions do not trigger it. A
second committed action refreshes the deadline instead of queuing animation.

The timer is nonblocking and shares the entity's mutually exclusive exploration
idle deadline, so fixed entity storage and ESP32 internal RAM do not grow. Pose
expiration dirties only the creature footprint. Rendering retains poison tint,
line-of-sight grayscale, the cyan turn outline, and death/KO precedence. ESP-NOW
protocol 7 carries a semantic action flag on authoritative combat results and a
10-byte authoritative exploration action event; clients derive presentation
locally and never transmit sprite frames or targeting movement.

## Playtest Batch 5 party items

Normal Healing Potions now restore `max(0, 1d8 - 1)` HP. In Dungeon and Forest
inventory actions they retain self-use and add Throw and Drop. Throw uses the
shared ground cursor with a 20-foot (four-tile) first-increment limit, line of
effect, and an allied player's occupied square. Confirmation makes a ranged
attack against AC 5 rather than the ally's AC; hit or miss consumes one potion,
and a miss shatters harmlessly. Combat throws spend the standard action, while
drops and pickups spend the move action. Exploration item actions are free and
never start combat.

Native/browser parties expose Trade Items in the map menu and Town bulletin
board. An offer may contain up to four item-instance lines, one unit per selected
stack. It is distance-independent, remains pending without moving ownership,
and transfers atomically only after the named recipient accepts. Acceptance
revalidates party membership, sender ownership, and recipient capacity. A combat
offer may be committed only by the acting player and spends that player's
standard action; administrative acceptance spends no recipient action. Each
player may participate in one pending offer, and disconnecting or leaving cancels
it without moving items.

World drops reuse the room-persistent `ENTITY_LOOT` record with one exact
`ItemInstance`. Dropping removes one unit only after an adjacent valid world slot
is created; pickup adds the item before removing that slot, so serialized input
races have one winner. Healing Potion bottles are light blue, Mana Potion bottles
are deep blue, and weapons, armor/shields, scrolls, and miscellaneous items use
small shared category glyphs in the production 240x240 framebuffer. Dungeon drops
follow room persistence and Forest drops follow the existing Forest lifecycle.

The current ESP-NOW protocol has no authoritative trade, world-drop, pickup, or
thrown-potion commands. Those multiplayer actions remain hidden during an
ESP-NOW shared activity rather than being simulated client-side; handheld
single-player item use, throwing, dropping, persistence, pickup, and rendering
remain available.

## Playtest Batch 6 monster balance

Published Pathfinder statistics are reference material, not mandatory targets.
Mini EXTREME tunes normal monster AC around the actual d20 hit chance of the
level expected to encounter it. At levels 1-2, fodder and normal enemies are
generally AC 10-14, tough enemies remain at or below AC 16, and an elite or boss
may reach AC 17 only when that defense is intentional. HP, offense, poison,
control abilities, positioning, and behavior preserve monster identity without
making repeated misses the default experience.

`Monster::armorClass` is the authoritative normal AC for attacks against a
monster. Player and NPC AC still use the shared character/equipment calculation;
cover and Full Defense remain additive. Creatures with no Constitution score use
their unmodified hit-die rolls instead of treating the missing score as an
implicit -5 modifier. There is no separate Easy/Normal/Hard/Extreme combat AC
modifier, so the base values are not applied twice.

`pfme_monster_balance_tests` is a native-only audit that enumerates every
production monster and reports CR, role, intended level range, effective old AC,
definition AC, average HP, attack bonus, average weapon damage, saves, and actual
level-1/level-2 Fighter hit probabilities. It also checks class attack profiles,
low-level encounter bands, themed encounter ordering, natural 1/20 behavior,
and level-3/level-5 progression. The current roster ends at CR 4 and deliberately
does not auto-scale early monsters; stronger future tiers should add distinct
definitions rather than erase player attack progression with automatic AC gain.
