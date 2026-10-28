#!/bin/sh
# REVIEW ONLY: intentionally refuses execution. Activation needs explicit approval.
# Replace STAGE, IMAGE and ORIGIN with the private reviewed artifact values.
# Run the reviewed commands individually in the owner's terminal, not by deleting
# this guard and blindly executing the file. Never run a policy reset or Funnel.
echo 'Review only; activation is not authorized by preparing this file.' >&2
exit 2

# STAGE=/home/jimbo/<reviewed-private-preparation-directory>
# IMAGE=sha256:<reviewed-local-image-id>
# ORIGIN=https://<approved-node>.<actual-tailnet>.ts.net

# Owner authorizes the real admission file; this is separate from tailnet policy.
# sudo install -d -o root -g jimbo -m 0750 "$STAGE/policy"
# sudo install -o root -g jimbo -m 0640 "$STAGE/owner-admission.pending.json" "$STAGE/policy/admission.json"

# No ports, network, home, SSH keys or Docker socket are exposed to the game.
# The socket directory already exists, UID 1000 and mode 0700.
# docker run --detach --name pfme-private-stage1 --restart=no \
#   --label pfme.private-stage1=stage1-review --cidfile "$STAGE/runtime.cid" \
#   --network=none --read-only --cap-drop=ALL --security-opt=no-new-privileges \
#   --pids-limit=32 --memory=256m --cpus=1 --ulimit=nofile=128:128 \
#   --mount "type=bind,src=$STAGE/socket,dst=/socket" \
#   --mount "type=bind,src=$STAGE/policy,dst=/policy,readonly" \
#   "$IMAGE" --runtime party --serve-socket /socket/backend.sock \
#   --serve-origin "$ORIGIN" --serve-policy /policy/admission.json \
#   --run-for-ms 3600000
# docker logs pfme-private-stage1

# ONLY AFTER a separate explicit approval of this exact private endpoint:
# sudo tailscale serve --https=443 "unix:$STAGE/socket/backend.sock"
# Browser: $ORIGIN/play     WebSocket: wss://<same-host>/
# No --bg, --tcp, --http, --tun, Funnel, router ports or unrelated mappings.

# Scoped rollback: first close the one ingress, then the one owned runtime.
# sudo tailscale serve --https=443 off
# RUNTIME_ID=$(cat "$STAGE/runtime.cid")
# test "$(docker inspect --format '{{index .Config.Labels "pfme.private-stage1"}}' "$RUNTIME_ID")" = stage1-review
# docker stop --time 5 "$RUNTIME_ID"
# docker rm "$RUNTIME_ID"
# tailscale serve status --json
# tailscale funnel status
# Keep the owner's narrow policy. Never restore the old allow-all rule as an
# automatic rollback, and never stop tailscaled or other NUC containers.

# Administrator revocation: remove the login, increment revision, validate JSON,
# install as root:root-or-game-group mode 0640 into policy/admission.next, then
# atomically rename within policy/ to admission.json. Any policy revision closes
# all old leases within about one second; remaining authorized users reconnect.
# Do not reuse a lower revision or silently remap a login to another principal.
