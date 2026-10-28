# First private group playtest: approval checklist

Preparation is not deployment. No real group result may be inferred from test
fixtures. Do not activate Serve, alter the live policy/admission, send invitations
or reuse a stale cidfile until the owner approves the complete named package.

## Roster and capacity

The owner and at most five individually approved friends may be admitted. Each
friend needs a personal Tailscale account on their own device. Do not use shared
accounts or make them administrators/members of the owner's entire tailnet.
Collect their actual Tailscale login and intended invitation email separately.
Keep real identities, principals and invitation links outside the public Git repo.

Assign one distinct stable application principal per approved login; preserve the
owner principal and previous ownership. Never silently reuse another person's
principal, accept a browser-supplied identity, or substitute an email claim for
verified Tailscale identity. Confirm the identity that accepts each individually
addressed node-share invitation in the trusted Tailscale sharing administration
view. A mismatch is a stop condition requiring a corrected approval, not automatic
admission of the accepting account. No blanket autogroup:shared grant.

The game has four OCCUPIED character slots, not six. Active connections and
occupied slots are different limits. Disconnect, lease expiration and revocation
retain a character's slot for its owner. A fifth/sixth admitted identity receives
Full and must wait. This is a clear rejection, not an automatic browser queue.
The character owner must explicitly Leave at a safe point to release the slot.
Combat can reject Leave with AdventureBusy; defer the handoff until safe. An
offline/revoked character cannot be evicted or controlled by another player.
No administrative force-eviction endpoint is introduced. If its owner cannot
return and Leave, the slot remains occupied until the bounded adventure ends.
Agree a rotation before starting. Late joins use the existing production character
creation/spawn rules; never copy another character or teleport the current party.

## Individual revocation correction

The previous owner-only image invalidated every lease for every admission
revision. Group preparation corrects this: retain an internal authorization
generation only when BOTH login and principal remain unchanged. Removed/remapped
identities lose their leases within the existing one-second check; unchanged
players keep their original socket, character and absolute 900-second deadline.
Adding another identity never renews existing deadlines. Regranting a removed
identity requires a new authorization generation; revoked leases do not revive.

Global admission revisions remain strictly monotonic. Invalid/unreadable policy,
rollback or changed bytes at the same revision still fails closed for everyone;
this intentional security failure behavior is different from removing one valid
entry. Use root-owned atomic replacement, keep the directory mounted read-only
in the game, and never remap a retained principal to a different person.
To revoke and regrant without changing a principal, first publish removal and
verify its connection is closed before publishing a higher-revision regrant.

This correction requires a newly tested immutable image. The historical owner-only
image remains a valid checkpoint but cannot satisfy individual group revocation.
Present the new image/digest and source diff in the next approval package; never
silently deploy it under approval for the older image.

## Complete policy and invitation approval

Obtain the COMPLETE current tailnet policy and existing node-share inventory,
including any HTTPS-enablement node attributes. Preserve owner administration.
Review grants AND legacy ACLs, groups, tags, node attributes, shared-node rules,
SSH policy, subnet/exit routes and overlapping wildcard grants. Access grants are
additive: a narrow new rule cannot subtract access granted elsewhere.

Prepare one full candidate and diff against the exact current backup. Grant only
the approved friend logins/group -> this node's exact Tailscale IPv4/IPv6,
tcp:443. No access to other nodes, subnets, SSH, SMB, Docker, Homepage, UniFi,
Navidrome, Kuma, music bot or backend ports. Preserve owner access. Include
positive game and negative other-service/node tests for each friend and an
uninvited identity. Validate the full candidate in Tailscale before approval.

The named approval package must contain:

1. Actual owner/friend login, stable principal, invitation email, acceptance
   verification method and current share status for each person.
2. Exact complete policy backup, full candidate, diff, tests and overlapping-rule
   assessment; root-managed admission backup/candidate/diff/revision.
3. Individually addressed invitations, limited to sharing this node; no broad
   tailnet invitation or unattended acceptance of an unexpected identity.
4. New verified immutable image and executable hashes, dedicated runtime identity,
   exact protected mounts/limits and bounded activation commands.
5. Per-friend revocation, scoped mapping/container rollback and recovery steps.
   A valid admission edit revokes only affected identities. To revoke network
   access as well, remove the matching grant/share with separate approved edits.

No named package can be completed with missing identities/current policy. Leave
all real friend admission and invitations pending until those inputs arrive.

## Real remote acceptance run

Keep LAN and Tailscale owner SSH working. Record the eight existing container IDs
and uptime, inactive ingress and unused443 before starting. Reuse the dedicated
service account/root-managed directories and isolation settings, but the newly
approved image. Review the previous root-owned cidfile after confirming its
container is absent; do not blindly overwrite/delete active records.

After explicit approval, use a single foreground private Serve HTTPS443 root
mapping to the protected Unix socket, with no Funnel/public router/firewall
changes. Check TLS and the exact HTTPS Origin/WSS CSP. Keep an owner-controlled
stop/rollback terminal open. Use the approved time limit, not a permanent service.

Record observations separately for two, three and four actual authenticated remote
players: each creates/controls their own original character, menus/240x240 view,
tile movement, separate rooms, shared events and available initiative behavior.
Confirm browser audio on each device and correct state after refresh/reconnect.
Attempt takeover/forged player ID only with the approved accounts and bounded
protocol tests; do not use browser JavaScript as authoritative game logic.

With four occupied slots, a fifth identity must receive Full without changing the
party. Disconnect must not make that slot available. At a safe point an owner
Leaves; a waiting identity retries and creates its OWN character. Confirm the old
owner cannot reconnect into that replacement. Test removal of one friend while
other original connections keep working; expiration must remain independent.

At least one genuinely different friend identity/device must perform the approved
network denial check. Local header spoofing/root fixtures are not substitutes.
Use a short finite TCP connect-only check of the exact NUC Tailscale addresses:
443 must work; the enumerated SSH/SMB/Docker/web-app/backend ports must fail. No
authentication attempts, scans of ranges, flooding or aggressive fuzzing. Probe
another exact tailnet device only if that device/address is explicitly in the
approved scope. Inspect routes/shared-node configuration for LAN exclusion; do
not blindly probe a home private address from a friend's network, where it could
refer to an unrelated device with the same address. A separate uninvited identity
must also be verified denied where available; report if none is authorized.

Capture bounded read-only docker stats for CPU/memory/PIDs plus disconnects,
error codes and elapsed time. Never record tokens/identity headers/secrets. Report
each live result and failure, not just simulation passes. Stop on any isolation,
ownership, origin, certificate or unauthorized-service failure; roll back the
owned ingress/runtime instead of weakening checks. Verify SSH, all eight original
containers, empty Serve/Funnel state and no443 listener after shutdown. Retain
policy/account/images as inactive review artifacts unless separate cleanup is
approved. Do not declare regular friend-session readiness before these real
cross-identity checks and gameplay observations pass.

Official references: [node sharing](https://tailscale.com/kb/1084/sharing),
[grants syntax](https://tailscale.com/docs/reference/syntax/grants),
[Serve](https://tailscale.com/docs/features/tailscale-serve).
