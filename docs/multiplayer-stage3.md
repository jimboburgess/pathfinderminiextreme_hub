# Multiplayer Stage 3

Stage 3 extends the Stage 2 ESP-NOW session, activity, pacing, acknowledgement,
and snapshot architecture. It does not introduce another transport or route
single-player play through networking.

## Architecture changes

- Protocol version 4 adds bounded shared-message, player-room-transition, and
  multiplayer-combat payloads.
- Dungeon location is authoritative per `PlayerID`. A participant's spawn state
  carries its room, position, and facing; the activity no longer treats one
  runtime room as the location of the whole party.
- Each handheld inflates and renders only its local player's room. Remote player
  entities exist only when their owner is in that room.
- The host retains one authoritative dungeon and the existing compact inactive
  room persistence. It temporarily inflates a remote room when validating a
  remote move or resolving combat, persists the result, then restores the
  host's displayed room.
- Every dungeon room has a wrap-safe 16-bit revision. Room transfers reject old
  revisions and reuse Stage 2's stop-and-wait snapshot acknowledgement.

## Shared messages

`setGameMessage()` remains local-only for menus, loading, connectivity, and
settings. `setSharedGameMessage()` asks the host to publish an authoritative,
fixed-size gameplay message. Shared packets carry the activity ID and a 16-bit
sequence; clients ignore stale or duplicate sequences and never rebroadcast a
received message.

Combat results use the reliable combat-event channel. Room and exploration
announcements use the lightweight ordered shared-message packet.

## Independent rooms

A client crossing a dungeon door sends its source room, direction, activity,
and transition sequence. The host validates the player's actual position,
door, puzzle lock, and dungeon graph connection. The client cannot choose its
destination room.

Only the moving client enters room loading. The host sends `ACTIVITY_PREPARE`,
`ROOM_TRANSITION`, room detail, current world state, current character states,
and same-room player spawns using the existing acknowledged transfer. Other
players continue in their current rooms. Joining or leaving a room creates or
removes only the appropriate room-local remote player entities.

## Stable identities

Players are referenced by `PlayerID`. Multiplayer monsters are referenced by
the pair `roomID + entityID`, where `entityID` is the stable non-player room
entity slot. Initiative and network requests never use raw `Entity*` values.
The host's compact player state remains valid even when a player's room is not
inflated.

## Multiplayer combat

Combat is global to the activity but does not teleport combatants. The host
rolls all initiative, owns action state and RNG, and initially adds all living
participating players plus the triggering room's living hostile monsters.
Monsters discovered later join the same combat and are sorted only into future
initiative positions, so a completed turn cannot repeat.

Only the device owning the current player combatant can submit an action.
Requests identify stable actor and target references. The host validates combat
and activity IDs, ownership, turn, room, range/line of sight, action use, and
target state before resolving attacks or abilities. Clients apply reliable,
ordered authoritative events containing positions, HP, state, room revision,
turn, round, and bounded message text.

Ordinary turn-based movement uses the same reliable action request/result path.
Door crossings retain the reliable player-specific room-transfer protocol, so
large room state never rides in a combat event.

Players in other rooms still receive turns and may move or cross doors toward
the encounter. Only the host runs monster behavior. A downed multiplayer player
becomes unconscious; combat continues while another living player and an
engaged hostile remain. Combat ends globally only when no engaged hostile
combatant remains.

The visible current combatant receives a cyan outline. When the current actor
is outside the local room, the shared turn message identifies that actor.

## Persistent effects

Map effects are stored per room in PSRAM and are included in the bounded world
snapshot. Successful area abilities schedule a reliable refresh only for
clients currently occupying the affected room. Players entering later receive
the latest room revision and effect footprint.

The snapshot workspace remains 384 bytes in internal RAM. A pathologically
crowded room with many simultaneous maximum-footprint effects may exceed this
bound and fail the transfer safely rather than allocating another dungeon or a
large DRAM buffer.

## Reliability

Combat begin, roster updates, action results, turn advancement, and combat end
use one bounded in-flight event. Each participating client acknowledges the
exact combat ID, event sequence, and packet type. Lost events or ACKs resend the
same idempotent event. Cosmetic movement remains latest-wins.

Room loading continues to use Stage 2's one-packet/chunk-at-a-time transfer,
bounded retries, overall timeout, and exact snapshot ACK identity.

## Memory

- Internal snapshot workspace: unchanged at 384 bytes.
- Network character state: four entries allocated once in PSRAM.
- Multiplayer combat runtime: one bounded runtime allocated once in PSRAM.
- Per-room persistent map-effect cache: twelve fixed entries allocated once in
  PSRAM.
- No additional dungeon or permanently inflated room is created.

## Deliberate limits

- Single-player combat and dungeon behavior retain their original direct paths.
- Multiplayer scroll casting is rejected until inventory consumption is
  authoritative.
- Full Defense, attacks, normal ability casting, movement, and ending a turn are
  networked. Delay, five-foot-step-specific UI, and item-use requests remain
  future work.
- Monster turns are host-only and use conservative room-local pursuit/melee;
  specialized monster spell/script networking remains future work.
- Complex world interactions that lack an authoritative result packet remain
  disabled rather than allowing clients to diverge.
- XP, loot distribution, revival, and split activity authority are not part of
  Stage 3.
