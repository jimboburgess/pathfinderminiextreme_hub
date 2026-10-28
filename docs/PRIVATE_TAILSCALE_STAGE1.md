# Private Tailscale Stage 1: implemented, activation gated

Starting point: clean master 327f388. This is native/server work only. ESP32
source, builds, graphics, saves, ESP-NOW and gameplay rules are unchanged.

## Actual implementation

The installed NUC Serve 1.104.1 CLI explicitly documents `unix:/path/socket`
proxy targets. No new intermediary is required. The proposed exact boundary is:

    personally authenticated, untagged owner device
      -> private Tailscale Serve HTTPS/WSS TCP 443
      -> protected Unix socket, root daemon as trusted local peer
      -> unprivileged pfme_server in network=none container
      -> original authoritative 1-4-character dungeon and C++ frames/audio

Explicit Linux startup requires all of --serve-socket, --serve-origin,
--serve-policy, --runtime party and a bounded --run-for-ms. It cannot be mixed
with a TCP --port. Windows rejects private mode and retains development mode.
No TCP listener is created in private mode. TCP development mode continues to
reject forwarded identity/forwarding headers; there is no header-trusting fallback.

The private process refuses root execution. It verifies an absolute socket path,
a canonical nonsymlink parent owned by its own UID and mode 0700, refuses any
existing socket/path, sets umask 0077 and socket mode 0600. Shutdown unlinks only
the socket inode it created. Every accepted Unix connection is checked using
Linux SO_PEERCRED BEFORE any request is parsed; only UID 0 is trusted. This
proves a root local peer, not an exclusive daemon executable identity. Root is
part of the trusted computing base, as is the installed root tailscaled. A
non-root process cannot impersonate it by owning the socket directory or forging
headers. Docker user-namespace/rootless mappings require a fresh UID review;
the prepared/tested daemon is not in those modes.

A root-owned, regular, single-link admission JSON file, not group/world writable,
is opened O_NOFOLLOW/O_NONBLOCK and read with bounded size/depth. Nonregular
files, including FIFOs, are rejected without a blocking open. It maps exact login strings
to unique stable principals; no display name or numeric player ID supplies
identity. Only one Tailscale-User-Login header is accepted on a trusted connection.
Missing identity, unknown/tagged users without identity, duplicate headers and
wrong Host/Origin are denied before HTTP/game data. HTTP assets are authorized
as well as WebSockets. Origin must be the configured exact HTTPS .ts.net origin;
CSP permits only its WSS endpoint. No X-Forwarded value establishes identity.

[Admission schema example](../config/private-playtest/admission.example.json)
is a nonsecret placeholder. The real owner file is outside Git and only a
pending artifact until root ownership and activation are approved. There is no
browser secret prompt or browser reconnect token in private mode. The browser
uses WSS, authenticates with an empty RPC against the already verified transport,
and tries principal-owned reconnect before creating a new character. Leave is
explicit and confirmed, retaining the existing combat guard. Development mode
and its private capability/reconnect proof remain available unchanged.

The authoritative Service retains the private PlayerHandle under the verified
principal. A disconnected character reserves its occupied slot; a new tab with
the same reverified identity resumes it. Duplicate connections cannot create a
second character, another principal cannot reclaim it, and a fifth occupied slot
is rejected without mutation. A numeric ID, development secret or supplied token
cannot select a character in private mode. Strictly increasing canonical decimal
request IDs reject duplicate/out-of-order actions before execution. One socket's
authentication cannot be replayed to change its principal.

## Expiration and revocation

A private connection has an absolute monotonic 900-second lease. Polls/actions
do not renew it. Expiration closes the socket and releases held input but keeps
the character; a fresh kernel/Serve-verified handshake can recover it if still
allowed. Policy reload occurs at most one second apart and validity is checked
before actions and on idle ticks. Invalid/unreadable policy fails closed.

The owner atomically replaces admission.json with a higher revision. Any
revision change preserves leases for unchanged login/principal mappings. Only
removed or remapped identities lose their leases; their characters are retained.
A regrant requires fresh authorization and cannot resurrect an old lease.
Unchanged leases keep their original absolute expiry. Invalid policy still fails
closed for all connections. To revoke every user, publish an empty users list
and wait for the revocation check before explicitly regranting. Previously, a
revision change invalidated all leases; that owner-only behavior was corrected
for group play. Remaining retained players reconnect only when necessary,
removed users cannot reauthenticate. This intentionally simple four-player policy
avoids stale authorizations. Lower revisions, or changed bytes with the same
revision, fail closed. Revocation is an owner file operation, never a player RPC.
An explicit regrant to the same stable principal may recover its retained
character; assigning that principal to another login is a trusted administrative
ownership decision and must not happen silently. Invalid/duplicate mappings are
rejected. Per-connection and per-principal command limits remain 20/sec, with
8 total connections, at most 2 WebSocket connections per principal (including
pending authentication), and bounded admission/budget storage. HTTP assets use
the global bound and existing short deadlines. Character capacity is
4, distinct from connection count.

