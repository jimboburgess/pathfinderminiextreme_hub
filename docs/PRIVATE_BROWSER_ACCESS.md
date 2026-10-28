# Private browser access: small review checkpoint

Status: preparation only. Starting master was clean at 3feaa75, following c4a8b7d.
No gameplay, server source, packages, network settings or runtime are changed by
this checkpoint. Discord is voice only. The examples are inactive review inputs,
not a deployment that can be launched with the current binary.

## Observed NUC state

Read-only inspection found Tailscale 1.104.1, BackendState Running, online, no
health warnings and MagicDNS enabled. Serve status was `{}`; Funnel reported no
Serve configuration. No game listener or TCP 443 listener was present. Certificate
availability was not established: status reported CertDomains null. Enabling
HTTPS/certificates, if required, needs approval; certificate hostnames can appear
in public certificate-transparency logs even though the service remains private.
Actual personal account names, tailnet addresses, keys and host inventory are
kept out of these public-repository examples.

SSH (22), SMB (139/445) and Navidrome (4533) listen on wildcard addresses, including
IPv6. Other app/admin listeners are LAN-bound. These existing listeners were not
changed or externally probed. Device sharing by itself is not game-only access.
The current editable policy and effective packet filter could not be inspected:
`tailscale debug netmap` denied access to the unprivileged SSH user. No operator
permission or sudo configuration was changed. Local status cannot prove the
absence of broad grants. Full policy review is a hard gate, not a passed check.
Existing NUC containers remained running throughout this inspection.

## Recommendation and exact boundary

For this small group, use the installed Tailscale Serve HTTPS reverse proxy and
individual device shares, conditional on the gates below. Serve is already the
narrow reverse proxy: adding nginx/Caddy or a new Node gateway is unnecessary for
this path. Integrate a small explicit trusted-Serve admission mode into the
existing C++ server in a subsequent focused source change.

    invited friend's personally authenticated Tailscale device
      -> approved shared node, TCP 443 only, HTTPS/WSS
      -> Serve TLS termination and authenticated identity header
      -> 127.0.0.1:18765, identity-aware pfme_server, --runtime party
      -> one authoritative dungeon, at most four occupied characters

The proposed URL is https://<approved-node>.<actual-tailnet>.ts.net/play and the
socket is wss://<same-host>/. Only /play, /play.js, /play.css and /audio.js are HTTP
assets; only / is the WebSocket upgrade. Unknown paths, file serving, CONNECT,
absolute proxy targets and extra upstreams remain denied. No browser-visible
backend port, host-wide bind or public ingress is needed. No Funnel.

Using the existing NUC node avoids creating infrastructure now, but is conditional
on verifying the ENTIRE additive policy for external shared users and an approved
process isolation boundary. Do not share this general-purpose NUC first and
restrict it later. If policy cannot exclude every unrelated service, use a
separate isolated game node/VM instead; do not loosen the requirement. A VM is a
fallback requiring separate approval, not a task implemented here. A dedicated
public HTTPS gateway is unnecessary for friends willing to use Tailscale and
would add public authentication/ingress exposure.

Tailnet access policies protect network access, not the host from compromised
native code. Host-local processes can forge HTTP identity headers to a TCP
loopback listener. Treat Serve and the local runtime boundary as trusted; other
local users/services are not automatically trusted. Before this mode is enabled,
approve isolation protecting that hop and denying the game access to host/LAN
services, home secrets and Docker socket. If host-local trust cannot be justified,
use a restricted Unix-socket hop/isolated game node rather than accepting headers
on an accessible loopback port. The current binary has no Unix-socket listener.
No container, service unit or firewall change is applied here.

## Current standalone authentication: what actually works

* apps/server/web/play.js sends the 64-hex development admission secret in the
  authenticate RPC. It is cleared from the input but retained in the page closure
  for reconnect; it is not written to browser storage. This is a shared gate.
* apps/server/service.cpp verifies its digest, then issues a random connection
  identity. That identity is not a verified person or Tailscale principal.
* join allocates a GameSession capability and a separate cryptographically random
  256-bit reconnect proof. The server stores the proof digest and private handle;
  display name/player number is never the ownership credential. Four occupied
  slots include disconnected characters; a fifth cannot replace one.
