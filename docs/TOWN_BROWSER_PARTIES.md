# Town and browser parties

## Audit and repaired boundary

The native browser used `Service -> GameSession -> PartyRuntime` with four owned
controllers, but a single dungeon/combat workspace. `PartyRegistry` was only
reachable from the headless CLI. Bulletin Board actions called unavailable
ESP-NOW and were hidden in native mode. Forest explicitly rejected native party
travel. Repainting the map beneath a partially redrawn menu erased its rows.

The native adapter now connects the original C++ Bulletin Board to PartyRegistry.
Five-character codes use its existing alphabet and normalization. Code possession
allows an already authenticated character to join; it never grants account or
character ownership. The new read-only `party_status` message has no identity or
player-ID fields and returns only the caller's own party. Authentication,
verified-principal bindings, reconnect capabilities and input schemas remain in
Service/GameSession. No networking or account infrastructure changes are needed.

## Playing

In Town, open Bulletin Board. Create Party makes you leader and displays its
code. Another character selects Join Party, rotates to each of the five code
characters, and presses A/encoder to enter each. B cancels. Party Status displays
code, activity, names, class, level, leader (`*`) and location/offline status.

All members must be connected, alive and in Town before leader travel. Enter
Dungeon or Enter Forest starts one production world and places all members at
safe, unoccupied entrance tiles without replacing their full characters. Town
travel cannot interrupt another member's active adventure. Parties lock during
adventures; new members join only after everyone returns to Town. No implicit
server-wide party or forced participation by unrelated characters is used.

Return to Town acts on your character. The party unlocks after every retained
character has returned. Leader-only Start New Dungeon requires all members home
and preserves their character data. The original confirmation and Resume/Back
flows are retained. Bulletin Board Leave is Town-only and retains your character.
Closing a party disposes its adventure; characters retain their own Town contexts.
The browser's separate Leave character permanently retires that character's
capability; a leader doing so closes the party and returns remaining members
home safely outside combat. Leader disconnection retains membership, ownership
and the reserved slot; it does not silently dissolve or transfer the party.
Reconnect reuses the verified identity and existing capability, not a numeric ID.

Stay Home supports rest, character sheet, save and Back. Saves use the original
serialization in character-scoped process memory, erased on slot retirement.
They do not survive server shutdown. Shop keeps original purchase, sale, stock,
prices and cancellation. Inventory/equipment/spells retain their original rules.
The browser uses all-connected-in-Town readiness and join codes. ESP-NOW travel
invitation Join/Stay popups remain handheld-only; no radio notification bridge
was added. Party codes are invalid after party closure; they have no wall-clock TTL.

## Runtime boundaries

The existing process has four authenticated character/controller slots TOTAL.
PartyRegistry retains its 16-party bound; active native worlds are separately
bounded by those four occupied controllers. Multiple smaller parties can coexist
within that existing cap (for example two parties of two); this change does not
silently increase server-wide character capacity. Extra authorized accounts can
remain signed in and receive the existing Party Full response.

Each party has its own authoritative Dungeon, stable resident room entities,
forest, combat controller, room effects and RNG state. The original globals are
serialized workspaces on the single event-loop thread, switched only by the
native adapter. Entity addresses remain stable across party/view switches.
Input, menus, targeting/inspection, camera, saves, visibility and framebuffer are
owned by each controller. Forest uses the original generator, entities,
movement, renderer and combat rules; no per-player simulation was introduced.
A room change moves one actor. Shared initiative includes all eligible party
actors, including actors in other rooms, without teleportation. Disconnected
turns retain the existing 15-second grace followed by turn skipping. Party world
events and combat messages are scoped to the relevant party; private UI remains
controller-owned. Audio uses the existing original command stream.

ESP32-specific paths retain their radio and single-world behavior. Native-only
world allocation and social hooks are compile-time guarded. The shared menu
invalidation repair keeps a complete C++ menu visible until dismissal.

## Verification

`pfme_town_party_tests` drives the original five input actions through character
creation, home/shop, Bulletin Board code entry, leader travel, movement, room
transitions, return, leave and leader closure, for sizes 1 through 4. Additional
cases cover separate worlds and the original full shared-combat driver.
`pfme_menu_render_tests` checks actual text/highlight pixels through scrolling,
submenu return and reopening. WebSocket regressions form parties via the same
menu inputs and query only authenticated own-party status. Browser evidence and
platform build results are recorded with the verification artifacts, not inferred
from controller allocation. No live deployment is performed by this task.