State and ownership are persistent across socket/tab loss, not process restart.
This stage does not implement durable full-game snapshots. A one-hour bounded
runtime stops and loses in-memory adventure state; agree the playtest duration
before starting. No claims of permanent saves or production security are made.

## Complete policy and current live state

The owner supplied the complete original policy: one unrestricted network grant
and the existing member/self/check SSH rule, no other active ACL/group/tag rules.
The full original text, parsed backup, hashes and actual-address candidate live
in private task outputs, never the public repository. The owner reported the
owner-only candidate had no validation errors and clicked Save. The assistant
did not press Save or edit the live tailnet. Owner SSH through the NUC's Tailscale
IP was then verified with the existing host key. The SSH rule was preserved
byte-for-value. Serve/Funnel were still unconfigured.

The owner-only grant replaces source * with the owner's exact login while
preserving dst */ip * for the owner's existing administration. There is no friend
or shared-user grant in this candidate. Thus it intentionally changes access for
all non-owner identities; no additional administrator identity was requested.
The supplied complete policy has no active overlapping ACL or grant negating
that restriction. [The sanitized complete example](../config/private-playtest/tailscale-policy.fragment.json)
retains the original SSH rule and includes positive owner/negative uninvited tests
for both IP families. Personal addresses and logins are outside Git.

[Future friend fragment](../config/private-playtest/future-friends.fragment.json)
is NOT active. After identities are supplied and separately approved, add exact
friends to their own group and grant only that group -> game-node IPv4/IPv6,
tcp:443; update the application admission policy and its revision separately.
Do not add autogroup:shared as an OR alternative to the explicit group, put
friends in the owner grant, or restore wildcard-source grants. Share only the
approved node, not broad tailnet membership. Validate the complete resulting
policy and negative SSH/SMB/Homepage/Kuma/Navidrome/UniFi/music-bot/Docker/backend,
other-node and LAN checks before granting friend access. No friends are currently
invited or authorized by these files.

## Runtime isolation and prepared artifacts

[prepare_runtime.py](../ops/private-playtest/prepare_runtime.py) copies the
verified Ubuntu executable, its exact existing shared libraries and licenses into
a scratch image using the installed Docker daemon, with no package downloads or
base-image pulls. It refuses existing output directories/tags and starts nothing.
The image contains no shell, proxy fixture, expiry fixture, SSH keys or credentials.
Its binary/library hashes and immutable image ID are recorded for review.

The production runtime uses the locked `pfme-stage1` system account and its sole
primary group, with no interactive shell/home or supplementary Docker group.
`prepare_runtime.py --runtime-uid <service-uid> --runtime-gid <service-gid>` embeds
that nonroot identity in the image; activation explicitly repeats `--user uid:gid`.
The packaging operator remains separate. Active paths are root-managed under
`/var/lib/pathfinder-stage1`: root:root 0750 parent, service-owned 0700 socket
directory, root:service-group 0750 policy directory and 0640 admission file.
Docker mounts only the two child directories; the game needs no access through
the host parent or the operator home. The reviewed runtime has no host
network, no published ports, no capabilities, no-new-privileges, read-only root,
256 MiB RAM, 1 CPU, 32 PIDs and 128 file descriptors. The only mounts are its own
0700 socket directory (writable) and a narrow root-managed admission directory
(read-only). Network=none denies host/LAN/internet connections even if game code
is compromised. Default Docker seccomp and AppArmor were checked. This is a
restricted process/container, not a guarantee against host/kernel vulnerabilities.
Existing NUC containers and their settings are untouched.

## Verification and limits

Serve 1.104.4 rewrites Unix proxy Host to exactly `localhost`. Only the already
kernel-verified root Unix peer may use that backend Host. WebSocket Origin remains
the exact configured external HTTPS origin. Private CSP uses that configured
origin for WSS, never the rewritten Host or client-forwarded headers. Legacy TCP
continues to reject identity forwarding headers. No wildcard origin is accepted.

