# Multiplayer connection reliability and capacity

This change supports 20 distinct authenticated accounts in the native browser
runtime. A party still has at most four characters, with party-local PlayerIDs
0–3. It does not create a 20-character party or a second gameplay engine.

## Capacity boundaries

| Boundary | Limit / behavior |
| --- | --- |
| Native authenticated account reservations | 20, including disconnected characters during grace |
| Native controller, character, input, presentation, framebuffer and audio slots | 20 |
| Admission ledger | Five bounded, original four-player GameSessions; composite controller IDs stay outside PartyRegistry |
| Authoritative world | One original runtime; each party shares its own dungeon/forest, monsters and initiative |
| Native world banks | 20, allowing individual Town worlds and different party sizes |
| Party membership / shared core PlayerID | Four / 0–3, unchanged |
| Shared PartyRegistry | Existing 16-party theoretical bound, unchanged |
| Raw native connections | 64, with a maximum of two per verified principal |
| Account gateway WebSockets | 20 distinct live accounts; 48 raw entries; a new verified connection replaces the old one |
| Uvicorn concurrency | 96; one worker, bounded password-work semaphore retained |
| Requests | Existing 20/second connection and verified-principal budgets |
| Request / response bytes | Existing 4 KiB / 16 KiB limits and JSON depth eight |
| Native visual queue | One response; at most one deferred response during protocol ping; no queued old frames |
| Gateway queues | Existing bounded eight-message WebSocket queues |
| Persistent characters | Existing three account-owned slots, unchanged file format and namespace |

Native entities now distinguish their global native controller from their
canonical party-local owner. This additional entity field is compiled only into
the native controller runtime. ESP32 structures, saves and ESP-NOW IDs retain
their existing representation. Native trade routing maps through the original
four-member trade rules. Legacy graph-only map, move and events operations are
unavailable in playable party mode; they must not leak another admission bank's
events or be mistaken for the actual production dungeon.

## Diagnosed close paths

The previous native read deadline was 15 seconds and did not count protocol
pong/control traffic. The gateway also used a 15-second backend ping policy.
That combination could close an otherwise live, gameplay-idle connection.
The browser had a four-second RPC deadline and no automatic recovery. The
gateway imposed a ten-second unbound timeout and collapsed backend failures
into an apparently normal close. A separate earlier fix for Space/name input
was committed but was not yet in the observed `ed685c1` production image.

These are confirmed source-level failure paths. Historical logs did not carry
enough information to attribute every reported player disconnect to one of them.
Network loss, Cloudflare restarts, suspended devices, resource limits and
browser exceptions remain possible and now have clearer recovery/diagnostics.

Each WebSocket leg has one keepalive owner. The native backend sends a protocol
ping after roughly 15 seconds without traffic and considers a peer dead after
45 seconds without valid traffic/pong. The gateway's backend client does not
run a competing ping timer. Uvicorn handles the separate browser-facing leg
with a 15-second ping interval and 45-second pong timeout. Protocol pongs do not
advance gameplay, reset input inactivity, take a turn or trigger saves.

Native response writes have a bounded 12-second deadline. Simulation uses the
existing single event loop and asynchronous socket writes. A pending write
pauses application reads from that peer, rather than adding old visual frames
to an unbounded queue. The existing framebuffer patch/RLE protocol, response
bound, original C++ renderer and browser Web Audio remain in use. This change
does not include the separately unfinished combined-input/frame lag experiment.

## Recovery and ownership

* An unexpected close starts browser retries at approximately 0.5, 1, 2, 4 and
  then 5 seconds, with modest jitter, for at most 120 seconds. The previous
  image remains visible with a reconnecting message.
* Only authenticated account identity may reclaim the reserved character.
  Public reconnect requests contain no caller-selected player ID or bearer
  ownership token. The existing protected Unix peer and admission checks remain.
* Reconnect performs a fresh authoritative play/frame/audio synchronization.
  Held keys and requests of uncertain delivery are discarded, never replayed.
* Temporary disconnect preserves party membership, character, world association
  and party-local PlayerID. Disconnecting the leader does not immediately close
  the party. The world continues even when no sockets remain connected.
* A disconnected combatant retains initiative and action state. Its first
  disconnected turn has the existing bounded 15-second grace, then ends;
  subsequent disconnected turns do not indefinitely stall the encounter.
* At 120 seconds, safe permanent retirement releases the reservation. Retirement
  is deferred while the original runtime reports combat or a save/retirement
  failure; cleanup must not destroy active initiative or silently lose a save.
  This can temporarily retain capacity beyond 120 seconds.
* Permanent leader retirement retains the existing party-close/Town behavior.
  Explicit Leave is distinct from temporary network loss. Loading a persistent
  character after a server restart is distinct from reconnecting a live adventure.
