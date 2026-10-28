#!/bin/sh
# REVIEW ONLY; missing account/domain/AUD/digests/egress validation block activation.
echo 'Not deployable until the named owner-only Cloudflare package is approved.' >&2
exit 2

# OWNER-ONLY SEQUENCE, each prerequisite separately reviewed:
# 1. Existing domain: review all current DNS/MX/SPF/DKIM/DMARC first; preserve them.
#    Protect proposed game hostname with an Access self-hosted application BEFORE
#    connecting a public route. Exact owner email allow rule; no Everyone/Bypass.
#    Record exact team issuer, application AUD and verified owner sub from trusted
#    Access administration. Configure short app sessions, desired IdP/MFA and WSS.
# 2. After install/account provisioning approval, pin reviewed cloudflared binary/
#    image digest, dedicated gateway/connector UIDs and two narrow IPC groups.
#    New /var/lib/pfme-cloudflare paths ONLY; never alter Tailscale's paths/policy.
#    Game back dir 999:backend-group0710, socket0660. Gateway front dir
#    gateway-uid:front-group0710, socket0660. Root policies/keys/config0640 RO.
# 3. Run refresh_keys.py with the trusted fixed issuer, review pending public JWKs,
#    atomically root-install keys. Refresh before one-hour cache deadline. No
#    credentials/keys in Git, no token-bearing URLs/arguments/logs.
# 4. Compute identity_login(issuer,verified-owner-sub) offline and root-map it to
#    the existing stable owner principal in a separate Cloudflare admission file.
# 5. Start bounded game, network=none, --user999:backend-group, read-only,
#    capdropALL/NNP,256MiB/1CPU/PIDs32/nofile128; only backRW and policyRO mounts:
#    pfme_server --runtime party --cloudflare-socket /back/backend.sock \
#      --cloudflare-origin "$ORIGIN" --cloudflare-policy /policy/admission.json \
#      --cloudflare-peer-uid "$GATEWAY_UID" --cloudflare-peer-gid "$BACKEND_GID" \
#      --run-for-ms 900000
# 6. Start bounded gateway, network=none, --user GATEWAY_UID:BACKEND_GID,
#    --group-add FRONT_GID, same restrictions/limits; frontRW, backRW, keysRO,
#    configRO mounts only. No game-policy, token, Docker socket or home mount:
#    gateway.py --config /config/gateway.json --run-for-seconds 900
# 7. Start connector behind connector_gate with a FRESH root-controlled release
#    nonce file absent, no root/capabilities/host networking/published ports;
#    mounts ONLY front socket dir, its config/credential fileRO and gateRO.
#    It must not execute cloudflared until the owned private network namespace has
#    a reviewed default-drop filter. Validate target container label/image/PID,
#    namespace differs from host, nft syntax/rules and allowed public IP sets.
#    nsenter --net=/proc/$CONNECTOR_PID/ns/net -- nft --check -f "$FILTER"
#    nsenter --net=/proc/$CONNECTOR_PID/ns/net -- nft -f "$FILTER"
#    Verify denied host/LAN/other-service destinations inside THAT namespace.
#    Only then root-create exact64-byte nonce release file. Do not use a stale GO.
#    EXEC after gate: cloudflared tunnel --config /config/tunnel.yml run "$TUNNEL"
# 8. Only after exact public-exposure approval, create the one reviewed DNS route.
#    Test outside Tailscale: owner login, valid HTTPS/WSS, original graphics/audio,
#    refresh/reconnect, signed-token denial, expiration/logout and deny all others.
#    Monitor bounded CPU/memory/PIDs; stop on failure, never weaken checks.
# 9. End the test: stop/remove ONLY the recorded labeled connector first, then
#    gateway and game after checking image/user/mounts. Remove ONLY the exact new
#    DNS route/Access application if that cleanup was approved; do not restore
#    whole zones, stop Tailscale, prune Docker or edit unrelated services.
#    Revoke tunnel credentials after confirmed connector shutdown when approved.
#    Verify eight original containers and both owner SSH paths. Keep test evidence.
