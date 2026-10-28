# Phase 3I production controller migration

The native server has an explicit `--runtime party` option. The default remains
the verified single-controller adapter while party-mode acceptance is completed.
Both modes bind only IPv4 loopback and retain the existing authentication,
capability ownership, origin checks, strict schemas and bounded screen protocol.
This is a development mode, not an invite-only identity or deployment solution.

## Implemented paths

The party adapter leases up to four production controllers through
`ControllerSlots`. One `Dungeon`, RNG and millisecond clock remain authoritative.
The original C++ button handlers, creation, full Character, menus, equipment,
inventory, movement, combat and Adafruit renderer execute in one serialized
runtime. There is no JavaScript gameplay implementation or per-player dungeon.

`context_fields` is a temporary, explicit borrowing bridge for private UI state
in existing production modules. It stores values with their C++ copy operations;
it does not snapshot a process, copy a world, or serialize raw object bytes.
The full Character, creation/input/menu models and native surface are owned by
each controller. Private UI pointers refer either to the borrowed module's own
restored storage or retained room entities. Each native canvas swaps its complete
drawing context into the original `tft`, retaining allocation ownership.

Room entities have one resident native collection per room. Selecting another
observer never invokes the handheld room unload/reload path. Actual travel
validates destination capacity/entry before committing only the moving actor.
During combat, the actor's production combat references are retargeted as part
of that transaction and one movement square is charged. Occupied initiative
slots cannot be recycled as other entities while the encounter retains them.
Room timers and map effects belong to rooms; discovery/LOS/render caches belong
to observers. Hidden monsters are also filtered from the native inspect action.

One production Combat controls initiative, attack resolution, conditions and XP.
Living occupied dungeon characters share initiative even when in separate rooms.
The current owner must match before movement, targeting or ending a player turn.
Monster AI uses a living target in its own room. World-event messages are shared
only with characters in the relevant room; personal menu/inventory text is local.
New entrants can create characters in town, but enter the dungeon only outside
an active encounter. They enter a legal free entrance tile; joining never
regenerates the dungeon or teleports existing actors.

Disconnect releases only that controller's controls and retains the actor,
inventory, UI and framebuffer. Connected players keep the world running. An
offline current player receives one 15-second grace, then yields without free
actions; subsequent turns while still offline yield immediately. Reconnection
does not replenish a grace already spent on the current turn. With no connected
playing clients, the server pauses advancement at its serialized tick boundary.

## Memory and transport

All additional storage is native-only. Each display is 115,200 bytes RGB565.
Four canvases plus the bounded per-owner screen snapshots/patch workspace need
about 2.3 MB plus the original 115,200-byte scratch canvas, before allocator/object overhead. Native resident room entities and
explicit UI/observer banks add bounded storage; they do not enter ESP32 RAM.
World visual refreshes are capped at 20 Hz per native observer; input/UI changes
can render immediately. WebSocket replies retain the 16 KiB limit and existing
request limits; each owner has an independent immutable pending screen frame.

## Verification and limits

The test driver sends the same five handheld actions through production handlers.
Tests cover every party size, independent tile steps, original menus, separate
rooms, distinct framebuffers and fifth-slot rejection without clock/event/frame
changes. Generated first-encounter fixtures use seeds 2/0/1/2 for sizes 1/2/3/4;
they exercise actual initiative, attacks, damage, XP and disconnect-turn behavior.
The all-Fighter fixtures demonstrate that no required party composition exists.
The wire tests authenticate each client separately, reject added player-ID fields,
exercise separate screen streams, verify reconnect proofs and clean shutdown.
Separate Chrome windows have also exercised the existing browser client.

This checkpoint is not production-secure. Standalone per-person admission and
the final private ingress/isolation configuration remain pending. The development
admission gate alone must not be used as invited-player identity. Browser audio is implemented through the original C++ command interpreter, as described below. Retiring a verified character clears its
private observer caches before the numeric slot can be reused by another owner. Party forest travel is deliberately rejected
before the legacy forest entry can reset global encounter state; the original
single-controller and ESP32 forest paths remain available. Broader puzzle,
class/spell, simultaneous-room and adversarial playtesting is still required.

