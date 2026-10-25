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
- the header-only quadrature decoder from `src/input/encoderdecoder.h`.

No source is copied into a native-only implementation. PlatformIO and CMake
consume the same authoritative C++ files.

## Linux prerequisites

Install a C++ compiler and CMake. Ninja is optional:

```sh
sudo apt-get update
sudo apt-get install cmake g++ ninja-build
```

GCC and Clang are both supported. The portable leaf target uses standard C++11,
matching the conservative language baseline supported by the ESP32 toolchain.

## Configure, build, and test

From the repository root:

```sh
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native
ctest --test-dir build/native --output-on-failure
```

If Ninja is unavailable, omit `-G Ninja` and let CMake select the default
generator. To use Clang, add `-DCMAKE_CXX_COMPILER=clang++` while configuring.

The native tests are dependency-free host executables. They execute protocol
golden vectors and validation, bounded snapshot behavior, deterministic spawn
selection, riddle state rules, and encoder decoding.

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
