# Pathfinder development roadmap — private browser playtest

Updated 2026-10-27. Immediate target: a real private browser adventure for any
party size **1, 2, 3 or 4**, hosted on the Ubuntu NUC. Discord is voice chat only.
Discord Activities, Embedded App SDK and Discord OAuth are postponed. No fixed
party composition, four-player start barrier or three-player optimization.

## Current implementation and limits

- Phases 2–3D: portable native foundation/headless execution verified on Windows
  and Ubuntu; ESP32 remains supported.
- Phase 3E: GameSession owns four generation-checked participant capabilities,
  topology-level room locations, bounded events and disconnect/reconnect/leave.
  This graph is not yet the playable tile/combat world.
- Phase 3F: authenticated loopback WebSockets, ownership proofs and bounded
  requests/connections/deadlines; no remote administrator/reset API.
- Phase 3G: original production C++ setup/loop, character creation, dungeon,
  movement, AI, combat and 240×240 rendering run natively. One playable controller
  owns the original global runtime; native sound output is currently silent.
- Phase 3H: optional Discord authentication adapter and offline security harness
  are retained, but deferred. Its SDK/Zod advisory remains unresolved. The
  standalone CMake browser/server does not import or bundle that adapter or SDK.
- Phase 3I first extraction: caller-owned production button decoding and character
  creation; native ControllerSlots retain full Character, input, menu, facing,
  view-room and framebuffer by private handle/connection lease. These foundations
  are tested for N=1..4. They are not yet the live shared-dungeon runtime. The
  one-controller lease stays until authority/rendering isolation is complete.

Do not describe graph navigation, separate character allocations or four logins
as completed playable multiplayer.

## Priority 1 — finish the multi-controller gameplay runtime

1. Finish the PlayerRuntime context seam through existing input handlers, menus,
   sheets, inventory/riddle/shop state and rendering. Each controller owns its
   own decoder/timers/UI and original 240×240 surface. Keep the existing 750 ms
   encoder hold and all five handheld actions; release only that player's inputs
   on blur/disconnect. Keep integer 1×–5× and Fit without state resets.
2. Give one SessionWorld the real generated dungeon, full canonical actors,
   monster/room/effect state, clock/RNG and shared messages. Use the existing
   owner-aware production movement, room persistence and ESP-NOW host mechanisms;
   never create another gameplay engine or use reduced NetworkCharacterState as
   a full character store. GameSession capabilities bind to these full actors.
3. Separate per-player view/visibility/room selection from the shared active-room
   simulation workspace. A door crossing moves one actor only. Room reloads must
   preserve full HP/MP, equipment, inventory, spells, XP/gold and conditions.
   Rendering must not consume gameplay RNG or change world state.
4. Extract/reuse the shared initiative controller with stable room/entity/player
   references. Check authenticated actor, turn/revision, room, LOS, resources and
   action budget before mutations. UI/target previews belong to each player;
   combat results, conditions, rewards and monster turns belong to the world.
5. Distinguish occupied character slots from sockets. Reconnect rebinds an owned
   slot; it never creates another actor. Four occupied slots include disconnected
   retained actors. Reject a fifth identity with PartyFull and no world mutation.
   Check entity/combatant capacity; never overwrite or silently drop occupants.
6. Safe late join: reserve a verified slot, create through production controls,
   spawn at a legal entrance at a non-combat authority boundary. If combat or
   entrance capacity prevents entry, keep a private pending-entry state. Never
   regenerate the world, teleport others or grant historical XP on joining.
7. Disconnect policy: retain the actor and release its controls. If it becomes
   current in combat, allow a documented 15-second server-clock grace, then end
   the disconnected turn without free actions/defense. Skip subsequent still-
   disconnected turns without repeated waits. Other connected users continue.
   Reconnect cannot reroll initiative or restore spent actions. If everyone is
   disconnected, pause at an authority boundary without catch-up attacks.

Keep the original ESP32 singleton wrappers, saves, graphics and ESP-NOW formats.
Native party surfaces/state never become four copies in firmware. Start solo
immediately; the same authority path must work at every supported party size.

## Priority 2 — original sound in the browser

