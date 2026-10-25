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

CTest includes production session/CLI checks and a separate scripted invocation
of the actual executable. The original six test targets remain present.

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

## ESP32 build

The handheld remains a separate, unchanged PlatformIO build:

```sh
platformio run -e waveshare_esp32s3_zero_n8r8
platformio test -e waveshare_esp32s3_zero_n8r8 \
  --without-uploading --without-testing
```

The second command compiles embedded test targets only; it does not upload or
execute them on hardware.
