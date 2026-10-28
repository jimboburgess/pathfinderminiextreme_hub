# Serve identity provenance and exact access-policy follow-up

**Historical checkpoint 327f388.** Stage 1 has since implemented and locally
verified the protected Linux Unix-socket mode described here. The TCP mode still
rejects forwarded claims. See [current implementation and approval gates](PRIVATE_TAILSCALE_STAGE1.md).
The owner later supplied, validated and saved the owner-only policy candidate;
no Serve endpoint was activated. The original findings below are retained as
design history, not the current implementation status.

This checkpoint starts at clean master 7482805. It implements a fail-closed
transport boundary and regression tests, not remote authentication. Requirement 5
applies: the current TCP localhost topology cannot securely establish Serve
provenance. No per-person ownership/expiry/revocation feature is claimed complete.

## What administrator access revealed

The SSH account belongs to sudo but interactive authentication is required;
`sudo -n -l` cannot enumerate privileges, and `tailscale debug netmap` is denied.
No operator, sudo, SSH, socket or Tailscale setting was changed. The installed
root-owned tailscaled executable is root:root mode 0755. The daemon runs as root.
The LocalAPI socket's mode is not proof that a TCP request came from the daemon.

The existing authorized Chrome session DID provide read-only administrator
access. Policies showed one active network grant: all users/devices to all
users/devices on all ports/protocols. JSON editor confirmed:

```json
{"src":["*"],"dst":["*"],"ip":["*"]}
```

No edit, Save, invitation or permission action was performed. This is positive
evidence of broad access, not merely inability to inspect policy. It must be
narrowed BEFORE sharing the NUC. This observation does not prove which packets
can currently traverse the home router or independently audit all tailnet state.

Exact repeatable read-only UI steps:

1. Open https://console.tailscale.com/admin/acls/visual/general-access-rules in
   the owner's existing authenticated browser. Check the selected tailnet.
2. Access controls > Policies: inspect every source/destination/port rule.
3. Select JSON editor, or open https://console.tailscale.com/admin/acls/file.
   Read/copy the existing policy without typing into it or clicking Save.
4. Obtain the full grants/acls, groups, hosts, tagOwners, ssh, nodeAttrs,
   autoApprovers and tests sections, including the bottom of the file. Keep a
   private timestamped backup outside Git. No API key or operator change is
   needed. The local debug netmap, if obtained by the owner with
   `sudo tailscale debug netmap`, is sensitive and only the compiled node view;
   it is not a replacement for the full administrator policy.
5. Inspect Tests and, later, Preview rules using the intended identities. Do not
   add users just to make a placeholder policy test resolve. Do not Save a draft
   until the complete reviewed change is explicitly approved.

## Minimal policy replacement, not an additive exception

The revised [policy fragment](../config/private-playtest/tailscale-policy.fragment.json)
is a REVIEW replacement for the observed grants array. Preserve all other live
sections and build its group/hosts/tests into the private reviewed draft. All
accounts and addresses in the example are placeholders, not actual inventory.

Replace the wildcard-source grant with the same destinations/protocols restricted
to group:trusted-existing, whose explicit members are the existing trusted owner
(and only other existing trusted administrators after review). This preserves
those administrators' present access without granting friends it. Do not add
friends to this group. A blanket autogroup:member exception would also give a
friend full access if they were accidentally invited as a tailnet member.

Add autogroup:shared -> ONLY the game node's IPv4 and IPv6 aliases, tcp:443. Use
individual external device sharing, not broad tailnet membership. Accepting a
share and the application's private allowlist are both required. Substitute the
real NUC addresses, owner and test identities outside Git; preserve tag/service
flows explicitly if the full policy review finds any. Do not replace the whole
policy or delete SSH/owner configuration to apply this fragment.

The example asserts TCP 443 success and denies SSH/SMB, Homepage, Kuma,
Navidrome, UniFi/admin, Docker TCP and game-backend ports on both IP families,
plus UDP 443 and representative other-node/LAN destinations. Replace the reserved
example LAN address with concrete relevant destinations. Add every other actual
service/dynamic port found in the approved inventory. These are test assertions;
Tailscale grants have no overriding runtime deny. No wildcard-source grant,
legacy ACL, subnet-route grant or overlapping group may still permit friends
broader access. The game grant alone cannot cancel existing permissions. Unix
Docker sockets must separately remain inaccessible to the runtime.

JSON structure and draft intent are checked offline; no actual tailnet policy
compiler or live guest negative-reachability test is claimed. A complete real
policy diff and preview/test results still require review before activation.

## Why current identity headers are rejected

Serve documents removing client-supplied Tailscale identity headers and forwarding
its authenticated user login. That statement describes traffic THROUGH Serve;
it does not attest the origin of a connection arriving at an independent port.
Both tailscaled and an unprivileged local attacker reach 127.0.0.1:18765 with the
same TCP peer address. Arbitrary headers, X-Forwarded-For, display names, player
numbers, knowledge of the development gate and absence of a public bind cannot
prove which local process connected. Tagged clients may have no forwarded user
identity and must never be admitted through a fallback.