* play/input/screen/audio resolve the bound capability and active connection
  serial. Supplied player IDs are rejected by the schema. Another admitted user
  with the same development secret cannot reclaim an existing character without
  its independent proof. Duplicate active reconnects are rejected.
* reconnect needs admission plus the retained proof. A stolen proof remains a
  bearer credential; today's code does not bind it to a verified person, expire
  it, rotate it on reconnect or offer administrative per-person revocation.
* disconnect retains character state. leave clears its handle/proof/slot, subject
  to the existing combat/AdventureBusy guard. The browser currently offers
  Disconnect/Reconnect, not a Leave button, although the protocol supports leave.
  Closing a tab loses its memory-only proof; restarting the process loses state.

Thus a shared password does not itself permit takeover, but it does permit new
slot allocation and does not supply individual identity, recovery or revocation.
Tailscale access alone does not change this application behavior.

## Smallest necessary source follow-up (not implemented here)

Keep existing development mode and all strict checks. Add a separate fail-closed
Serve mode with an operator-configured exact HTTPS origin, trusted local ingress
and private allowlist. It must reject development-secret fallback.

1. apps/server/main.cpp and transport.h/.cpp: explicit mode/config; still local
   only. Require one exact public Host and Origin for WebSocket handshakes,
   trusted local peer path and exactly one valid Tailscale-User-Login header.
   Serve strips spoofed identity headers; the server must not trust arbitrary
   client headers or X-Forwarded-* values. Missing identity, uninvited login and
   tagged clients without user identity fail closed. Preserve deadlines and
   size/depth/connection/backpressure limits. Authorize HTTP assets too.
2. service.h/.cpp: map each exact allowlisted login to an operator-assigned stable
   principal, then one retained PlayerHandle. join/reconnect/leave use that
   principal; display names cannot claim identities. Keep existing private handle
   and proof server-side in this mode. A second connection never creates a second
   character, and another principal's proof cannot transfer ownership. Renamed
   logins require explicit mapping, never silent character reassignment.
3. Add 15-minute connection authorization expiry, reauthentication via a fresh
   Serve-verified handshake, allowlist/revocation checks at least every second and
   before actions, and immediate socket closure on revocation. Network policy
   updates alone must not be assumed to terminate existing game sockets. Limit
   concurrent/admission attempts per principal, not only per socket. A private
   owner-operated revocation file/API must never be a player command.
4. web/play.js/.html: explicit server-advertised private mode, wss for HTTPS,
   no shared secret prompt in that mode, and reconnect through newly verified
   identity. Preserve ws loopback development mode, graphics, audio and controls.
   Add an explicit confirmed Leave action with the existing combat guard.
5. transport.cpp: retain restrictive CSP, with connect-src only the exact private
   wss origin; exact Host/Origin, no wildcard and no Origin rewriting to localhost.
   No browser cookies or JS-held reconnect secret are needed for this minimal
   identity-on-every-WS-handshake model. Cross-site WebSocket attempts still fail
   Origin validation. An explicit Disconnect closes the connection but does not
   sign the person out of Tailscale; invitation revocation is owner-controlled.
6. tests/server: cover missing/duplicate/spoofed identity, tagged/missing identity,
   wrong Host/Origin, shared-password fallback rejection, different-principal
   reconnect, revoked/expired live connections, duplicate sockets, all 1-4 party
   sizes and fifth-slot preservation. Reuse existing protocol/runtime tests.

The current browser hardcodes ws://, backend Host/Origin accepts only HTTP
loopback, and CSP permits only that loopback socket. Merely enabling Serve or
rewriting Origin in a generic proxy would neither work securely nor implement
identity. Do not reuse the deferred Discord gateway/SDK or its vulnerable
transitive dependency as a shortcut.

## Review examples and policy gates

