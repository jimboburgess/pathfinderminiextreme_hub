# Multiplayer Stage 2 Exploration

## Scope

Stage 2 extends the existing ESP-NOW session rather than replacing it. A party remains connected when its members close the Bulletin Board. The host is Player 0 and party leader. Party membership and current-activity participation are separate fixed-size states, so a member may remain in Town while other members explore.

The Bulletin Board uses the existing menu stack and presents `Host Party`, `Nearby Adventurers`, `Party Status`, `Leave Party`, and `Back` as appropriate. Its flavor text is `Find other adventurers.`

## Host-Led Travel

When the host selects Forest or Dungeon with connected party members, the host sends a session-scoped travel invitation. Clients explicitly choose Join or Stay in Town. A decline does not leave the party. Accepted clients receive the authoritative activity preparation data, report ready, and wait for the host's activity-start event before movement is enabled.

Travel responses, activity preparation/start/end, character state, spawn/despawn, snapshots, and room transitions use bounded retries or repeated idempotent events. Duplicate travel invitations for the same activity are invisible and never reopen the prompt. Movement requests and positions are latest-wins events and are not retransmitted indefinitely.

## Forest Synchronization

The Forest is synchronized from authoritative host state, not a seed. Forest creation consumes Arduino's shared global `random()` stream while placing terrain and entities, so call-order or firmware changes can make seed-only regeneration diverge. The host generates normally, then sends the 15x14 tile map and compact non-player entity records in bounded chunks. Clients clear and reconstruct the Forest from that data.

## Dungeon Synchronization

Dungeon synchronization preserves the Stage 1 decision that clients must not regenerate from a seed. The host sends:

- the compact dungeon graph and room metadata;
- the current room's traps, suspicions, furniture, fountain, and puzzle state;
- the current room's 15x14 geometry and compact non-player entities;
- explicit network-safe player character records and authoritative spawn positions.

Only the active room is inflated. Inactive rooms retain the existing compact dungeon persistence representation. When the host crosses a dungeon exit, the host persists the current room, prepares the destination, increments the snapshot epoch, and sends the destination room state. All activity participants transition together without another invitation.

## Snapshot Protocol

Each snapshot chunk carries its snapshot type, activity ID, room ID, epoch, chunk index, total chunk count, payload length, and at most 200 payload bytes. The receiver uses one fixed 384-byte workspace. It rejects wrong activities, room IDs, epochs, chunk counts, out-of-range indexes, oversized data, malformed intermediate chunks, and conflicting duplicates. Identical duplicates are idempotent.

Activity loading uses application-level stop-and-wait reliability. Only one important synchronization frame is in flight across the activity. The client validates and acknowledges `ACTIVITY_PREPARE`, area/transition begin, each character state, each snapshot chunk, and each player spawn. An ACK identifies the activity, epoch, packet type, snapshot type, room, player/item index, and chunk index. A lost frame or lost ACK causes only that item to be regenerated and resent. Duplicate chunks are acknowledged again without changing accepted state. `ACTIVITY_START` is repeated and acknowledged idempotently. Transfers use bounded per-item retries plus a 30-second overall loading timeout.

The ESP-NOW MAC send callback records delivery success/failure for diagnostics but does not replace application ACKs. The receive queue remains six frames because pacing limits synchronization to one in-flight application frame; increasing it would spend roughly another full ESP-NOW frame of internal RAM per slot without addressing the original burst behavior.

No PSRAM allocation was added. There is no full Dungeon duplicate, all-room expansion, dynamic network `String`, giant packet array, or permanent retransmission cache.

## Players and Movement

Every participating adventurer is an `ENTITY_PLAYER` on `TEAM_PLAYER`; `ownerPlayerID` distinguishes ownership. `NetworkCharacterState` explicitly serializes a bounded name, class, level, HP, six ability scores, and speed without sending the runtime-heavy `Character` object. A device retains its full local `Character`; the compact representation is only used to construct remote adventurers.

The host selects spawns by a deterministic Manhattan-radius search around the normal starting tile. It rejects invalid coordinates, blocking map tiles, doors where unsuitable, traps, furniture, and blocking entities. Each accepted spawn immediately blocks later choices.

Single-player still invokes the normal movement path directly. In multiplayer, the host invokes the same authoritative movement rules for local and received input. Clients conservatively predict ordinary unobstructed floor/grass movement, then send a compact request. Doors, transitions, objects, traps, puzzles, and other interaction-heavy tiles wait for the host. Authoritative positions include a uint16 movement sequence; wrap-safe comparison discards stale results. Corrections dirty the old and new entity footprints and do not force a full-map redraw.

Player entities block each other. The first valid host-authoritative move wins; swapping and pass-through are not supported.

## Safe Stage 2 Restrictions

- Shared combat is deferred. A multiplayer encounter is stopped with a Stage 3 message rather than starting divergent combat simulations.
- Multiplayer activity interactions that mutate world state are disabled until their authoritative event protocols exist. This includes chests, NPCs, fountains, traps, puzzles, and pushable furniture. Single-player behavior is unchanged.
- The host controls dungeon room transitions. Split-room dungeon exploration and late activity joining are deferred.
- If a client leaves an activity, its entity is despawned and it returns to Town while remaining in the party. If the host leaves, the activity ends for everyone. Host loss also returns clients cleanly to Town.
- The current-combatant visual highlight remains deferred with shared combat to Stage 3.

