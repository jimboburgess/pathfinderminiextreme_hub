#!/bin/sh
# REVIEW ONLY. Exact values must be frozen in the approved run manifest.
echo 'Review only: no container or DNS change is performed.' >&2
exit 2

# Required separately recorded values:
# CF_RUN=<approved-run-label>
# CF_CONNECTOR_ID=<full-container-id>
# CF_CONNECTOR_IMAGE=sha256:<approved-connector-digest>
# CF_GATEWAY_ID=<full-container-id>
# CF_GATEWAY_IMAGE=sha256:<approved-gateway-digest>
# CF_GAME_ID=<full-container-id>
# CF_GAME_IMAGE=sha256:<approved-game-digest>
# Confirm each ID's label pfme.cloudflare.run equals CF_RUN, image digest equals
# the recorded digest, numeric User equals its assigned role and mounts match
# exactly the approved paths. Stop for review on drift; never infer by prefix.
# Example for the connector (repeat verification for gateway/game):
# test "$(docker inspect --format '{{index .Config.Labels "pfme.cloudflare.run"}}' "$CF_CONNECTOR_ID")" = "$CF_RUN"
# test "$(docker inspect --format '{{.Image}}' "$CF_CONNECTOR_ID")" = "$CF_CONNECTOR_IMAGE"
# docker inspect --format '{{.Config.User}} {{json .Mounts}}' "$CF_CONNECTOR_ID"
# docker stop --timeout 5 "$CF_CONNECTOR_ID"
# docker rm "$CF_CONNECTOR_ID"
# Repeat verified stop/removal for CF_GATEWAY_ID then CF_GAME_ID, in that order.
# Check tunnel replicas; revoke ONLY this tunnel credential when approved.
# Remove ONLY the new game DNS route and Access app when that cleanup is approved.
# Never reset a whole DNS zone/nameservers, stop Tailscale, delete its admission,
# prune Docker, remove unrelated containers, change firewall/router/SSH settings
# or delete shared secrets/accounts as an automatic rollback.
# Finally verify all eight original containers, LAN/Tailscale owner SSH and no
# game TCP listener. Keep inactive images/accounts/policies/evidence for review.
