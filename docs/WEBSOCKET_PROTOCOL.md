# Private localhost transport — application protocol 1

Phase 3F adds `pfme_server`, an optional native adapter over the existing
`pfme_portable_leaf` library and `GameSession`. It has one session, initialized
at process startup. Navigation is still topology-only. There is no browser UI,
remote administrator, combat, persistent session storage, or Discord integration.

## Dependencies and build

The server uses pinned Boost 1.92.0 (Beast/Asio and JSON) and Monocypher 4.0.3.
Beast was selected for its event-loop integration, bounded composed operations,
and RFC WebSocket handling. IXWebSocket was considered, but would add another
transport-specific lifecycle abstraction; using JSON from the same Boost release
keeps serialization acquisition together. No custom WebSocket framing is used.
Monocypher supplies BLAKE2b and constant-time digest comparison without requiring
OpenSSL development packages on either host. OS CSPRNGs supply 32 random bytes
per credential: Windows BCryptGenRandom and Linux getrandom.

`apps/server/CMakeLists.txt` pins official HTTPS source URLs and SHA-256 hashes.
No installation script is run. Dependencies extract/download into the ignored
build tree. Licenses are retained and installed with the server. For offline
builds, set `FETCHCONTENT_SOURCE_DIR_PFME_BOOST` to the verified extracted Boost
root and `PFME_MONOCYPHER_DIR` to a directory containing the pinned `.c`, `.h`
and `LICENCE.md` files. Monocypher files are hash-checked even in offline mode.

```sh
cmake -S . -B build/native-server -DPFME_BUILD_SERVER=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/native-server --parallel 1
ctest --test-dir build/native-server --output-on-failure
```

Server C++17 is isolated from the shared C++11 engine. Tests additionally require
Python 3.8+ (standard library only). `PFME_BUILD_SERVER` defaults OFF, so ordinary
headless builds do not acquire server dependencies. PlatformIO never compiles
`apps/server`, Boost, JSON or Monocypher. No ESP-NOW version-4 codec, packet,
save format, existing gameplay rule or firmware setting changes.

## Startup and shutdown

Set `PFME_DEV_SECRET` to a **fresh CSPRNG-generated 32-byte lowercase hex value**
(64 characters). There is no fallback credential. The server rejects missing or
incorrectly formatted configuration and never prints its value. Example setup
in PowerShell (the generated value is captured, not displayed):

```powershell
$env:PFME_DEV_SECRET = python -c "import secrets; print(secrets.token_hex(32))"
.\build\native-server\apps\server\pfme_server.exe --port 8765 --seed 12345
Remove-Item Env:PFME_DEV_SECRET
```

Ubuntu:

```sh
export PFME_DEV_SECRET="$(python3 -c 'import secrets; print(secrets.token_hex(32))')"
./build/native-server/apps/server/pfme_server --port 8765 --seed 12345
unset PFME_DEV_SECRET
```

Ctrl+C/SIGTERM closes the listener and initiates closing handshakes; stalled
peers are force-closed after a one-second shutdown grace period. Windows Ctrl+
Break is also handled. `--run-for-ms N` optionally bounds a temporary run to
at most one hour. Port 0 requests an ephemeral test port; other ports must be
1024–65535. The default is 8765. The address is **hardcoded to 127.0.0.1**;
`--bind`, LAN/public interfaces and IPv6 binding are not supported.

Only path `/` and a Host of `127.0.0.1:<server-port>` or `localhost:<server-port>`
are accepted. Query strings and all browser Origin headers are rejected in this
phase; browser support needs an explicit origin policy in a later milestone.
Do not put credentials in URLs, logs, command-line arguments or tracked files.

An optional development SSH **local** forward may be run manually:

```sh
ssh -N -L 127.0.0.1:8765:127.0.0.1:8765 jimbo@nuc-server
```

This does not expose the server publicly and does not change SSH configuration.
Do not use public tunnels, reverse proxies, router forwarding, firewall changes
or permanent services. No such forwarding is started by build/tests.

## Messages and authorization

The application version `v:1` is independent of ESP-NOW protocol 4. Frames are
UTF-8 JSON text. Every request has integer `v`, printable string `id` (1–32
characters), and string `type`. Only the documented fields for a command are
accepted. Booleans/floats do not substitute for version numbers. JSON parsing
uses Boost.JSON once, with depth/size limits; it follows Boost's last-member-wins
semantics for duplicate object keys. There is no second interpretation layer.

The server initially sends a `hello` response containing no session state.
Unauthenticated peers may use `hello` or `authenticate`; all gameplay/state
commands require authentication within three seconds of TCP acceptance.
Three failed credential attempts close the peer. Comparison uses Monocypher's
constant-time `crypto_verify32` over BLAKE2b digests. Authentication returns a
fresh server-issued connection identity, not a player ID or ownership grant.

Examples below contain placeholders, **not usable credentials**:

```json
{"v":1,"id":"auth-1","type":"authenticate","secret":"<environment-secret>"}
{"v":1,"id":"join-1","type":"join","name":"Fighter"}
{"v":1,"id":"move-1","type":"move","direction":"east"}
{"v":1,"id":"events-1","type":"events","since":"0"}
{"v":1,"id":"resume-1","type":"reconnect","token":"<issued-ownership-proof>"}
```