* Newest verified same-account connection wins. The old socket receives code
  4001 and does not automatically reconnect and fight for control. Native
  authorization expiry/revocation uses 4003; gateway account-session revocation
  uses 4401; gateway server-full rejection uses 4409.
* Existing native admission leases still expire. Fresh reconnect obtains a new
  trusted lease; account disabling, password reset, logout, admission revocation
  and normal session expiry are not bypassed by the retry feature.
* The 21st distinct account is rejected without evicting an existing character.
  Existing reservations reconnect at full capacity without consuming a new slot.

Disconnect and shutdown flush the active account save through the existing
atomic storage implementation. Unslotted new characters still need their first
explicit Save. A reused numeric controller cannot inherit the former account's
save binding, presentation or actor. Automatic saves retain the existing
30-second world-time cadence; heartbeat packets do not cause extra saves.

## Diagnostics and runtime health

Significant native lifecycle events include CONNECTED, AUTHENTICATED,
CHARACTER_BOUND, JOINED_PARTY, LEFT_PARTY, RECONNECTING, RECONNECTED,
RECONNECT_EXPIRED and DISCONNECTED. Available fields include opaque account,
connection, controller, party and party-local player IDs, close code, constant
reason, initiator, transport error and duration. The gateway logs its integer
account ID and transport outcome. Client-provided close reason text, passwords,
cookies, reconnect secrets, admission credentials and invitation codes are not
written to ordinary logs.

Shutdown emits a bounded RUN_SUMMARY, including requests, successful reconnects,
expired reservations, full/auth failures, frame/unchanged counts, response bytes
and maximum tick cost. Ticks exceeding 100 ms are logged at most once per five
seconds. Normal frame-by-frame logging is not enabled. Docker log rotation is
retained. Logs diagnose future incidents; they are not a claim of production
security or proof that every possible close has been eliminated.

The protected backend publishes a mode-0600, same-UID heartbeat file next to its
socket every five seconds of event-loop progress. `pfme_server --healthcheck
/back/backend.sock.health` checks a regular, non-symlink, singly linked file
owned by the runtime UID and less than 20 seconds old. An exclusive advisory
lock prevents a second native runtime from replacing a live socket. Following
a crash, restart may retire only a same-owner socket that refuses connection
and still has the inspected inode. No public health or identity-bypass endpoint
is introduced.

The game Compose example uses `unless-stopped`, FD limit 256 and the local
healthcheck. The memory/CPU/PID limits, unprivileged identities, capability
drop, read-only root and network isolation remain. Docker's health status alone
does not restart a hung process; `unless-stopped` restarts exited processes.
An operator must investigate repeated unhealthy states or crashes. Process
restart loses the in-memory adventure; persistent account saves remain.

## Focused verification

* Native production capacity tests: 1, 4, 8, 12, 16 and 20 characters; at 20,
  five original parties of four, fifth-party-member and 21st-character rejection,
  original frames, actual tile movement/door transitions, independent cameras,
  cross-party isolation and 12 deterministic seeded reconnects. A further
  20-character forest test checks exact ownership and absence of orphan actors.
* Injected-clock actual Service tests: idle minutes with pongs, 15/45-second
  liveness policy, 20/21 authenticated accounts, 119-second recovery, 120-second
  cleanup and duplicate verified connection replacement.
* Browser logic tests: full retry backoff, bounded grace, failed opens,
  generation guards, no Join/action replay, replacement handling, controls,
  RGB565 decode, integer zoom and audio logic.
* Real RFC6455 test: an idle authenticated peer survives with protocol pongs
  for more than 45 seconds, a silent peer expires, and the healthy peer remains
  responsive. Existing real slow-reader and malformed-client tests remain.
* Disposable network-isolated account harness: 21 separate fixture accounts,
  HTTP logins, 1/4/8/12/16/20 concurrent original framebuffer streams, per-case
  latency/bytes/CPU/memory measurements, reserved reconnects, replacement close,
  clean full rejection, reclaimed capacity, forbidden origin/identity tests,
  exclusive runtime lock and forced-crash socket recovery. This is distinct
  from the native five-party dungeon/forest gameplay test; neither substitutes
  for 20 humans playing through Cloudflare.
* Existing account/invitation/revocation/save and private monitoring regression
  tests run through the real account gateway and protected C++ backend.

Observed Windows final focused tests: 18/18; remaining focused regressions:
55/55. Earlier full wire tests passed for party sizes 1–4. Four original combat
assertions (`pfme_town_combat_1`, `_3`, `_4`, `pfme_party_combat_1`) fail on the
preserved baseline and are not hidden or repaired in this connection task.
An additional forest-combat regression found during this migration was fixed
by binding the original forest actor before filtered lookup; the forest tests
now pass. ESP32 release RAM is 292,704 bytes and flash 1,069,837 bytes, unchanged;
all 26 embedded compile targets passed. No upload was performed.

