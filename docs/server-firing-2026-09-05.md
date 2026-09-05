# Server-enforced firing rules — 2026-09-05

First roadmap implementation milestone. The original server fired 20 queued Uzi rounds within 450 ms from a single input packet, despite the selected semi-mode interval of 120 ms. That behavior was reproduced on an isolated local server before editing.

## Result and policy

- `ServerFire` owns per-client cooldown, mode, request queue and actual spawned-shot count. Weapon definitions remain the source of firing intervals. One server tick can attempt at most one queued shot; unused cooldown time cannot be accumulated into a volley.
- Up to four pending requests are retained for 350 ms after receipt. Oversized counter jumps or batches that would overflow the queue are consumed and rejected, rather than becoming an unbounded backlog. Expired requests are discarded.
- Input and shot counters use wrap-safe half-range serial comparisons. Duplicate/reordered requests do not duplicate shots.
- Queued aim and rendered-snapshot timestamps are captured when a request arrives, so later movement packets do not overwrite them. Spread still uses the movement input simulated by the server and authoritative player state.
- A server-issued firing epoch changes on weapon/mode, reload start/end, death/respawn and slot reuse. Requests from old epochs are rejected. Client shot counters restart only when a newer authoritative snapshot changes epoch, so old counts cannot be relabeled as new shots.
- Mode/weapon changes preserve the existing cooldown. The client waits for the server's selected weapon/mode before requesting fire; stale bursts are cancelled across epoch changes. A gun swap does not refill magazines.
- Non-finite aim, out-of-range pitch/lean, invalid weapon/mode IDs and non-semi Glock modes are rejected. Finite yaw is normalized. These checks are not complete network hardening or anti-cheat.
- Spawn failure (empty magazine, reload, death, full bullet pool) does not increment the replicated shot counter. Rejected pool requests are not retried later.

## Compatibility and code organization

Protocol version rises from 3 to 4. Input adds epoch and mode; state adds the recipient's epoch and selected mode. Existing v3 peers must be restarted on matching builds and are rejected at handshake. World generation/revision is unchanged.

Wire sizes were compiled and checked: input 31 bytes; player state 35 bytes; empty snapshot 586 bytes; maximum snapshot 1468 bytes; maximum impact packet 418 bytes. The snapshot cap is now 63 bullet records, with a compile-time 1472-byte standard-IPv4 payload ceiling. The simulation pool remains 256 bullets. Smaller-MTU paths and tunnels still need an explicit transport policy.

Firing and rewind responsibilities moved into `server_fire.h/.cpp` and `server_rewind.h/.cpp`. The server entry file is below 300 lines; new modules have no SDL/GL dependencies or per-tick heap allocations. A double-precision simulation clock supplies cooldown timing. Rewind interpolation and its existing 1.4-second bound are covered by regression checks.

## Verification

- Pre-change real UDP reproduction: 20 rounds fired within 450 ms from a single backlog packet.
- `server_fire` tests: all supported weapon/mode intervals, sustained request spam, bounded/expired queues, oversized jumps, counter wrap, swaps/mode toggles, death/respawn/slot reuse, reload/auto-reload, ammunition conservation, full bullet pool, captured aim/rewind, input decoding/validation and extracted rewind interpolation.
- `client_fire` tests: fresh epoch reset, stable counts through ordinary snapshots, and rejection of stale/truncated state effects on the firing counter.
- `server_fire_udp` test: launches a separate server on a temporary localhost port; proves oversized backlog rejection, small-batch recovery, duplicates/reordering, swap/reload epochs, ammo conservation, invalid Glock mode and v3 handshake rejection.
- Both client/server targets built in default and Release configurations. The default headless tests and UDP regression passed; all three CTest suites also passed in Release (3.96 seconds total).
- The graphical v4 client joined a matching temporary Paldiski server with 100 ms one-way lag, ±20 ms jitter and 5% simulated loss, received world state, captured a frame and disconnected. This was a connection smoke test, not a sustained combat/load test. Both test processes were closed. The existing graphics texture-unit warning remains outside this change.

## Limits and next work

Firing intervals are quantized upward to the 60 Hz server tick: current 120 ms semi fire becomes about 133 ms, 70 ms burst becomes about 83 ms, and 100 ms auto remains about 100 ms. This deliberately never fires faster than the configured interval. Sub-tick scheduling is not introduced here.

The cumulative shot counter still cannot reconstruct distinct aim/timestamps for lost individual shots or prove physical trigger edges. Recovered requests use the receipt packet's aim, and severe packet loss can drop requests rather than fire them late. Full timestamped shot events are a separate roadmap item. The server's general receive-loop budget, full client packet validation, authoritative input replay, and 16-client load tests remain open.

Next visible gameplay improvement: tree-trunk bullet collision. Keep the remaining online validation/input-replay work on the foundation track.
