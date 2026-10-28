# Cloudflare browser hosting — preparation, not deployment

No live Cloudflare application, tunnel, DNS route, connector or public game is
created by these sources. Tailscale/SSH and the eight existing NUC applications
remain separate and unchanged. Real edge HTTPS/WSS/browser acceptance needs a
separately approved owner-only activation after the gates below pass.

## Architecture and trust boundaries

Browser HTTPS/WSS -> Cloudflare Access allowlisted application -> outbound-only
Tunnel -> unprivileged cloudflared -> protected FRONT Unix socket -> unprivileged
JWT gateway -> protected BACK Unix socket -> existing authoritative C++ game.

The same exact game hostname serves HTML/scripts/styles/audio and the WSS upgrade
at `/`. No WebSocket bypass application/path. Protect the whole hostname with
Access; never an Everyone or Bypass rule. Native gameplay, four character slots,
original 240x240 C++ frames, input semantics and Web Audio command definitions are
unchanged. The gateway relays bytes, never reimplements/render-calculates gameplay.

Both game AND gateway use network=none, readonly root, no host PID/network, no
capabilities, no-new-privileges, CPU/memory/PID/FD limits. Game mounts only back
socket RW and its root admission RO. Gateway mounts front/back IPC directories and
its root config/JWK cache RO; no tunnel credential, home, SSH or Docker socket.
Connector mounts only front IPC plus its own read-only configuration/credential
and release-gate files. It cannot mount the game socket or admission/key cache.

Game UID stays the dedicated unprivileged identity. A distinct gateway UID uses
the narrow backend IPC group as primary and frontend IPC group as its sole
supplementary group. A third UID for cloudflared belongs only to the frontend
group. Numeric IDs in examples/tests are not authorization to create host users;
allocate/review real identities and group membership at deployment approval.

Back directory game:backend-group0710, back socket0660. Front directory
gateway:front-group0710, front socket0660. Root-owned parent/policies/config/key
files (0640) and readonly mounts prevent unprivileged policy/key replacement.
The C++ Cloudflare mode verifies exact gateway UID AND primary GID with
SO_PEERCRED before parsing requests. The gateway verifies exact connector UID
before reading HTTP. Groups permit socket access, but group membership alone
does not authenticate a peer. Wrong/root/direct peer processes are rejected.

`--cloudflare-*` is a separate Linux native mode; mixed/incomplete Cloudflare and
Tailscale flags cannot fall back to TCP. Existing Tailscale mode still requires
root peer UID0 and `.ts.net` origin. Its permissions/configuration are untouched.
Only one game ingress mode runs in a process; Tailscale administration stays
available independently. No durable world snapshots or restart migration is added.

## Authentication: email codes versus passwords

Recommended initial owner test: Access email one-time PIN with EXACT explicitly
allowlisted addresses, or an already trusted OIDC identity provider with MFA.
Friends need a browser and access to their mailbox/provider, not Tailscale.
Email OTP proves mailbox control; it is NOT a Pathfinder username/password account.
Do not create a shared login/password. Access is the edge policy/login provider;
Pathfinder is still responsible for character authorization and hostile actions.

If individual passwords are wanted later, integrate a suitable OIDC/SAML identity
provider that manages passwords/MFA/reset/rate limits. Do not build a password
database into the C++ game. Map the new verified provider identity to the existing
stable game principal only after explicit administrative verification. Same email
text is never sufficient to merge owners. Access sub is account-specific and may
change if a user is deleted/re-added or moved to a different organization.

The gateway validates `Cf-Access-Jwt-Assertion` with cryptography's RSA
PKCS1v1.5/SHA256 verification, not custom cryptographic algorithms. Header is
strict RS256/JWT/kid; no none/HMAC, key URLs, embedded public keys or critical
extensions. Verify signature, exact configured issuer/audience, application type,
nonempty subject, integer exp/iat, optional integer nbf and current clock. Reject
malformed/noncanonical base64url, duplicate JSON fields, excess size/depth, unknown
kid, service tokens, expired/future tokens, wrong issuer/audience/signature.
Signed email is not the ownership key. Unsigned email/user headers are ignored.

Identity login is `cf-` + SHA256(verified issuer + NUL + verified sub). The root
native admission maps that opaque login to a stable existing game principal.
For the owner, preserve the existing owner principal after verifying the Access
subject; do not automatically bind the first visitor or expose registration.
Get the subject from trusted Access administration/signed assertion, not a
browser-submitted email. Optional inspect_assertion.py verifies a token in the
OWNER'S interactive terminal and prints only identity login/expiry. Never send
the token to chat, jwt.io, source control, process arguments or ordinary logs.