Keep C++ AudioCommand definitions and playback semantics as the source of truth.
Add a native audio output sink and bounded, owner/room-filtered audio events,
then a local Web Audio presentation backend. Preserve tone/pause/sweep/triangle
vibrato/LFSR pitched noise/end, four pulse duties, runtime bell sequences, mono
interruption, stop and mute. Do not replace melodies or add music mixing/looping
that the handheld does not have. Account for 2 ms accents between native ticks.

Audio starts from a real browser gesture, requires no microphone permission,
resynchronizes without replaying stale effects, and is independent per listener.
Test timing/output traces from the production interpreter and actual audible
browser behavior. No unbounded media backlog or authentication bypass.

## Priority 3 — invite-only browser access, then approval

See [PRIVATE_BROWSER_ACCESS.md](PRIVATE_BROWSER_ACCESS.md) for the evaluated
options, identity boundary, approval checklist and proposed friend launch flow.
Preferred first option is **Tailscale sharing of a dedicated isolated game node**
on the NUC with reviewed game-HTTPS-port-only grants and private Serve, provided
friends accept a one-time Tailscale client/login. Do not share the general-purpose
NUC with broad default access. Do not enable Funnel or install/configure anything
before approval. Alternative: a dedicated isolated HTTPS gateway with real
per-person authentication, if clientless access is necessary.

A network allowlist does not replace player ownership. Admit only verified
individual identities, issue bounded expiring game sessions and one-use socket
admission, retain per-identity limits and protect reconnect capabilities. The
shared development password alone is not a player identity. Discord OAuth is
not required for the standalone build.

## Priority 4 — isolation and operational playtest

Use a dedicated nonroot game VM/sandbox on the NUC with no personal directories,
SSH credentials, Docker socket or host networking; read-only filesystem where
possible, bounded writable storage, no capabilities/new privileges and explicit
CPU/RAM/PID/fd limits. Deny home-LAN/metadata/unrelated-service egress. Demonstrate
those boundaries from inside the sandbox; a container alone is not proof.

Do not alter Homepage, Navidrome, Uptime Kuma, music bot, UniFi, shared proxy or
other existing services. No router ports, public endpoint, firewall changes,
network-software install, DNS/tunnel activation or invitations without a reviewed
concrete configuration and explicit approval. No permanent game service yet.

## Acceptance and verification gates

For **each N=1,2,3,4**, exercise real application/protocol code: independent
identity/ownership/input/UI/media, actual tile collision and room transitions,
one shared clock/monster/initiative state, complete character retention, clean
shutdown and reconnect/expiry/replay/malformed-client denial. Use independent
browser profiles and no required class composition; include duplicate classes.

- N=1: complete a real solo creation/town/dungeon/combat/victory/exploration loop.
- N=2: separate movement, owned reconnect and shared combat.
- N=3: different rooms, authorized shared events and initiative.
- N=4: complete an encounter with independent original views and audible music/SFX.
- Fifth occupied character: PartyFull with state unchanged; valid owner reconnect
  still succeeds while full. Race duplicate joins/last-slot allocation and test
  pending late join, explicit leave/reuse and old generation rejection.

Do not substitute mocks, graph-only tests or direct state assignment for the
end-to-end gameplay result. Use controlled fixtures only to arrange repeatable
worlds/encounters. Recheck Windows/Ubuntu native and WebSocket regressions, ESP32
release and all embedded compile targets whenever shared source changes.

## Later, explicitly deferred

Discord Activity/SDK/OAuth, public hosting, multiple concurrent session registries,
new rendering/UI, production persistence/HA and broad infrastructure work are not
prerequisites for this private standalone trial. Preserve useful Phase 3H security
work as an optional reference. Resolve its dependency advisory before ever
reactivating it; do not ship the SDK in the standalone page.

## Completion definition

One authoritative Pathfinder dungeon is genuinely playable by any party size
from one through four, with original graphics/input/sound and verified individual
ownership. Friends launch a normal browser through an approved private access
path without SSH or development tools; Discord provides voice only. Approval,
isolation and gameplay acceptance must pass before invitations. Local commits
retain reviewable progress; no automatic push or deployment.
