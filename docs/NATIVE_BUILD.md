# Native portable-leaf build

The native build coexists with PlatformIO and compiles the original production
sources directly. It does not replace or modify the ESP32 firmware target.

## Current scope

The `pfme_portable_leaf` library currently contains only modules that already
have no Arduino, ESP-IDF, display, audio, GPIO, Wi-Fi, or ESP-NOW dependency:

- multiplayer packet codecs from `src/multiplayer/network_protocol.cpp`;
- bounded snapshot helpers from `src/multiplayer/multiplayer_snapshot.cpp`;
- riddle definitions and answer rules from `src/dungeon/riddles.cpp`;
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

## ESP32 build

The handheld remains a separate, unchanged PlatformIO build:

```sh
platformio run -e waveshare_esp32s3_zero_n8r8
platformio test -e waveshare_esp32s3_zero_n8r8 \
  --without-uploading --without-testing
```

The second command compiles embedded test targets only; it does not upload or
execute them on hardware.