The gateway forwards only a newly generated Pfme-Verified-Identity value over the
kernel-authenticated backend socket. Cookie, JWT, forwarded email/IP and all other
browser identity fields are stripped. C++ accepts this header ONLY in the explicit
Cloudflare mode with the expected peer UID/GID; it rejects raw Access assertions
and Tailscale identity claims on that hop. Tailscale never accepts the Cloudflare
private header. Native principal ownership, replay rejection, reconnect, safe
Leave, individual revocation and four occupied slots remain authoritative.

## Key rotation, expiry, revocation and logout

Public keys come ONLY from the configured team's HTTPS
`/cdn-cgi/access/certs`. refresh_keys.py refuses redirects/environment proxies,
validates TLS, size/key type/kid/strength, and writes a NEW pending file. It does
not replace active trust. Operator reviews and atomically root-installs the
complete current+previous JWK set. No request-controlled issuer/jku/key fetch.
Unknown kid fails closed until an approved refresh. Current and previous keys
are selected by kid, not a hardcoded PEM. Cloudflare documents automatic rotation;
the local cache has a hard one-hour freshness bound regardless of vendor overlap.

Refresh is deliberately operator-controlled for the first bounded test; no cron
or background privileged updater is installed. A regular deployment needs an
approved, isolated, monitored update job. Failure/stale keys stop authorization,
not an indefinite stale-key fallback. Active connections recheck expiry/key trust
every half second; key removal closes affected tokens. Long-lived sockets end no
later than JWT expiry or an absolute 900 seconds, with independent native leases.
Activity never extends a lease. Reconnect needs a fresh valid authenticated hop.

Native admission removal revokes only that identity within its existing check;
other players keep their original deadlines. Character slots survive socket loss
or revocation; an explicit safe Leave releases a slot. A revoked offline character
does not become another player's property. Process shutdown still loses memory-
only adventure state. Five admitted people do not imply five character slots.

Logout procedure: Disconnect safely, then navigate to Cloudflare's
`/cdn-cgi/access/logout`; do not call Disconnect a full identity-provider logout.
For immediate all-device gameplay revocation remove native admission as well as
the Access policy/session. Access revocation may not notify an existing upgraded
socket immediately; the native allowlist/expiry and gateway checks bound that
gap. JWTs are bearer credentials: a stolen still-valid JWT can replay the SAME
identity until expiry/revocation. No claim of universal token-replay prevention.

## Connector isolation: mandatory before exposure

Docker bridge networking without published ports still reaches host/LAN; that is
NOT acceptable isolation. Connector must use its own reviewed network namespace,
with default-drop output allowing only the pinned connector version's required
Cloudflare PUBLIC endpoints/ports and approved public DNS. Force HTTP2/IPv4 for
the first test, minimizing egress; no generic localhost/LAN proxy, private route,
host network or public listen. Review Cloudflare's current endpoint requirements
at activation; template destination sets are EMPTY and fail closed by default.

connector_gate.c performs no network calls before exec. It waits at most120sec
for a fresh64-byte root-controlled release nonce (regular file, no symlink,
single link, not group/world writable). Start the unprivileged connector behind
this gate, validate exact labeled container/PID and ensure its namespace differs
from the host. Load/check connector.namespace.nft ONLY through nsenter into that
owned namespace, populate exact reviewed public endpoint sets, test host/LAN
denial, inspect rules, THEN root-release the nonce. Connector retains no NET_ADMIN
capability. Never run that nft file directly on the host or release a stale gate.

Tests apply/check the empty deny-by-default template in an OWN disposable fake
connector namespace (network=none), and verify the gate before/after root release.
This verifies syntax/kernel scoping/gate behavior, NOT actual cloudflared egress
or Cloudflare connectivity. The real pinned connector is not installed/run.
Its network namespace must be tested with real approved egress rules before
owner-only public exposure. If that fails, stop rather than allowing host/LAN.
Root/Tailscale/Docker/kernel, gateway and Cloudflare account/keys remain TCB.

## Account/domain setup and approval gates

1. Confirm Cloudflare account control, intended hostname and Zone/Zero Trust team.
   Owning a domain does not imply its DNS is Cloudflare-managed. Back up current
   DNS/MX/SPF/DKIM/DMARC and existing website records before any approved DNS/NS
   change. Preserve existing record values and service behavior; do not rehost
   mail/website services as a side effect. Review DNSSEC/registrar DS handling
   explicitly before any separately approved nameserver migration.
   If retaining the existing authoritative DNS provider, verify eligibility for
   Cloudflare partial/CNAME setup (currently Business/Enterprise); it is not a
   free-plan assumption. Otherwise full-zone delegation needs a separately
   approved, complete record-preservation plan.
2. Create an owner-only self-hosted Access application covering the exact WHOLE
   game hostname before public routing; explicit email allow rule, no wildcard,
   Everyone, Bypass or service-token admission. Select provider/MFA and short
   session duration. Record exact team issuer and application AUD.