[Policy fragment](../config/private-playtest/tailscale-policy.fragment.json):
placeholder host/IP/account; only external shared users to TCP 443, plus negative
port assertions. It is NOT a complete policy and must not replace the owner's
policy. Resolve the placeholder to both actual node addresses; inspect existing
acls, grants, groups, SSH rules and node attributes. Explicitly remove/narrow any
overlapping rule that permits shared users broader access, while preserving
owner/admin access. Adding this grant cannot override an existing broad grant.
An assertion named deny is a policy TEST, not a runtime deny rule. Extend tests
to the real IPv6 address, other tailnet nodes, concrete LAN addresses, UDP and all
existing service ports. No tailnet policy validation has been claimed here.

[Backend contract](../config/private-playtest/backend-contract.example.json) is
an implementation/review specification, NOT accepted by today's binary. The
real allowlist, principal mapping and all credentials belong outside Git with
restricted permissions. No personal identifiers, session tokens or secrets are
in these examples. [Serve commands](../config/private-playtest/serve-commands.txt)
are inert text, including the proposed foreground-only command and rollback.

Approval is needed for: the actual node/HTTPS URL and certificate enablement;
the complete policy diff and isolation/local trust boundary; runtime account,
limits/egress protection and any later OS changes; the implemented identity mode
and revocation procedure; starting the bounded foreground game and Serve; then
each individual share/invitation. No approval is inferred from this preparation.
No package installation, permanent service or infrastructure rewrite is proposed.

## Later end-to-end gate and launch

After approval, test using actual intended external shared identities on their
own Tailscale devices. Each friend installs/logs in to Tailscale if needed,
accepts only the approved device share, and opens the private /play link in a
normal browser. No SSH or development tools are required. Verify HTTPS, actual
WSS upgrades, audio gesture unlock, reconnect and private views. Serve has an
[upstream report of HTTP/2 WebSocket issues](https://github.com/tailscale/tailscale/issues/20882);
this is a test dependency, not proof that installed 1.104.1 is affected. Do not
solve a handshake failure by enabling insecure WS or raw public TCP.

Confirm TCP 443 works and SSH/SMB/Navidrome/Homepage/Kuma/UniFi/Docker/backend
ports, other tailnet nodes and LAN routes are denied to those identities. Use
bounded connection checks only, not a broad or disruptive scan. Validate both
IPv4/IPv6 and policy tests. Then verify expiry/revocation including an already
open socket, stolen old capability with a different identity, slot reservation
and all party sizes. Stop on any unexpected access, ownership failure or service
impact; do not invite the full group until these checks pass.

## Rollback

There is no activated configuration to roll back in this checkpoint. Later:
close the exact playtest listener with `tailscale serve --https=443 off` and stop
only its owned foreground Pathfinder process. Confirm Serve/Funnel status and
listeners match the captured baseline. Revoke playtest shares/admission and
remove only the reviewed playtest grant; preserve existing owner rules and
services. Do not stop tailscaled, run serve reset, delete unrelated state or
restore an old whole policy over newer owner changes. Restore the reviewed
application artifact/config if required. Process shutdown currently loses game
state; it is not a durable save operation.

## References reviewed

* [Serve and trusted identity headers](https://tailscale.com/docs/features/tailscale-serve)
* [Serve CLI and scoped off command](https://tailscale.com/docs/reference/tailscale-cli/serve)
* [Sharing and access policies](https://tailscale.com/docs/features/sharing)
* [Grant syntax](https://tailscale.com/docs/reference/syntax/grants)
* [Policy tests and additive access](https://tailscale.com/docs/reference/syntax/policy-file)

The documents/configuration are checked locally; relevant existing security tests
are rerun without rebuilding gameplay or firmware. This does not verify an
unimplemented identity adapter, applied policy, live TLS/WSS, or process isolation.

## Checkpoint verification result

The inactive JSON examples parsed successfully. Their one game-only grant,
negative port assertions, HTTPS/Host relationship and unchanged server limits
were checked against source, with no credential/personal-network identifiers
found. git diff --check passed. Existing Windows security/HTTP/two-player
ownership-reconnect tests passed 3/3; the existing Ubuntu security test passed
1/1. No gameplay/firmware source changed, so native/embedded rebuilds were not
repeated. After tests, node Serve status remained empty, Funnel remained
unconfigured, and no game or TCP 443 listener was added. All eight existing NUC
containers remained running. Real tailnet-policy validation, trusted identity
handling and HTTPS/WSS through Serve remain unperformed approval gates.
