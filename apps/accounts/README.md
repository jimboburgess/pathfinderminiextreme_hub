# Pathfinder friend hosting

One authoritative C++ game, a small Django account/Unix adapter, and cloudflared.
Accounts are administrator-created; no public registration or remote admin API.
Any of approximately5–10 invited accounts can sign in. The C++ session permits
four occupied characters. Extra signed-in visitors see Party Full and can retry
after a character uses Leave. Disconnect preserves ownership and the occupied slot.

## Prepare, without public activation

Use the reviewed pinned images in a private `.env`, copied from `.env.example`.
Keep this directory mode0700. Never put `.env`, passwords, cookies or tunnel tokens
in Git. `pfme.sh start` refuses without the separately approved activation marker.

```sh
sh pfme.sh init
sh pfme.sh friend create jimbo
sh pfme.sh friend create friendname
sh pfme.sh friend list
```

Create/reset prompts for a password privately in the administrator terminal; no
password arguments. Passwords must contain at least 2 characters; there are no common-word, numeric or similarity restrictions. Deliver it
privately to that friend. They can change it at `/accounts/password/` after login.

```sh
sh pfme.sh friend reset friendname
sh pfme.sh friend disable friendname
sh pfme.sh friend enable friendname
```

Each command synchronizes the native root-owned allowlist. Disable/reset revokes
that user's sessions; others retain their characters. The UUID ownership mapping
survives password changes, disconnects and adapter restarts. Accounts, Django secret
and sessions persist in named volumes. Do not use `down -v` or Docker prune.

## Public activation — separate approval required

Use a REMOTELY managed Cloudflare Tunnel. Its entire configuration is
`remote-tunnel.example.json`: one game hostname to `unix:/front/gateway.sock`,
exact external Host, terminal404 and no private network routes. No router ports.
Do not run Cloudflare's displayed service-install command or use host networking.

After approval, create/configure only that named tunnel and its game CNAME; store
its token with `sh pfme.sh secrets`. This hidden prompt writes two separate private
Docker secret volumes. No token appears in Compose arguments/environment or logs.
Remove ONLY the existing Access application protecting this game hostname after
the account adapter has passed its offline checks, as part of the approved
transition. Keeping Access would impose a second login. Preserve all other apps,
apex/WWW/cloud DNS, Microsoft365, SSH and Tailscale.

```sh
# Only after the final reviewed public activation approval:
touch .public-activation-approved
sh pfme.sh start
sh pfme.sh status
sh pfme.sh logs
sh pfme.sh stop
sh pfme.sh restart
```

`start` initializes only owned volumes, starts the network-disabled game/accounts,
starts cloudflared behind the root-controlled gate, installs its namespace-only
default-drop filter and checks forbidden host/LAN destinations BEFORE releasing
the gate. No host firewall change. Only Cloudflare tunnel UDP7844 (QUIC) and public DNS
egress are allowed. Exceptions stop the owned deployment. The game has no scheduled shutdown (`--run-for-ms 0`); explicit restart begins a NEW in-memory adventure.

Players visit `https://game.noburdenmusics.com`, enter their own credentials, then
Connect. Original graphics, integer zoom, handheld controls and Web Audio remain.
Backend identity headers come only from the authenticated adapter UID/GID. Legacy
`--cloudflare-*` native flags name the existing protected proxy transport; this
account deployment does not accept Cloudflare/Tailscale identities or shared
development secrets. On the protected connector socket, Cloudflare-overwritten
X-Forwarded-Proto must be HTTPS; HTTP redirects to the canonical HTTPS site and
missing/unknown protocol fails closed. No identity comes from forwarding headers. Cookie sessions are Secure/HttpOnly/SameSiteStrict, four
hours by default. The unchecked Remember me option grants a fixed seven-day login
on a personal device; logout, disabling and password reset still revoke access.
Existing logins keep their original deadline. This does not extend the adventure
lifetime or persist dungeon state. Native connection leases are15min; Reconnect/refresh revalidates ownership.

## Private Homepage tile

After approval/start: `sh pfme.sh homepage` appends ONLY a Pathfinder section to
`/home/jimbo/homepage/config/services.yaml`, retains a private exact backup and
sets the result mode0640. No existing container restart or other dashboard edit.
Homepage's server fetches bearer-protected
`http://192.168.1.216:18766/status`. This LAN-only endpoint is NOT in the tunnel;
game `/status` returns404. Only the status container has that private TCP listener. Its dedicated namespace filter blocks new outbound connections; it has no game socket or account-database mount.