No public endpoint, network installation, firewall change, service deployment,
invitation or push is authorized by this implementation.

## Original browser audio

The native-only audio adapter retains the original 43 C++ music/sound programs,
command interpreter, note frequencies, integer sweep/vibrato/noise modulation,
dynamic bell sequences, duty settings, mute and single-channel interruption.
Each occupied controller owns its playback state and a fixed 512-event ring.
Replies carry at most 32 frequency/duty transitions, with a server-owned cursor;
the authenticated capability and connection serial select the listener. There is
no client-selected player or listener ID and no additional endpoint/protocol.
Private UI tones stay local; world sounds reach listeners in the same room.
Muted listeners retain their own setting. Other rooms receive no sound events.

The presentation audio clock advances from elapsed runtime ticks. Virtual native
rendering delays cannot rush the original hardware-duration notes. A late or
reconnected listener resynchronizes to the current oscillator snapshot rather
than replaying old history. Four fixed rings add approximately 32 KiB native RAM;
all audio/controller additions are excluded from ESP32 builds.

The fixed, locally served audio.js uses Web Audio PeriodicWave pulse coefficients
for the original four LEDC duties and schedules the C++ frequency changes with
120 ms lookahead. It interprets no gameplay rules or sound command programs in
JavaScript. Browser oscillators are band-limited approximations of piezo drive,
not recordings of the physical speaker; acoustics and precise network timing
will differ. Nodes are capped at 32. User gestures unlock audio; lost focus,
hidden tabs and disconnect stop/suspend it. Focus loss wins over an in-flight
resume. No microphone, external media, CDN, autoplay bypass or new permission is
used. Single-controller legacy native mode remains silent in the browser.

## Verified implementation checkpoints

The complete Windows CTest run passed 34/34. Ubuntu GCC 15.2.0 / CMake 4.2.3
passed 30/30; four optional Node/browser logic tests are absent on that host
because Node is not installed. The Windows run includes those four tests.
Party sizes 1, 2, 3 and 4 pass production exploration, wire ownership/reconnect,
and generated encounter fixtures. Four actual Chrome clients were also tested
against both Windows and a temporary Ubuntu loopback server through an owned
SSH forward: separate original views in different rooms, retained reconnect,
240x240 / 3x nearest-neighbor display, and nonzero real Web Audio output for
each client. This is controlled browser automation, not a friends playtest.
All 43 original C++ sound traces were rendered through Chrome OfflineAudioContext.
The original title reference still has zero differing pixels.

ESP32 release and all 26 embedded compile targets are checked separately; there
is no firmware upload or hardware execution. The verified release footprint is
292704/327680 bytes RAM and 1050145/4128768 bytes flash, unchanged from the
starting Phase 3I checkpoint. No PlatformIO configuration change is needed.

## Remaining playtest gates

This establishes independently playable dungeon exploration and generated shared
encounters, not completion of every adventure, puzzle, boss or class/spell path.
Party forest entry remains blocked. The original 16-combatant capacity needs
explicit policy/testing for simultaneous large multi-room encounters. Connected
players can still stall their own combat turn; disconnected players have the
specified grace/yield policy. Saves and reconnect capabilities survive socket
loss, not server-process restart; a tab's capability is deliberately memory-only.

Per-person standalone admission, durable identity/session expiry/revocation,
approved isolated runtime and a game-port-only private access path are still
required before inviting friends. Development admission plus private character
capabilities is not a substitute for verified individual accounts. The optional
Discord Activity/SDK dependency is deferred and absent from active standalone
assets; this work does not resolve its outstanding advisory. Do not enable it
without rechecking and resolving its dependencies. No public connectivity,
network installation, firewall change, existing-container change or push occurs
as part of these verification checkpoints.
