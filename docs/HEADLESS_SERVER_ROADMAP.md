# Headless server roadmap

`pfme_headless` proves that the current portable production modules can be
composed without Arduino or display hardware. It is not yet a game server.

## Authority boundary

The future process should contain a `SessionRegistry` that owns independent
`GameSession` instances. Each session should own:

- dungeon graph, room runtime, authority RNG, turn state, and shared messages;
- one `PlayerState` per stable player ID, including room, position, visibility,
  movement, interaction, character, and reconnect state;
- combat state that enrolls all participating players in initiative without
  moving players between rooms;
- bounded command and event queues with monotonically increasing sequence IDs.

Clients should submit commands such as join, move, change room, interact,
attack, cast, end turn, and inventory action. The session validates identity,
room, position, turn ownership, targets, resources, and command sequence before
calling shared gameplay rules. Clients never submit authoritative dice,
damage, or resulting positions.

## Transport and serialization

The session layer should emit bounded events and snapshots through a transport
interface. The existing ESP-NOW codecs and compact multiplayer references
remain authoritative compatibility inputs; a future WebSocket adapter can
frame versioned server messages without putting socket code in gameplay.

Snapshots need explicit versions for session metadata, dungeon topology,
room state, player state, combat state, and shared-message position. Reconnect
should authenticate a stable player, resume from an acknowledged event
sequence when possible, and otherwise send a complete snapshot. Persistence
should use atomic files outside the container image and must not silently
reuse the ESP32 save format for server sessions.

## Incremental implementation

1. Add a portable `GameSession` shell with stable player IDs, independent room
   ownership, shared messages, and graph-level room-transition validation.
2. Extract real room geometry, walkability, and character value state so the
   session can validate movement and interactions without rendering.
3. Move initiative progression, HP/conditions, abilities, and combat results
   behind command/result interfaces while preserving current handheld rules.
4. Add versioned session snapshots and deterministic reconnect tests.
5. Add an in-process transport test double, then a WebSocket adapter.
6. Add browser and Discord clients only after authority and snapshot tests pass.

## Ubuntu NUC deployment

The Intel NUC5i3MYBE can run the future server as one small x86-64 Linux
process without a desktop environment. The recommended eventual deployment is
a multi-stage Docker image that builds from a pinned unified `master` commit
and copies only the release executable into a minimal non-root runtime image.

Initial deployment should:

- use a dedicated Docker network and avoid publishing any host port;
- alternatively bind only to `127.0.0.1` for local administrative testing;
- run as a non-root UID with a read-only root filesystem and one bounded data
  volume for session persistence;
- start with approximately `0.5` CPU and `128 MB` memory limits, then measure;
- use tagged images or pinned commits rather than modifying a live container;
- leave existing containers, router, firewall, and reverse proxy unchanged.

Do not expose the service publicly until authentication, authorization, TLS,
rate limiting, origin checks, and backup/recovery are implemented. A reverse
proxy and secure `wss://` endpoint should be a later deployment layer shared by
the browser client and Discord Activity.