The tile reports backend online/offline, active characters out of4, signed-in
account count and active usernames. Its private JSON includes player/game display
names and waiting usernames. Class, level and current town/dungeon location are
currently null: the production protocol does not expose structured character/UI
metadata. Do not infer these from pixels or invent values.

## Rollback

`sh pfme.sh stop` stops only this project's connector, adapter, game and status.
Delete only the reviewed game CNAME if public DNS rollback is approved. Keep
account/secret volumes. To undo Homepage, first check that no later edits occurred,
then restore only its `services.yaml.before-pathfinder` backup; preserve other
configuration. Do not remove/restart the eight existing services, alter Tailscale,
reset Cloudflare/DNS or delete the old inactive tunnel automatically.

## Verification and limits

The real isolated test creates10 generated accounts, authenticates independently,
plays four original characters through the adapter/native protocol, rejects six
additional joins, verifies separate-room frames/audio, ownership/refresh reconnect,
safe Leave then waiting admission, individual disable and private counts. These
are automated application tests, not human browser/public-edge playtests.

Public HTTPS/WSS and actual human friend play require the separately approved
activation. Homepage rendering requires its approved minimal config addition.
Do not describe the system as production-secure based on tests. Host/Docker and
Cloudflare administration remain trusted. Back up account volumes privately;
adventure state is still process-memory-only. No ESP32/shared firmware sources,
saves or ESP-NOW configuration are changed by this account wrapper.

The live Unix WebSocket route requires QUIC: HTTP2 produced cloudflared's
'http: no Host in request URL' during the owner test. The connector Dockerfile
reuses the pinned binary and CA bundle, with directories 0755 and the public
CA file 0444. Record the resulting image ID in the private .env before starting.
Finite --run-for-ms durations still have a 24-hour maximum. Explicit zero runs until an operator stops it. Protected modes require this flag explicitly, so omitting it fails closed. Account logins have separate four-hour defaults and optional seven-day Remember me sessions.


## Invite friends to choose their own credentials

Run these only in your own NUC SSH terminal:

```sh
sh /home/jimbo/pathfinder-friends/pfme.sh invite create "Alex"
sh /home/jimbo/pathfinder-friends/pfme.sh invite list
sh /home/jimbo/pathfinder-friends/pfme.sh invite revoke INVITATION-ID
```

The label is for your records, not a verified identity. After admission sync
succeeds, share the generated link privately with that friend. Anyone holding an
unused link can claim it, so do not paste links into logs, Git or public channels.
Links expire after 48 hours, work once, and cannot be recovered from the database;
only their SHA256 digests are stored. Revoke an unused invitation by its ID.
If lost/expired, create a replacement. The 32-account ceiling includes reserved
and revoked invitation accounts; the host can review these if this limit is met.

The browser clears the token fragment from the address bar before submitting it
with a CSRF-protected form. URL fragments are not sent to the tunnel or ordinary
HTTP request logs. Registration validates passwords and assigns no administrative
permissions. The friend chooses a lowercase username and any password of at
least 2 characters, then signs in normally (Remember me remains optional).

The host creates an unusable-password account with a random internal username and
stable principal first, and synchronizes its admission through the existing
root-only helper. Registration only renames and sets a password for that reserved
account. The public adapter cannot write the native admission policy. A failed
sync leaves the link unusable until the host finishes synchronization.

Account creation does not occupy a character slot. Four characters may play;
additional authorized accounts see Party Full. Disconnect retains the character;
Leave character releases its slot. Revoking a used invitation does not silently
disable an existing player: use `friend disable USERNAME` instead. Password reset
and disable retain the stable ownership mapping. No emails or invitations are
sent automatically. Real multi-friend playtesting is still required.


## Keep the game running; stop it yourself

The game runs without a lifetime timer. Authentication, revocation and connection
leases still expire as before; staying online does not make logins permanent.
Use `sh /home/jimbo/pathfinder-friends/pfme.sh stop` to take Pathfinder down,
`start` to bring it back, or `restart` to begin a fresh adventure. These commands
only affect Pathfinder. Account/invitation volumes persist; adventure state is
still in memory and is lost when the game process stops. Docker restart policy
remains `no`: this change does not add a boot service or automatic crash recovery.