## Memory Strategy

Stage 2 adds one 384-byte receive/scratch buffer plus compact participant and reliable-transfer cursors. It does not retain transmitted frame copies: retries regenerate the current packet from authoritative state. Packet payloads are stack-local and never exceed ESP-NOW limits. No Stage 2 allocation uses PSRAM. Exact final firmware RAM and flash figures are recorded in the implementation handoff after a clean PlatformIO build.

## Hardware Reliability Correction

Initial two-device testing exposed two related problems. Repeated `TRAVEL_INVITE` packets reopened the same menu, resetting its cursor and repainting the display. The client now recognizes the same activity/type while pending, accepted, or declined and ignores the duplicate. Travel responses retry independently, so invitation reliability no longer depends on reopening the prompt.

The original loader transmitted roughly a dozen preparation, character, graph, room, world, and spawn packets immediately into a six-frame receive queue. `esp_now_send()` returning `ESP_OK` only confirmed local submission, so a dropped queued frame left the client incomplete until another full burst repeated the failure. Loading now uses the stop-and-wait protocol described above. `MULTIPLAYER_DEBUG=1` prints receive drops, immediate send failures, MAC completion success/failure, reliable retries, duplicate/rejected chunks, transfer identities, and explicit client load blockers.

### Two-Device Reliability Retest

1. Build the same protocol-version firmware for both devices; optionally enable `MULTIPLAYER_DEBUG=1` on both.
2. Host on Device A, join from Device B, and verify both remain listed in Party Status.
3. Select Explore Dungeon on A and leave B's invitation visible for at least five host retries.
4. Rotate B's cursor to `Stay in Town` and back; verify the cursor stays in place and the prompt never flashes or grows the menu stack.
5. Select Join on B and verify one stable `Joining Dungeon... / Receiving area...` display.
6. Observe A sending one reliable item at a time and B acknowledging prepare, begin, character states, graph/detail/world chunks, and spawns.
7. Temporarily increase distance or briefly shield one device to induce a retry; verify only the current item repeats and duplicate chunks are acknowledged safely.
8. Verify B changes to the dungeon map, both devices show the same entrance room, and both player entities are visible on distinct tiles.
9. Verify Party Status reports both players in Dungeon and movement still works in both directions.
10. If loading is deliberately made to fail for over 30 seconds, verify B returns to Town, remains in the party, and A reports that B could not load the dungeon.

## Two-Device Hardware Test

Enable `MULTIPLAYER_DEBUG=1` only when serial traces are needed. Do not upload mismatched protocol versions.

### Test A - Discovery

1. Power on two devices and load compatible characters.
2. Open Bulletin Board on both devices.
3. Select Host Party on Device A.
4. Open Nearby Adventurers on Device B and verify Device A appears by name/class/level.

### Test B - Party Join

1. Join Device A from Device B.
2. Verify Device A is Player 0 and Device B is Player 1 in debug output.
3. Open Party Status on both devices and verify names, classes, host marker, and Town status.

### Test C - Party Persistence

1. Close Bulletin Board on both devices.
2. Use ordinary Town menus for at least ten seconds.
3. Reopen Party Status and verify both members remain connected.

### Test D - Forest Invite

1. On Device A choose Enter Forest.
2. Verify Device B shows the host-named Join/Stay prompt.
3. Choose Join and wait for activity start.
4. Verify both devices show the same terrain/entities and two distinct adventurer sprites on separate tiles.

### Test E - Movement

1. Move Device A one tile at a time and verify Device B updates promptly.
2. Move Device B and verify prediction is immediate and Device A receives the move.
3. Rotate rapidly through several valid moves.
4. Verify delayed packets never move either character backward and authoritative corrections settle cleanly.

### Test F - Collision

1. Approach the same empty tile from opposite sides.
2. Attempt both moves nearly simultaneously.
3. Verify only the first host-accepted player enters it and the other is corrected without overlap.

### Test G - Dungeon Invite

1. Return to Town; verify the party remains connected.
2. On Device A choose Explore Dungeon and accept on Device B.
3. Verify the graph/current room shown on both devices matches, including doors, obstacles, and entities.
4. Verify both players spawn on separate valid nearby tiles.

### Test H - Dungeon Room Transition

1. Move the host through a room exit.
2. Verify Device B enters a brief loading state and transitions automatically.
3. Verify both devices show the same destination room and distinct spawns.
4. Verify Device B receives no second travel invitation.

### Test I - Decline

1. Start a new Forest or Dungeon invitation from Device A.
2. Choose Stay in Town on Device B.
3. Verify Device A starts without Device B, Device B remains in Town, and both remain party members.

### Test J - Disconnect

1. Join an activity with both devices, then power off Device B.
2. Wait beyond the Stage 1 heartbeat timeout.
3. Verify Device A removes Device B's entity, displays a concise disconnect notice, and can continue exploring.
4. Repeat by powering off the host and verify the client returns safely to Town.
