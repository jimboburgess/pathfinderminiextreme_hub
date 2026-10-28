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
admission gate alone must not be used as invited-player identity. Browser audio
is still pending at this checkpoint. Retiring a verified character clears its
private observer caches before the numeric slot can be reused by another owner. Party forest travel is deliberately rejected
before the legacy forest entry can reset global encounter state; the original
single-controller and ESP32 forest paths remain available. Broader puzzle,
class/spell, simultaneous-room and adversarial playtesting is still required.

No public endpoint, network installation, firewall change, service deployment,
invitation or push is authorized by this implementation.