The real Linux protected-transport driver creates only temporary labelled scratch
images/containers, with no network, and removes its own resources. It verifies
unprivileged peer denial, trusted-root fixture acceptance, forged/duplicate/missing
identities, Host/Origin, authenticated assets, wrong-principal takeover, valid
reconnect, revision revocation, stale-policy rejection, expiration, fresh verified
recovery, Leave revocation, request replay and fifth-slot preservation. Four
characters execute original creation/town/movement and show different original
rooms over the protected protocol. The two-second expiry executable is explicitly
test-only, excluded from normal builds and never installed/prepared in the
production image. Production leases are 900 seconds.

The disposable regression fixture uses UID1000; separate production-image
verification uses the dedicated service UID/GID and root-managed paths.
In-namespace probes verify zero capabilities, no-new-privileges,
nonwritable image/policy, no Docker socket/home secrets and no external route.
The proxy fixture is a narrowly mounted ROOT TEST CLIENT, with DAC_OVERRIDE only
to access the protected socket; it is not the game runtime or an installed proxy.
It exercises actual kernel peer credentials and the real application code, but
is NOT a test of live Tailscale identity issuance, TLS/certificates or Serve WSS.
Those require the explicitly approved owner-device test after activation.

Focused Windows/Ubuntu checks cover the new policy/ownership unit tests, existing
security tests, browser authentication/control tests and four-player combat/wire
regressions. No ESP32/shared-game source changed, so unrelated firmware builds
are not repeated. See private task logs for exact results and source manifests.

## Exact approval gate, activation and rollback

[activation.review.sh](../ops/private-playtest/activation.review.sh) is intentionally
non-executable in effect: a guard exits before every commented command. The private
approval report supplies the actual runtime paths, service UID/GID, immutable
IMAGE ID and ORIGIN. No command
in it has activated Serve. Obtain explicit approval for root admission-file
installation, the exact container, private HTTPS/certificate enablement and the
one Serve mapping; do not infer approval from the owner's earlier policy Save.

After approval, start only the bounded isolated game, verify its Unix listener,
then run foreground Serve mapping only TCP 443 to that exact socket. Browser URL
is ORIGIN/play; socket is wss://same-host/. Serve may enable HTTPS/certificates;
its hostname can appear in public certificate-transparency logs while access
remains tailnet-only. No Funnel, HTTP redirect port, raw TCP, subnet route,
router forwarding or public DNS is introduced. Owner/admin SSH remains governed
by the already preserved owner grant; activation does not touch SSH/firewalls.

Rollback first stops the exact foreground Serve session. Fallback removes only
the verified root mount with `--https=443 --set-path=/ off`, preserving other
mounts. Check the recorded container ID, label, image, explicit identity and
exact two mounts before stopping/removing that runtime. Preserve the owner's narrow policy
and existing NUC services. Do not restore the old all-sources grant, reset all
Serve mappings, stop tailscaled or prune unrelated Docker resources. Old image
artifacts and admission backups remain available outside Git for review.

Next controlled check: the owner opens HTTPS/WSS from an authorized personal
Tailscale device; verify actual Serve login forwarding/Host behavior, browser
keyboard/zoom/audio, reconnect and a bounded shutdown. Stop on any unexpected
Origin/provenance/service access. Friends wait for their own identities, an
approved narrow full-policy update, admission update and device shares.

## Recorded Stage 1 checkpoint results

Windows focused regression set: 8/8 passed. Ubuntu focused regression set: 6/6
passed. Final Windows identity/private-client checks passed after the last file
hardening. The final real Unix/isolation driver also passed after adding FIFO,
wrong-owner, writable-policy, symlink, unsafe-parent and path-collision cases.
All native server/test/ops source hashes matched the Ubuntu copy; the prepared
immutable production image matched the final executable hash. Changed-file
credential/privacy and diff checks passed. No firmware/shared-game source or
configuration was changed. The live owner-only policy change was performed by
the owner, not the assistant. No Serve/Funnel, permanent runtime or friend share
was activated, and all eight existing NUC containers remained running.

## Final compatibility and service-identity checkpoint

The focused Windows transport/identity/private-browser tests passed 3/3, Ubuntu
transport/identity CTest passed 2/2, and the actual Unix regression passed with
localhost Host, external WSS CSP, malformed Host/Origin, ownership, reconnect,
expiry, revocation and direct-peer denial. The dedicated production image was
started only for a bounded network-isolated verification, checked with its real
service UID/GID, restrictive socket/policy metadata and only the two approved
mounts, then stopped and removed. The stopped image and root-managed account/
paths remain prepared. Real Tailscale TLS/WSS identity forwarding is still a
separate approval-gated owner-device test; no ingress has been activated.
