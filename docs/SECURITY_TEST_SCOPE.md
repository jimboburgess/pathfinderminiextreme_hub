# Pathfinder staging security test scope — not an external test authorization

Owner must fill exact endpoints, accounts, time window and contact before any
invited test. No external endpoint is currently approved. The automated harness
targets only disposable loopback processes it starts itself, never live NUC
services. Fighter/Wizard/Rogue are synthetic OAuth identities, not real Discord
accounts or proof of shared playable multiplayer.

Allowed offline cases: missing/invalid/expired/revoked auth; spoofed player ID;
wrong Origin/Host; stale/replayed ticket or command; unknown admin/state setters;
duplicate keys, nested JSON, binary and 4097-byte frames; ownership denial;
reconnect; logout; bounded rate exhaustion. Fixed gateway log categories must not
contain input strings, names, codes, identities, tokens or client secrets.

Automatic harness ceiling: one backend with 40-second maximum lifetime, one
gateway on random loopback port, eight WS maximum, <=21 small ping attempts in a
controlled one-second burst, <=4097-byte malformed fixture. Stop on unexpected
non-loopback listening, missing process cleanup, exceptions, unrelated-service
impact or OS pressure. Test runner has a 45-second case deadline. Do not expand
these limits or target an existing server to make a test pass.

Future invited scope MUST list:

* Exact HTTPS and WSS Pathfinder routes, application ID, allowed test-account IDs.
* Authorized dates/times, maximum clients/request rates, packet/body limits and
  synthetic data policy; tester and owner emergency contact.
* Explicit exclusions: NUC host, LAN, SSH, Docker API/socket, Homepage, Navidrome,
  Uptime Kuma, music bot, UniFi, metadata, other users/services/provider accounts.
* No scanning, credential stuffing, phishing, persistence, destructive payloads,
  privilege escalation against host, data exfiltration or aggressive DoS. Discovery
  of a host escape stops testing and triggers owner notification, not exploration.
* Stop on error spikes, any unrelated service degradation, sustained CPU/RAM limit,
  suspected escape, unknown data or owner request. Disable connector first.
* Report minimal reproducer and fixed event categories; redact tokens and private
  data. Rotate/revoke test credentials after assessment. Do not publish findings
  containing personal infrastructure details without owner approval.

Passing this harness is not a penetration-test certification. Real OAuth/proxy,
container enforcement, native sanitizer/fuzzer and three-player gameplay evidence
remain required. No aggressive tests were run against the NUC or home network.