apps/server/transport.cpp now rejects Tailscale-* and forwarding-header claims
before any HTTP asset or WebSocket upgrade on its development TCP listener.
Valid direct loopback development clients still use the unchanged gate and
private capability. This deliberately prevents accidentally treating the current
binary as a trusted Serve backend. It does not add identity binding by ignoring
or trusting such claims.

## Smallest justified trusted connection follow-up

Use Serve's supported Unix-domain upstream inside a protected local runtime,
not a shared TCP listener for trusted headers. Proposed boundary:

    private Serve HTTPS/WSS :443
      -> unix:/run/pfme-private/backend.sock
      -> Linux-only explicit trusted-Serve mode in pfme_server

The directory/socket must be inaccessible to unrelated accounts; the server must
also verify Linux SO_PEERCRED on each accepted socket (the expected root daemon
UID, with root part of the trusted computing base). A path permission check alone
is insufficient because the game process owns its socket. Treat root compromise
as outside this guarantee; do not run the game as root. Verify protected parent
ownership, no unsafe symlinks, refusal to overwrite existing paths and safe
cleanup of only the owned socket. Require a kernel-verified peer before parsing
forwarded identity, and test non-daemon UIDs on the real transport. This is not
implemented here. No Unix listener or root helper was started.

That explicit mode must have no parallel TCP trusted-identity listener. Preserve
legacy Windows/loopback development mode. Use exact HTTPS Host/Origin and wss CSP;
verify Serve's actual Host forwarding behavior rather than weakening checks.
An isolated game node remains the fallback if the local trust/host-egress boundary
cannot be proven. No new proxy framework or Discord dependency is needed.

After this hop is implemented and tested, bind the single forwarded, allowlisted
login to an operator-assigned stable principal and a server-private retained
PlayerHandle. One principal owns at most one occupied character; connections are
separate from slots. A renamed login needs an explicit mapping update. Duplicate
active connections cannot create another character. Reconnect verifies the same
principal, not a supplied player ID or bearer proof from another identity.
Retain full Character/UI state on disconnect and on authorization expiration;
release it only through verified Leave under the existing combat guard.

Use an absolute 15-minute connection authorization lifetime on a monotonic clock.
A fresh trusted handshake renews authorization, never ordinary polling. Reload
an owner-protected allowlist/revocation file atomically; check before commands
and at most one second apart even while idle, closing revoked/expired sockets
and releasing held input. Missing/unreadable/invalid policy fails closed. Do not
silently transfer a character after a revoke/reinvite or on login renaming.
Owner revocation is not an ordinary player RPC. No shared-secret fallback in
this mode. Keep credentials/server capabilities out of logs, URLs and Git.

"Persistent ownership" here must include tab/socket loss. Current characters
survive only the game process; durable server-restart ownership and gameplay
snapshots require a separate reviewed design before claiming restart recovery.
No ESP32 saves or engine behavior are modified by this boundary checkpoint.

## Verification scope and remaining gates

New real-protocol regressions cover forged HTTP/WS headers (including mixed
case), JSON identity claims, shared-gate takeover of a disconnected character,
unknown numeric ownership, correct proof reconnect and Leave revocation. Existing
unauthenticated denial, deadlines, rate limits and party ownership tests remain.
These are NOT tests of unimplemented verified-principal expiry or revocation.
In legacy mode a stolen valid reconnect proof is still a bearer credential;
the verified-principal binding described above is required before friend access.

Before any remote playtest: approve/test the protected local transport and
identity mode; resolve individual allowlist, expiry/revocation and restart policy;
review the complete policy replacement and process isolation; approve the exact
HTTPS hostname/certificates; then separately approve Serve activation and one
controlled external tester. Verify actual HTTPS/WSS, allowed game access and
bounded negative checks to unrelated IPv4/IPv6 services from that identity before
inviting the group. No networking is activated by this commit.

Sources: [Serve identity headers](https://tailscale.com/docs/features/tailscale-serve),
[Serve CLI](https://tailscale.com/docs/reference/tailscale-cli/serve),
[additive grants and policy tests](https://tailscale.com/docs/reference/syntax/policy-file).

## Verified checkpoint results

Targeted server builds and security/HTTP/two-player reconnect tests passed on
Windows 3/3 and Ubuntu 3/3. Spoofed identity claims are tested over real sockets,
not only a mocked request parser. JSON/draft assertions demonstrate why appending
the narrow grant to the observed wildcard is insufficient and verify the proposed
replacement's intent; they do not execute Tailscale's policy compiler. Diff and
credential/privacy checks passed. Only native server/tests changed; the ESP32
source, configuration and firmware were untouched, so firmware was not rebuilt.
Serve/Funnel remain inactive and the existing NUC containers remain running.
The verified-principal connection, ownership expiry/revocation and live remote
access are explicitly unimplemented, not successful verification results.