| Command | Additional fields | Effect |
| --- | --- | --- |
| `hello` | none | Protocol/authentication status only |
| `authenticate` | `secret` | Development gate, once per connection |
| `join` | `name` | New player; returns player state and `reconnect_token` |
| `reconnect` | `token` | Binds to an existing disconnected player after proof verification |
| `move` | `direction` | Moves only this connection's participant through GameSession |
| `players` | none | Roster plus an atomically captured `event_cursor` |
| `map` | none | Immutable session graph and topology-only limitation |
| `events` | `since` | Pull bounded events strictly after the decimal-string cursor |
| `disconnect` | none | Unbinds this connection and retains its player record |
| `leave` | none | Removes this connection's player and revokes ownership proof |
| `ping` | none | Application-level health response after authentication |

There is no `create_session` or remote reset. The shared development gate is
not sufficient to take over a player. Each join returns a separate unpredictable
256-bit bearer reconnect credential exactly to its owner; only its hash is
retained by the server. Neither roster/events nor errors return those proofs.
Keep the credential private. The server rejects player-selection fields such
as `player_id` on move/reconnect. An active player cannot be taken over; only
one active connection may own a participant. Close/abrupt loss unbinds and
disconnects the player while preserving location. Reconnect requires both the
development gate and the retained bearer proof. Leave revokes it; server restart
loses all players and credentials. Disconnected records reserve session capacity.

Success responses contain `v`, echoed `id`, `type`, `ok:true`, and `data`.
Errors contain `ok:false` and `error:{"code":"..."}` without request payloads,
paths, stack traces or credentials. Examples: `NotAuthenticated`,
`AuthenticationFailed`, `OwnershipRequired`, `AlreadyConnected`,
`InvalidRequest`, `MalformedJson`, `UnsupportedVersion`, `UnknownCommand`,
`InvalidDirection`, `NoExit`, `Full`, `InvalidCursor`, `RateLimited`.
Protocol/frame violations can instead close with a WebSocket code, e.g. 1009
for oversized messages or 1008 for policy violations. Deadline expiry closes
the underlying connection without disclosing authentication information.

Request IDs correlate replies; they do not provide retry deduplication. After
losing an acknowledgment, reconnect and inspect authoritative state/events
instead of blindly replaying a movement request.

Events preserve GameSession's 32-event ring. Sequence values/cursors are decimal
strings so future JavaScript clients do not lose uint64 precision. A stale
cursor returns `ok:false`, `error.code:"HistoryGap"`, and `data` containing the
retained suffix, `gap:true`, and oldest/newest metadata. Refresh `players` and
resume from its atomically captured `event_cursor`; the map is immutable for
this process. A future cursor is rejected. Server restart invalidates previous
credentials and event cursors. No push subscriptions or per-client event queues.

## Limits and threat model

Source constants in `server_limits.h` cap live TCP/HTTP/WebSocket peers at 8,
participants at the shared four-player limit, requests at 4096 bytes, JSON depth
at 8, IDs at 32 bytes, names at 15 printable ASCII bytes, and serialized replies
at 16384 bytes. Each peer has at most one pending reply; reading resumes only
after writing it. The bounded input buffer permits 8 KiB framing/read-ahead
slack while Beast enforces the 4 KiB logical message cap. There are no unbounded
worker threads, outbound queues or gameplay queues.

Limits are 20 application requests per connection per fixed one-second window,
40 control frames per second, and 32 admitted TCP accepts per one-second
window. Authentication/HTTP deadline is 3 seconds, incomplete/idle read deadline
15 seconds, write deadline 5 seconds, and closing-handshake deadline 3 seconds.
One Asio event-loop thread serializes dispatch and all GameSession mutations.
Clients must stay active or reconnect after the idle deadline. Any admitted
client can read shared state, but only an owned handle can change its player.

This is private development transport, **not public-ready authentication**.
Plain WebSocket traffic is not encrypted on loopback; SSH can protect an
inter-machine forward. The shared gate admits trusted local testers, not distinct
real users. Bearer proofs can be replayed if stolen, do not rotate, and have no
persistent recovery. Any admitted tester can consume the small session capacity.
There is no defense against a privileged local process reading environment or
memory, an OS-wide resource attack, or unrelated local software. Future public
deployment requires real identity/admission, TLS, browser Origin policy,
token expiry/rotation/revocation, auditing, reconnect recovery, persistence and
operational security review. Localhost is not treated as authentication.

The real-socket security test launches the actual executable with ephemeral
in-memory credentials, checks Windows/Linux OS socket tables, exercises multiple
Beast clients and resource/auth/protocol boundaries, performs a server closing
handshake, and verifies no listener remains. It installs no Python packages.

Primary references: [Beast](https://www.boost.org/doc/libs/1_92_0/libs/beast/doc/html/index.html),
[Boost release](https://www.boost.org/releases/1.92.0/),
[Monocypher timing-safe comparison](https://monocypher.org/manual/verify),
[Monocypher releases](https://monocypher.org/download/).
