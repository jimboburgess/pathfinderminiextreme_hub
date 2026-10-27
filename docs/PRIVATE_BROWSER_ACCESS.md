# Private browser playtest access — review proposal, not deployment

The immediate game is a standalone normal-browser client for 1–4 players. Discord
is voice only. Nothing in this document authorizes software installation, network
policy changes, sharing, public ingress or invitations.

## Practical recommendation

Prefer a dedicated isolated game node/VM on the Ubuntu NUC, shared individually
over Tailscale, with private HTTPS Serve and access grants limited to the one game
HTTPS port. Friends install/login to Tailscale once, accept their own device-share
invitation, then open a normal browser link. They need no SSH, compiler or shell.
This is not zero-install access; if that is a requirement, use the alternative
below after a separate security review.

[Device sharing](https://tailscale.com/docs/features/sharing) is scoped to a shared
machine and respects its owner's policies. It does not itself mean only the game
port is available. Review the entire effective policy: additive broad grants must
not also permit shared users to other ports. Test allowed game traffic and denied
SSH, application/admin ports and other nodes with the exact intended identities.
Do not advertise subnet routes or an exit node. Do not share the general-purpose
NUC with default broad access; an isolated game node is the proposed boundary.

[Serve](https://tailscale.com/docs/features/tailscale-serve) provides private HTTPS
proxying and identity headers, including for users accepting a device share.
Funnel is public-facing and is excluded. HTTPS service naming/certificate and
WebSocket behavior must be verified with the selected client versions before
approval; no hostname is configured now.

## Identity and ownership

Replace the shared development gate as the primary browser identity with verified
individual admission. Serve removes client-supplied Tailscale identity headers
and supplies authenticated user information. Trust those headers only through a
protected Serve-to-gateway path inside the isolated node, with the gateway on
loopback and direct backend access denied. A raw client header is never proof.
Use a configured stable identity/allowlist mapping, not a display name. Missing
identity (including unsupported tagged-device cases) must fail closed, not fall
back to a shared password.

Issue separate random expiring game sessions and one-use WS tickets; keep backend
admission and reconnect proofs server-side. Bind the verified identity to its
existing GameSession capability and occupied character. Preserve exact Origin/
Host/schema/sequence/epoch checks, per-identity resource limits, logout/revocation
and private views. Do not reuse Discord-specific Origins or blindly trust an
X-Forwarded-* header. Network policy and game authorization are separate defenses.

This standalone identity adapter is planned, not implemented. The existing
shared development gate remains suitable only for its private development use.

## Alternative: isolated HTTPS gateway

A separate HTTPS gateway with verified per-person OIDC/passkey/other reviewed
authentication can allow friends to use only a browser. It requires public-facing
TLS/ingress, identity-provider configuration, session/CSRF/Origin handling,
resource protection and strict fixed Pathfinder routing to an isolated backend.
It exposes a larger unauthenticated attack surface than the proposed private
network, so it is the second choice for a few friends who can use Tailscale.
No generic localhost tunnel, public game bind, host proxy or public endpoint is
approved. If selected, identify exact provider, routes, URLs and trust boundaries
before asking for approval. No outside authentication page/Discord iframe
compatibility assumption is necessary for this standalone client.

## Isolation before access

The game node must not have home-directory credentials, SSH keys, Docker socket,
host networking or unrestricted LAN/metadata access. Separate game and admission
process identities/secrets and enforce read-only filesystems/resource limits.
Measure the four-player native runtime. Prove that an authenticated malicious
tester cannot reach unrelated NUC services through the game namespace or proxy.
Sharing a node does not isolate a compromised process from its host/LAN.

## Concrete configuration required for approval

* Node/VM boundary and immutable runtime build; required software and versions.
* Exact private HTTPS hostname/port/routes and WS endpoint; no wildcard mapping.
* Effective Tailscale sharing/grant policy, or alternative ingress/OIDC policy.
* Individual identity mapping, game sessions, expiration, revocation and reconnect.
* Origin/Host, TLS/CSP, request/connection/rate/media limits and secret storage.
* Filesystem/process/egress denial evidence and impact on existing NUC services.
* Safe test accounts/scope, stopping conditions and one-command session shutdown.
* Actual N=1..4 gameplay/audio results, fifth-slot rejection and ESP32 regressions.

Only after explicit approval: install/configure the chosen software, test the
negative access policy, start the bounded game session, then invite individual
testers. Do not silently change an existing tailnet policy or shared container.

## Friend launch experience to deliver

1. Accept the individual private invitation; authenticate with their own account.
2. Open the approved HTTPS game URL in a normal supported browser.
3. Create or resume only their character; click/press to enable local sound.
4. Use original controls/zoom and Discord voice independently; reconnect restores
   their retained slot. PartyFull explains occupied capacity without replacing anyone.
5. Owner ends the playtest and revokes access/sessions. No friend gets NUC admin,
   SSH or other-application access as a side effect.

The optional apps/activity SDK dependency is outside the standalone CMake/server
and page. Keep it deferred; do not bundle/import it or declare its Zod advisory
fixed. A standalone client must have no runtime Discord/SDK/OAuth dependency.
