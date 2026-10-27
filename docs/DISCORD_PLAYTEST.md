# Phase 3H — preparation only; external deployment is NOT approved

## Status and blockers

The Activity gateway is implemented and locally testable, not deployed. The
official SDK is bundled locally, not loaded from a CDN. The actual C++ renderer,
240×240 pixels, integer zoom/Fit, keyboard and pointer controls are reused by
the build; only the admission UI/transport adapter changes. The existing private
SSH browser workflow is unchanged.

**Do not invite friends yet.** Blocking items:

1. The real handheld runtime has one global character/UI. Only one participant
   may play it; other identities cannot see/control its screen. GameSession's
   shared graph/events are a separate topology-only milestone. Fighter/Wizard/
   Rogue tests verify independent authentication and denials, not three playable
   characters, separate dungeon rooms, synchronized playable messages or combat.
   Next engine work must move per-player handheld state/UI behind GameSession
   capabilities and retain one shared world; multiple independent game servers
   would not satisfy multiplayer.
2. Current official SDK 2.5.0 requires Zod 3.x. npm audit reports high-severity
   [GHSA-mrhx-c7xc-m3w5](https://github.com/advisories/GHSA-mrhx-c7xc-m3w5)
   in Zod (<4.6.6), plus the SDK's transitive finding, with no compatible fix.
   This is one underlying advisory reported against two packages. SDK validation
   runs in the browser, not the gateway; practical exploitability has not been
   established. Do not force an incompatible Zod 4 override or call audit clean.
   Obtain a compatible upstream fix or independently reviewed mitigation before
   approving deployment.
3. No Discord application/client ID, secret or invited IDs have been supplied.
   No real grant, SDK iframe, Discord web/desktop, proxy Origin/Host or WSS test
   has occurred. No Discord configuration or invitations were changed.
4. No isolated runtime has been activated. Ubuntu has no Node installed; the
   Node gateway has not run there. No packages were installed. Native Ubuntu
   C++ verification is separate from Activity verification.
5. No reviewed ingress hostname, provider account, image digests, egress policy
   or sandbox enforcement evidence exists. These are required before exposure.

Read [the threat model](DISCORD_THREAT_MODEL.md) before code or deployment review.

## Implementation

`apps/activity` is an explicit, optional Node >=22 adapter. It does not enter
PlatformIO or CMake builds. `npm ci --ignore-scripts`, `npm run build` and
`npm test` prepare/test it locally. npm lockfile pins transitive integrity hashes.
The esbuild native optional package is installed by npm without lifecycle scripts.
Generated dist and node_modules are ignored, not source artifacts.
SDK console forwarding is disabled and browser console calls are removed by the
bundle build: upstream SDK parse-error branches otherwise print raw payloads.
Browser developer tools can still inspect a user's own memory/network; no frontend
technique conceals that user's access token from a malicious authenticated user.

For real C++ integration tests set PFME_TEST_SERVER to an absolute native
pfme_server executable, and include its compiler runtime in PATH on Windows.
Without it, the real-wire case is explicitly SKIPPED, not passed.

```powershell
Set-Location 'C:\Users\james\CLionProjects\pathfinderminiextreme_025\apps\activity'
$env:PFME_TEST_SERVER='C:\Users\james\CLionProjects\pathfinderminiextreme_025\build\native-playable\apps\server\pfme_server.exe'
npm ci --ignore-scripts --no-fund
npm run build
npm test
npm audit --omit=dev
```

Audit currently returns nonzero for the SDK/Zod blocker. Do not suppress it.
Do not run test fixtures on a public endpoint. Tests start only loopback
listeners, synthetic identities, a fresh random backend secret and a finite
40-second real C++ server. They stop owned processes. They do not contact Discord
or scan any LAN address. No fake-identity option exists in the production CLI.

## Trusted authentication flow

1. SDK starts in the configured Discord iframe; gateway serves only compiled
   entry/assets and fixed API routes. Entry query parameters are consumed by
   the SDK; guild/channel/instance/user claims never select server permissions.
2. Same-origin POST `/api/challenge` returns a random, single-use 60-second
   challenge. SDK authorize requests only `identify`, with that challenge as
   state. No guild/channel membership restriction is claimed.
3. Browser POSTs exactly challenge/code to `/api/login`. Backend consumes the
   challenge before exchange and sends the code to Discord's fixed HTTPS token
   endpoint using application credentials kept only in server environment.
4. Backend verifies `/oauth2/@me` application, scope and expiry, and obtains
   identity from `/users/@me`. Only configured Discord IDs may enter. No browser
   name or player ID establishes identity. SDK authenticate receives the short-
   lived OAuth access token as Discord requires; this is not the client secret
   or the backend admission secret. Browser compromise can steal its own token.
5. Gateway issues a separate random 256-bit session, stores its hash and keeps
   OAuth data only in memory. Sessions expire at the earlier of OAuth expiry or
   15 minutes. A fresh verified login invalidates that identity's old session.
   No refresh token is retained; reauthorization is required.
6. Bearer-authenticated `/api/ticket` issues a single-use 10-second WebSocket
   ticket. It is sent in the first text message to `/ws`, never in URLs, cookies,
   logs or subprotocol headers. Authentication must complete within five seconds.
7. After trusted identity verification, gateway authenticates its private C++
   socket, joins or reconnects the participant associated with that Discord ID,
   and retains the C++ reconnect proof server-side. A second socket for the same
   identity is denied; a different ID cannot supply/reuse another player's proof.
8. Each socket gets a fresh random epoch. Every command must carry that epoch
   and the next strictly increasing request ID. Old-socket replay, duplicates,
   gaps and unknown fields close the connection. Only play, input, screen, ping
   translate to C++; no player IDs, result setters, map, admin, files or shell.
9. Identity budget persists across sockets. Browser sees only its own authorized
   replies. C++ remains the gameplay authority and enforces the runtime lease.
10. `/api/logout` first invalidates the local session/tickets and terminates its
    sockets, then requests Discord token revocation. Revocation failure is logged
    as a fixed category. Local logout still succeeds. Logout pauses the character;
    only later verified login to that same identity can reclaim it.

Active sessions are rechecked with Discord after 30 seconds, including idle
socket heartbeats. With the five-second sweep and bounded API calls, revocation
has a delay (roughly up to 45 seconds under normal scheduling), not instantaneous
consistency. Provider failure denies further commands and closes affected sockets.
Logout/replacement during an awaited validation cannot revive a removed session.
Restart clears all sessions/proofs; the game is process-memory only. Restart
gateway and C++ together; gateway-only restart cannot recover old backend leases.

No ordinary player can restart either process. Admin shutdown is OS-level only.

Future gateway environment (do not populate secrets in chat or Git):

| Variable | Meaning |
|---|---|
| PFME_DISCORD_CLIENT_ID | Public application's decimal ID |
| PFME_DISCORD_CLIENT_SECRET | Gateway-only OAuth client secret from approved secret storage |
| PFME_DISCORD_INVITED_IDS | Comma-separated exact verified user IDs, 1–8 |
| PFME_DEV_SECRET | Private random backend admission value, shared only with C++ service |
| PFME_ACTIVITY_PORT | Loopback gateway port, default 18766 |
| PFME_BACKEND_PORT | Loopback C++ port, default 18765 |

`npm start` starts only the gateway on 127.0.0.1 after those settings and dist
exist. It does not start a tunnel, Discord application, C++ process or service.
It fails with a fixed startup error if required configuration is absent/invalid.
This is setup documentation, not permission to activate external ingress.

## Network architecture and origins

Proposed, not activated:

```text
Discord web/desktop iframe (untrusted)
  HTTPS/WSS https://<CLIENT_ID>.discordsays.com
  Discord URL mapping / -> one Pathfinder ingress hostname
  managed TLS ingress, exact route allowlist, global edge connection/rate limits
  outbound-only connector in dedicated disposable staging VM
  gateway 127.0.0.1:18766 (only fixed assets, /api/* above, /ws)
  C++ server 127.0.0.1:18765 (no public bind change)
```

Both services/connector should share an isolated network namespace inside that
VM so localhost does not mean the NUC host namespace. Do not use host networking.
The game process requires no network egress beyond gateway responses. Gateway
needs only Discord HTTPS, routed through a separately reviewed fixed-destination
egress policy. Connector needs only its provider. Deny home LAN, link-local,
metadata, other container networks and host services. Demonstrate the denial
from inside the game/gateway sandbox before adversarial testing.
Use separate UIDs and PID/filesystem isolation for gateway and game, even if
their isolated network namespace is shared. Supply a minimal separate environment
to each: the game must never inherit the Discord client secret or OAuth sessions.
Do not mount a shared secret directory into both or rely on one container UID to
protect secrets from another process with that same UID.

**Prefer a separate disposable VM/host without a route to personal LAN data** for
an invited penetration tester. A managed outbound connector avoids inbound router
ports but is not authorization or egress isolation. A small separate hosted VM
avoids a home-LAN tunnel altogether at the cost of patching, billing and account
security. A NUC-hosted sandbox is possible only after equivalent namespace and
egress enforcement is demonstrated; existing containers must remain untouched.

[Cloudflare Tunnel](https://developers.cloudflare.com/cloudflare-one/networks/connectors/cloudflare-tunnel/)
is a candidate outbound transport, not selected/configured infrastructure. No
quick tunnel, broad localhost mapping, generic forward proxy or new DNS record
is created. A generic zero-trust login interstitial is not assumed iframe-safe.

Gateway CLI accepts exactly `https://<CLIENT_ID>.discordsays.com` as browser
Origin/Host, no wildcard, null or forwarded-header fallback. C++ still accepts
only its original loopback Host and absent/internal loopback Origin. The trusted
ingress must preserve/set the reviewed Activity Host and preserve Origin while
dropping untrusted forwarded headers. Real proxy behavior must be captured and
reviewed using synthetic requests without logging credentials. Do not weaken
policy to make a failed Discord launch work.

CSP allows scripts/styles and game connections only from self; frame ancestors
are the explicit Discord stable/PTB/canary HTTPS domains. No arbitrary frame,
inline/eval script or external asset is enabled. No cookies or cookie-based CSRF
dependence: the session is JS memory only. HTTP POSTs require exact Origin and
Bearer for authenticated APIs. First-message tickets protect WS admission.
Discord's actual enclosing frames/CSP are still a live integration check.

## Resource and logging bounds

| Resource | Bound |
|---|---|
| HTTP headers/body | 8 KiB / 4 KiB; no encoded bodies |
| Pending HTTP auth operations | 4 |
| HTTP server connections | 16 |
| Global HTTP / socket upgrade | 120/min / 24/min |
| OAuth login attempts | 8/min globally |
| Invitations / sessions | 1–8 configured IDs / 8 sessions |
| Challenges / tickets | 16 each; expiring and pruned |
| Activity WS | 8 sockets; compression off; 4 KiB input |
| Identity requests | 16/sec across reconnects; browser schedules ~14/sec |
| Ordering/backpressure | One outstanding RPC; pipelining closes socket |
| Reply / buffered output | 16 KiB / 16 KiB |
| Ping/pong | 5-second heartbeat; 40 control events/sec |
| Ordinary security logs | Fixed event names only, at most 60/min |
| C++ | Existing 20/sec, 8 sockets, 4 KiB/depth 8, 16 KiB reply and deadlines |

Budgets are fixed windows and intentionally small for staging; boundary bursts
remain possible. Edge DDoS handling, fair per-IP throttling behind Discord's
shared proxy, log retention and alert delivery require a reviewed ingress. No
client-supplied forwarded IP is trusted. Fixed logs omit identities, tokens,
payloads and raw exception text; they indicate categories rather than forensic
packet contents. Keep any deliberate packet capture synthetic and separately
approved. OS CPU/memory/fd limits are essential, not optional application claims.

## Isolation review template

`apps/activity/staging/compose.review.yaml` is an **offline-only, inert profile**
requiring an operator-built pinned image. It exposes no ports and has no network,
so real Discord OAuth cannot work in it. It demonstrates constraints for local
synthetic tests; it does not claim the final egress-connected topology is ready.
No Compose command was run. Before a real VM image is approved, check runtime
ELF/glibc/libstdc++ compatibility, rootless/user namespaces, default seccomp,
enforcing AppArmor, no-new-privileges, capability drop, read-only filesystem,
tmpfs quotas, UID ownership, resource limits and crash cleanup from inside it.
No home directories, SSH keys, Docker socket or host data mounts may be added.

## Approval gates — do not skip

1. Complete shared playable multiplayer/room privacy with three actual classes;
   resolve SDK advisory; review OAuth/session/epoch code independently.
2. Provide public application ID and invited ID list through normal configuration;
   provision client secret only in approved secret storage, never chat/Git/browser.
   Configure a private Discord development application, Activity entry and minimal
   identify authorization. Check current portal tester/distribution controls;
   application discovery/launch permission does not replace our allowlist.
3. Build immutable images and run the offline harness in a disposable isolated VM;
   collect firewall/namespace/resource-denial evidence without touching live NUC
   apps. Add sanitizers/fuzzing and controlled crash tests in that environment.
4. Present exact proposed ingress hostname/URLs and Discord mapping, Origin/Host
   observations, TLS/headers, secret storage, egress rules, images, quotas, shutdown
   procedure, test scope and remaining findings to James. No hostname is approved
   now; `<CLIENT_ID>` and `<INGRESS_HOST>` are placeholders, not deployed URLs.
5. **Request explicit deployment approval only after that configuration is concrete
   and reviewable and security blockers are resolved.** No tunnel/DNS/ingress is
   enabled by the instructions in this document.
6. After approval, stage only exact Pathfinder routes through HTTPS/WSS; deny all
   other hostnames/paths. Configure Discord URL mapping `/` to that dedicated
   gateway endpoint. Verify desktop/web authorize, expired/revoked grant, correct
   forwarded Origin/Host, iframe CSP and socket proxy behavior before invitations.
7. Verify title/menus/creation/dungeon/combat with three real players; test keyboard
   focus loss, OS/Discord shortcuts, long Enter, pointer cancellation, reconnect,
   zoom/Fit, room-specific privacy/messages. Mobile has onscreen inputs but no
   demonstrated Discord mobile playtest; do not claim support based on desktop.
8. Agree written scope and stopping conditions, then explicitly invite IDs. No
   invitations or Discord-account changes are automatic.

Shutdown: stop connector first; revoke active gateway sessions (logout or stop
gateway), stop C++ runtime, remove approved ingress/DNS mapping, revoke OAuth
credentials if compromised, rotate backend admission secret and restart the pair.
No remote player reset/restart endpoint is provided. Document retained provider
logs and session loss; in-memory saves/players do not survive pair restart.

Sources: [official SDK](https://github.com/discord/embedded-app-sdk),
[Activity build guide](https://docs.discord.com/developers/activities/building-an-activity),
[networking/security](https://docs.discord.com/developers/activities/development-guides/networking),
[OAuth2](https://docs.discord.com/developers/topics/oauth2).