3. Obtain and verify owner sub; root-map its opaque identity to the stable owner
   principal. Keep native and Access allowlists coordinated, owner-only initially.
4. Approve a specific cloudflared version/image digest and acquisition/install,
   dedicated identities/directories, credentials storage, connector-only egress
   namespace/rules, image digests/resource limits and JWKS refresh procedure.
   No such approval is inferred from ownership of the domain.
5. Prepare stopped game/gateway/connector artifacts, freeze configs/hashes, run
   local tests. The Cloudflare native mode requires a new game image; do not
   silently reuse an older image lacking this adapter mode.
6. Present exact public hostname/URL, account/AUD/issuer, owner allowlist and
   principal, DNS diff, Tunnel route/config/credentials scope, per-container
   mounts/groups/limits, egress rules/test evidence, rollback and risks. Obtain
   explicit approval BEFORE a public hostname or live tunnel/DNS change.

## Owner-only activation and rollback

ops/cloudflare/activation.review.sh and rollback.review.sh are guarded/nonactivating. Follow its ordered
steps only after all exact values and approvals are supplied: fresh keys/root
admission, bounded network-disabled game, bounded network-disabled JWT gateway,
gated unprivileged connector, verified connector namespace filter, root nonce
release, then separately approved one-host DNS route. Config tunnel.example.yml
has one Unix application origin and terminal404; no other service routes.
Protect all paths with Access. Keep no token in image/Git/command arguments.

Use only the new Cloudflare directories/containers; existing Tailscale/root
admission paths are not reused or changed. Record exact container IDs/labels,
image digests, users/groups and mounts. Keep existing owner LAN/Tailscale SSH.

After approval test in a NORMAL browser outside Tailscale: Access login, valid
HTTPS, authenticated WSS, original creation/menus/movement/audio, refresh to same
character, logout/expiry, unauthorized identity/direct-backend rejection. Then
approve invited friends and record two/three/four real remote players, fifth-slot
rejection, separate rooms and resource usage. Simulated tests are not live edge
or browser playtests. Cloudflare can terminate WebSockets during network updates;
reconnect authorization must restore the owner, not create a duplicate.

Stop on any boundary failure. Rollback first stops/removes ONLY the recorded
labeled connector after checking exact image/user/mounts. That closes the outbound
tunnel. Then stop/remove the owned gateway and game. Inspect for other tunnel
replicas; revoke only this tunnel credential when approved. Remove only this new
hostname route/Access app if approved, never the whole DNS zone or other apps.
Verify unavailable game, no public backend listeners, preserved eight containers
and both SSH paths. Keep inactive artifacts/logs; no broad prune/cleanup.

## Tests and honest limits

Windows/Ubuntu tests exercise real RSA signatures, claim rejection, malformed
tokens/duplicate JSON, kid rotation/cache expiry, origin/config and native mode
options. Existing Tailscale protected-transport regressions remain passing.
Linux tests exercise BOTH protected Unix hops and real C++ gameplay: wrong UIDs,
unsigned identity claims, root admission, four independent original frame streams,
separate rooms, original audio commands, fifth-slot rejection, ownership and
reconnect, per-identity revocation, JWT expiry and key removal on active sockets.
Fixtures use generated keys/identities and network-disabled containers, not real
Cloudflare accounts. No live edge certificate, Access login/cookie, cloudflared,
public HTTPS/WSS, real browser audio or external friend outcome is claimed yet.

No ESP32/shared-game, save or ESP-NOW sources changed; unrelated firmware builds
are not repeated. Dependencies are existing Python3.14 with cryptography46.0.5 on
Ubuntu and50.0.2 in the selected Windows runtime. RSA/SHA256 uses vetted APIs;
unused OpenSSL legacy algorithms are explicitly disabled in gateway images.
package_gateway.py copies installed libraries into a stopped scratch image and
records file hashes/versions; it installs nothing. Pin/review runtime patches
and licensing before public deployment. No new Node/browser SDK dependency.

## Official references

- [Validate Access JWTs and rotation](https://developers.cloudflare.com/cloudflare-one/access-controls/applications/http-apps/authorization-cookie/validating-json/)
- [Access user/application claims](https://developers.cloudflare.com/cloudflare-one/access-controls/applications/http-apps/authorization-cookie/application-token/)
- [Email one-time PIN](https://developers.cloudflare.com/cloudflare-one/integrations/identity-providers/one-time-pin/)
- [Tunnel egress requirements](https://developers.cloudflare.com/cloudflare-one/networks/connectors/cloudflare-tunnel/configure-tunnels/tunnel-with-firewall/)
- [WebSocket support/limitations](https://developers.cloudflare.com/network/websockets/)
- [Vetted cryptographic backend and legacy-provider setting](https://cryptography.io/en/latest/openssl/)

[Cloudflare partial-zone requirements](https://developers.cloudflare.com/dns/zone-setups/partial-setup/).
