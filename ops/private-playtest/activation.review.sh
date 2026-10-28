#!/bin/sh
# REVIEW ONLY. Provisioning is separate; live Serve always needs explicit approval.
echo 'Review only; no activation is authorized by preparing this file.' >&2
exit 2

# RUN=/var/lib/pathfinder-stage1
# IMAGE=sha256:<newly-reviewed-dedicated-image-id>
# ORIGIN=https://<approved-node>.<actual-tailnet>.ts.net
# SERVICE_UID=$(id -u pfme-stage1)
# SERVICE_GID=$(id -g pfme-stage1)
# Require locked account, sole primary group, current patched daemon, empty Serve/
# Funnel configuration and unused TCP443. Root-managed paths must already exist.
# Socket: service:service0700; policy directory root:service0750, file0640.
# sudo docker run --detach --name pfme-private-stage1 --restart=no \
#   --user "$SERVICE_UID:$SERVICE_GID" \
#   --label pfme.private-stage1=stage1-review --cidfile /run/pathfinder-stage1/runtime.cid \
#   --network=none --read-only --cap-drop=ALL --security-opt=no-new-privileges \
#   --pids-limit=32 --memory=256m --cpus=1 --ulimit=nofile=128:128 \
#   --mount "type=bind,src=$RUN/socket,dst=/socket" \
#   --mount "type=bind,src=$RUN/policy,dst=/policy,readonly" \
#   "$IMAGE" --runtime party --serve-socket /socket/backend.sock \
#   --serve-origin "$ORIGIN" --serve-policy /policy/admission.json --run-for-ms 3600000
# Verify actual UID/GID, mounts, limits and socket0600 before approved ingress.
# ONLY AFTER separate live-access approval, foreground, no --bg or Funnel:
# sudo tailscale serve --https=443 --set-path=/ "unix:$RUN/socket/backend.sock"
# Browser: $ORIGIN/play. WebSocket uses the same external hostname over WSS.
# Backend Host localhost is checked only behind the trusted root Unix hop.

# Rollback: Ctrl+C in the exact foreground Serve session first.
# Fallback ONLY after verifying that node:443 root handler matches this exact
# Unix backend, and no unrelated configuration has replaced it:
# sudo tailscale serve --https=443 --set-path=/ off
# RUNTIME_ID=$(sudo cat /run/pathfinder-stage1/runtime.cid)
# test "$(docker inspect --format '{{index .Config.Labels "pfme.private-stage1"}}' "$RUNTIME_ID")" = stage1-review
# test "$(docker inspect --format '{{.Image}}' "$RUNTIME_ID")" = "$IMAGE"
# test "$(docker inspect --format '{{.Config.User}}' "$RUNTIME_ID")" = "$SERVICE_UID:$SERVICE_GID"
# Inspect exactly the two expected mounts, then:
# docker stop --time 5 "$RUNTIME_ID"
# docker rm "$RUNTIME_ID"
# tailscale serve status --json
# tailscale funnel status
# Preserve account/policy/inactive artifacts for review; no recursive cleanup,
# Serve reset, tailscaled stop, policy restore, Docker prune or unrelated changes.
# Revocation: root atomically replaces admission JSON with a higher revision;
# only removed/remapped identities lose leases; unchanged players keep their
# original expiry. Invalid/rollback policy still fails closed. Never reuse a
# lower revision or silently assign another login an existing principal.
