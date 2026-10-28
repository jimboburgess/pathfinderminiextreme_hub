# Private browser access

Stage 1 now implements Linux-only protected Serve admission in the existing C++
server. No Serve/Funnel or permanent game service is active. Development TCP
mode still rejects forwarded identities and retains its existing authentication.
The [Stage 1 setup and approval document](PRIVATE_TAILSCALE_STAGE1.md) is the
current source of truth; [the original provenance investigation](SERVE_IDENTITY_BOUNDARY.md)
records why TCP headers alone were rejected.

The owner supplied the complete tailnet policy, reported that the complete
owner-only candidate validated without errors, and reported clicking Save.
The assistant did not apply it. Subsequent SSH through the NUC's Tailscale IP
succeeded. Friends remain excluded. The previous unrestricted policy is backed
up privately outside Git; it must not be automatically restored as rollback.

The game endpoint, after explicit approval, will be one private HTTPS/WSS port
forwarded by the existing root Tailscale daemon to a protected Unix socket.
The game runs without privilege, host networking, Docker socket, home secrets
or outbound network, with bounded resources. Only root may forward the exact
allowlisted login; identities own private server-side character capabilities.
Root/host compromise is outside this identity guarantee. No second gameplay
engine, Discord SDK, public gateway or extra permanent proxy is introduced.

Real protected-transport verification uses a temporary root proxy fixture; it
is not proof of actual Serve TLS/WSS operation. That controlled owner-device
check remains the next approval-gated action, before any friends are invited.
