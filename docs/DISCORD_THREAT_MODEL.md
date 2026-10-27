# Phase 3H threat model — deployment gate CLOSED

Written before the Phase 3H implementation, 2026-10-27. Scope: a malicious
invited account, an unauthenticated Internet client, and a compromised game
process. Discord's iframe is an untrusted browser. No public endpoint is approved.

## Actual baseline

master is c7e130d (Phase 3F); Phase 3G is uncommitted. The original runtime
successfully plays title → creation → town → dungeon → combat → victory on
Windows/Ubuntu. One global handheld character/world/UI is leased to one
GameSession participant. The graph-only GameSession has independent rooms and
bounded shared events, but these are NOT the playable dungeon or combat state.
Multiple rendered playable characters, room privacy and shared playable combat
are missing. Do not substitute independent game processes for shared multiplayer.

The C++ backend uses an admission secret, in-memory reconnect capabilities,
loopback-only bind, strict Host/Origin, 4 KiB JSON/depth 8, 16 KiB replies,
20 commands/sec/connection, eight connections, authentication/write/idle
deadlines and serialized request/reply backpressure. No remote admin/file/shell
API exists. RLE screen patches contain original C++ pixels. The browser uses
textContent, bounded decoding, integer scaling and focus-loss button release.
Direct C++ request IDs correlate replies; they do not prevent replay. The shared
secret is suitable only for the existing private SSH development workflow.

## Boundaries and threats

| Surface | Attack and impact | Required control / residual risk |
|---|---|---|
| SDK/iframe | Faked user/channel/guild/participant claims; XSS steals sessions | Backend exchanges code and checks Discord OAuth application, scope, expiry and `/users/@me`; explicit ID allowlist. Ignore all client identity metadata. Minimal identify scope. SDK is not an identity oracle. |
| OAuth entry | Login CSRF, code replay, stolen code, token leak | Single-use expiring server challenge, same-origin POST, exact schema, bounded fixed HTTPS Discord calls without redirects. Codes are one-use at Discord. Browser/extension compromise can still steal its own credentials. |
| Sessions/reconnect | Account takeover, replay, stale socket after revocation | Random memory-only bearer sessions; bounded lifetime; periodic upstream validation; single-use short-lived WS tickets sent as first message, never URL. Upstream player proof remains only in gateway memory and keyed by verified ID. Logout closes sockets and removes local credentials. Revocation is bounded, not instantaneous. |
| Commands/state | Forged IDs/HP/loot, duplicate attacks, cross-player frames | Narrow action schema; monotonic per-socket sequence, no replay on reconnect; one outstanding RPC; C++ validates lease/state. No client result setters. Separate-room playable privacy is NOT implemented: deny second controller. |
| Pixels | Unauthorized screen reads; oversized patches; XSS text | Authenticate before upstream connection; screen allowed only for leased player. Fixed RGB565 codec and size limits; no HTML generated from names or payloads. Screen pixels themselves disclose that owner's state. |
| Ingress | Origin spoofing, proxy bypass, alternate route, compression abuse | Exact configured Discord proxy Origin/Host; fixed routes only; no arbitrary proxy; WSS/HTTPS at reviewed managed ingress; compression off. Origin is CSRF defense, never authentication. No trust in X-Forwarded-* or client IP. |
| Exhaustion | Code-exchange floods, connections, slow readers, nesting, pings, crashes | Global preauth budgets, bounded maps/bodies/connections, serial RPC and deadlines, per-identity bucket across reconnects, outbound caps, heartbeat, strict flat messages; container CPU/RAM/PID/fd limits still required. Fixed limits can deny legitimate users under attack. |
| Logs/config | Token/secret leakage, log injection, browser bundle secrets | Only fixed structured event categories/counters; no payloads, URLs, tokens, identities or exception stacks in ordinary logs. Server environment only; lockfile; no secret files in Git. Host/root can read process secrets. |
| Process/files | Memory corruption, file traversal, shell/container escape | No user paths/exec or dynamic upstream; read-only nonroot sandbox; no home/SSH/Docker mounts; no capabilities/new privileges; resource bounds. Native C++ remains memory-unsafe; sanitizers/fuzzing and independent review needed. |
| LAN/NUC | Compromised process probes private services/metadata/Docker | Prefer a separate disposable host/VM with no LAN route. Container alone does not guarantee egress isolation. No public ingress until tested firewall/network-namespace policy denies host/LAN/metadata and unrelated containers. |

## Design decision before implementation

Add a separately testable Node gateway with official Embedded App SDK client.
Keep the original C++ bind and private admission mechanism unchanged behind it;
never deliver its secret or reconnect token to the Activity. Gateway terminates
verified Discord sessions and translates only play/input/screen commands. Keep
the SSH development client intact. No shared-cookie dependency inside iframe:
short-lived tokens live only in JS memory; no storage/URL credentials.

One-controller runtime and missing real Discord application credentials are
explicit acceptance blockers, not security exceptions. Mock OAuth tests verify
our policy, not Discord's real grant/iframe/proxy behavior. No claim of production
security or three-player playable support is justified by passing tests.

## Application code cannot guarantee

TLS termination, DNS/account integrity, ingress route restrictions, container
kernel isolation, host patching, seccomp/AppArmor enforcement, egress denial,
physical LAN separation, browser/Discord-client integrity, provider availability
or a tester's compliance with written scope. All require deployment evidence.

Sources reviewed: [Discord networking](https://docs.discord.com/developers/activities/development-guides/networking),
[Activity guide](https://docs.discord.com/developers/activities/building-an-activity),
[OAuth2](https://docs.discord.com/developers/topics/oauth2).