Ubuntu final focused tests passed 24/24 in 557.33 seconds, including all four
party sizes over the real protocol, capacity/forest-20, ownership and idle tests.
The corrected candidate game and account images then passed both disposable
real-account harnesses: 20 simultaneous independent logins/streams, 21st rejection,
five reserved reconnects, same-account replacement, exclusive runtime lock and
forced-crash socket recovery; plus ten single-use invitations, independent
passwords, four durable account save namespaces, different original room frames,
refresh/reconnect, individual revocation and private monitoring authorization.
All existing production containers remained running throughout the isolated tests.
The separate server-only build (`PFME_BUILD_PLAYABLE=OFF`) also passed on Windows.

NUC samples below use real account HTTP/WS through the protected Unix sockets,
original title-screen framebuffer streaming at approximately ten requests/second
per account, and five-second measurement windows. They do not include Cloudflare
latency or claim to represent 20 people simultaneously fighting monsters. CPU is
Docker percentage of one core; memory is the sampled game-container working set.

| Accounts | Median / p95 / maximum RPC ms | Game CPU | Game MiB | Bytes/account/s |
| --- | --- | --- | --- | --- |
| 1 | 2.303 / 2.904 / 6.767 | 0.41% | 6.574 | 18,586 |
| 4 | 3.894 / 10.289 / 70.872 | 13.97% | 8.020 | 18,800 |
| 8 | 6.315 / 18.827 / 91.169 | 24.81% | 9.398 | 17,082 |
| 12 | 7.474 / 32.150 / 96.367 | 36.44% | 11.250 | 16,375 |
| 16 | 11.167 / 43.170 / 92.782 | 46.42% | 12.890 | 14,597 |
| 20 | 12.745 / 48.910 / 99.646 | 52.25% | 14.410 | 15,462 |

After reconnect/replacement, the 20-account sample completed 1,000 requests in
5.056 seconds, median 17.554 ms, p95 49.303 ms and maximum 101.412 ms. Each client
had at most one pending request. No historical before/after aggregate latency
claim is made: the previous production limit could not admit 20 accounts.
Frame completion counts include unchanged polls; byte counts include JSON/audio
reply data but not TLS/WebSocket wire overhead. Native five-party gameplay and
original combat regression results are reported separately above.

Candidate image IDs: game `sha256:291326269f333b65582a70f7aee8e6713473c58b0460f799c2118a34c9891116`,
accounts `sha256:f251d66a994c54ef53d9597a8a8236200fcac40f96cc993c951bb0f03fb8de8f`.
Server executable SHA256: `9cf7bd39f59a06a40028d1a441720de3e426b46dcdf2c488a1dfa6086b93d362`.
Base images were reused offline after verifying their local tags against expected
IDs; no package install or new dependency pull was needed.

## Deployment boundary

Keep the existing path: public HTTPS/WSS hostname → existing Cloudflare outbound
connector → protected frontend Unix socket → account gateway → protected backend
Unix socket. Do not change DNS, tunnel routing, Tailscale, SSH, host firewall,
router configuration or Docker networks. Only the game and account gateway need
replacement. Preserve the connector/status containers and unrelated services.

Before activation: review source and secret scan, finish candidate account-image
tests, verify source/binary/image checksums, back up `pfme-friends_characters`
using the existing Samsung HDD helper, and preserve account/policy volumes.
Do not reinitialize or recreate the external character volume. Update both the
image tag and expected ID for GAME and ACCOUNTS; otherwise the existing admin
helper correctly rejects image drift. Preserve CONNECTOR pins and credentials.
Validate the exact Compose candidate, including `/saves`, UID/GID, Unix sockets,
read-only policy, network isolation, resource limits and health command path.

The user authorized deployment after validation and accepts an adventure reset.
After the scoped restart, verify running images/executable checksums, health,
account/save ownership, actual authenticated HTTPS/WSS in a browser, original
graphics/controls/audio and refresh/reconnect. Compare other container IDs and
start times with the recorded baseline. Keep the save volume and hourly Samsung
HDD backup schedule intact. Roll back only the two Pathfinder image pins and
their Compose configuration if verification fails; never delete data volumes.

The candidate was tested in an isolated source export so the separate unfinished
lag work could remain uncommitted in the original repository. Review and commit
only this verified change before an explicit, non-force master-only push. Never
stage generated build artifacts or accidentally include the unrelated lag
experiment. A running release must be checked against its image and executable
checksums; a successful candidate test alone is not deployment verification.
