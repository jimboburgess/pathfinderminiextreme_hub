# Original handheld display: bounded rendering slice

This is an **offline title-screen capture**, not a playable browser game and not
completion of a full browser-client milestone. It deliberately does not invent a
dungeon UI for the topology-only GameSession.

## Build and view

From the repository root, using the existing native compiler, CMake and Python 3.8+:

```text
cmake -S . -B build/display -DCMAKE_BUILD_TYPE=Release -DPFME_BUILD_BROWSER_CAPTURE=ON
cmake --build build/display
ctest --test-dir build/display --output-on-failure
```

Open `build/display/apps/browser/viewer/index.html` directly in a desktop browser.
Copy the entire `viewer` directory when moving it; no server or install is needed.
Do not put this directory on a public host. The capture contains no credentials,
player data or transport client. Its CSP prohibits network connections.

Select 1× through 5× or Fit to Window. Default is 3× when it fits, otherwise the
largest smaller integer scale. Fit also uses an integer factor; viewports smaller
than 240 pixels scroll rather than distort the display. The backing canvas always
remains 240×240. Scaling changes CSS dimensions only; it neither redraws gameplay
nor restarts the animation clock. Nearest-neighbor CSS and disabled canvas image
smoothing preserve game pixels. Browser zoom / OS fractional display scaling can
affect physical device-pixel alignment; use 100% browser zoom and integer display
scaling for physical pixel comparisons. Browser visual QA is still required.

## Source of truth and shared rules

`extract_title.py` extracts these existing C++ function definitions verbatim from
`src/graphics/display.cpp` at build time:

- `drawStartScreen` and `drawStartAnimation`.
- The width/height transparent sprite loop and its 64×64 wrapper.

The generated include is a build artifact, not a maintained second screen layout.
Changes in the original display file regenerate it. The capture links the actual
`src/graphics/sprites.cpp` arrays, including the RGB565 magenta transparency key.
It uses the same Adafruit GFX 1.12.6 implementation and classic bitmap font as the
installed handheld dependency. See `third_party/adafruit_gfx/README.md` for the
vendored source provenance and license. Nothing was changed under `src/` or in
PlatformIO configuration.

The native-only Arduino/Print compatibility headers supply the small subset needed
by GFX and this slice. No GPIO, SPI, Arduino runtime, clock, save system or audio
emulation is claimed. The message is the original empty cold-start message. The
title theme is suppressed. The original 800 ms animation sequence is sampled at
800/1600 ms; JavaScript presents those two C++-rendered frames on that timeline.
It converts RGB565 to opaque RGBA; it contains no sprite, font or layout rules.

This extraction is a temporary bridge for a self-contained screen, not a general
C++ parser or a portable replacement for the entire display subsystem. An expanded
slice must be audited and should introduce a typed draw surface and immutable view
state rather than accumulate dependency stubs or copy drawing rules into JS.

## Reference and differences

The reference backend subclasses Adafruit GFX with only `drawPixel` overridden,
retaining its generic clipping, text and primitive drawing. The browser capture
backend uses GFXcanvas16, including its optimized framebuffer operations. Both run
the actual original title function, original font and original sprites. The capture
program compares every RGB565 pixel before packaging; it fails on any difference.
Animation comparison also verifies erasure and redraw, and packaging checks that
all changed pixels lie within the original fighter rectangle (64,95)-(127,158).

On Windows and Ubuntu the title and animation match with zero differing pixels.
The title SHA256 is
`c756eceaa1e6bc13fa3e0e03d8e0dc12e665fadf2f7ab53fcfa575dc522557f2`;
the animated frame SHA256 is
`7b882aca7199ae4bd96ee280ecdb711017d8d8ab1f3e137da79157a2a781d634`.
Animation changes 3,016 pixels. `capture.json` records each generated artifact.

This is a **software reference**, not a physical ESP32 display capture. It verifies
RGB565 drawing results, not panel gamma, backlight, SPI timing, tearing, perceived
color or audio. The native PNG is also a software capture. Automated browser visual
QA was blocked because the available browser tool rejects `file:` URLs. No server
was introduced to bypass that restriction. Node tests exercise pixel conversion,
zoom, timeline continuity and control intents with a mock DOM; they do not establish
browser-specific rasterization or CSP behavior. Manual browser inspection remains
an acceptance gap. A real hardware reference comparison is also outstanding.

## Controls and authority boundary

Up/Right/W/D are encoder-clockwise intents. Down/Left/S/A are counterclockwise.
Enter is encoder press. Button A/B default to J/K and can be changed to distinct
letter/number keys excluding WASD. Escape is a back/cancel intent. Input fields
retain normal keyboard behavior. Onscreen equivalents are provided.

These controls report unsupported intents outside the game surface. The title's
actual A = New Game / B = Load Game semantics are retained in that report. They
do not fabricate character creation, load a different game, or turn encoder
rotation into cardinal movement. The handheld start handler itself ignores encoder
rotation/press; the viewer reports that. No room/player state is stored in JS and
no GameSession command is issued. The existing C++ GameSession and authenticated
loopback transport remain the only implemented authority for shared gameplay.

## Why the rest is not ported yet

The audit found direct ST7789 calls in `graphics/display.cpp`,
`dungeon/roomdraw.cpp`, `graphics/charcreationscreen.cpp`, `characters/sheet.cpp`
and the input/menu systems. Room tiles depend on DungeonRoom, active entities,
puzzle/trap/effect state and handheld globals, beyond the portable session's room
topology. Existing tile arrays, wall/floor variants, sprites, cursors, overlays and
message drawing must be preserved, not approximated with a new topology map.
Character sheets and combat menus likewise require view state not currently owned
by GameSession. Porting them faithfully is substantial work.

Smallest next migration: define a read-only, authoritative tile-room view and a
typed draw surface, then move one existing `roomdraw` path behind that surface.
Test its original tile geometry, palette, transparency and cursor against a
software reference and a physical handheld capture before expanding menus/combat.
Keep input intents separate from actions; each original state interprets them.
Connecting browser input requires a separate reviewed authentication/origin design;
the Phase 3F server still rejects all Origin headers and was not relaxed here.

## Memory, bandwidth and synchronization

- One native RGB565 frame: 240×240×2 = 115,200 bytes. Reference plus canvas use
  230,400 pixel bytes, in addition to GFX objects, assets and process overhead.
  This exists only in the native capture executable, never on ESP32.
- Two stored frames: 230,400 raw bytes, about 307,200 base64 characters. The browser
  retains two RGBA ImageData frames (460,800 pixel bytes), a canvas surface and
  encoded/transient data; browser runtime/compositor overhead is additional.
- Current gameplay/network bandwidth is zero: files are generated once and opened
  locally. Animation needs no server synchronization and zoom changes no state.
- A future full-frame stream would cost 115,200 bytes per frame before framing:
  1,152,000 bytes/s at 10 fps, or 1,536,000 bytes/s with base64. It cannot fit the
  existing 16 KiB response limit. Do not raise limits or bypass authentication
  merely to stream this capture. Review bounded chunks/dirty regions, revision and
  frame identifiers, atomic presentation, backpressure and stale-frame rejection.
- Future live rendering must consume an immutable session revision on the same
  serialized authority path. Rendering must never modify gameplay or cause a
  separate JavaScript simulation. Client zoom stays independent of that revision.

No framebuffer, extra assets or compatibility header enters ESP32 compilation.
No service, port, authentication change, NUC package or machine setting is needed.
