# Network

Connectivity state, secure transports, time synchronization, provisioning, and
provider-neutral network policies. Credentials are never stored in source.

## Implemented prototype

`WifiSupervisor` is a platform-neutral state machine. It requests connection by
opaque configured-profile key/revision, emits disconnect actions when the
profile or enabled state requires one, and retries transient timeout/link
failures with profile-distributed jitter and bounded exponential backoff. It
stops after eight failed attempts or an authentication rejection until the
profile reference changes. Adapter observations must echo the active profile
reference and attempt generation; callbacks from replaced attempts and
out-of-order terminal states are ignored.

It does not contain SSIDs, passwords, a credential store, an ESP32/Arduino Wi-Fi
adapter, TLS transport, captive-portal handling, or proof of connectivity on the
FlightWall Mini. Those remain separate, gated implementation work.
