# Constellation: routed-host experiment

Keep the machines small and the ways they cooperate flexible. This experiment
adds explicit routing to a separate Port-only host, leaving Uxn, Constellation
v0, Tiny Neighbors, and the browser game unchanged. It is not yet a replacement
for v0 or a released console specification.

## Cartridge declaration

A cartridge supplies an ordered list of ROMs and a list of directed routes.
Each route is `(source node, local selector, destination node)`. Node IDs are
positions in that cartridge's list, not roles understood by the host. Each
source's selector must be unique. Different sources may reuse the same selector;
multiple selectors can lead to the same destination. Self-routes are allowed.

The proof cartridge is declared as C tables in `tests/test_routes.c`:

| Node | ROM |
| --- | --- |
| 0 | `routes-burst.rom` |
| 1 | `routes-relay.rom` |
| 2 | `routes-collect.rom` |

| Source | Local selector | Destination |
| --- | --- | --- |
| 0 | 01 | 1 |
| 0 | 02 | 2 |
| 1 | 07 | 2 |

The sender uses local route 01 without knowing which node implements it. A
second test reorders the other two nodes and rewires the table, using exactly
the same ROM files. Human-readable labels are not routing authority.

This is an in-memory declaration, **not an on-disk bundle or manifest parser**.
The test harness loads declared files; guests have no filesystem access. A file
format can follow once the routing semantics have earned their place. There
is no required presentation node, game tick rate, scene model, or ROM-per-entity
rule. One node with no routes also works.

## Storage and limits

The caller supplies storage for the exact declared node and route counts.
The host copies the route definitions during initialization and exposes no
runtime topology-editing API. Caller-owned host/node/link/config buffers must
not overlap; initialized state must remain alive and must not be moved or
modified behind the host's back. Reinitialization discards the whole session.

Every directed route has its own four-slot FIFO, carrying 0–255 payload bytes
per message. A full route does not consume another route's capacity. Copies
wrap inside bank-zero RAM; subsequent sender writes cannot alter queued data.
Zero-byte messages consume a slot and invoke the receive vector.

This byte-addressed experiment accepts 1–255 nodes and 0–255 routes, with node
IDs and local selectors 0–254 (`ff` means none). These are defensive encoding
bounds, not a chosen console size or a demonstrated throughput capability.
Testing here uses one or three nodes. Each current Uxn struct still reserves
1 MiB of RAM internally, while the guest addresses only 64 KB. No shared memory
or extra memory banks are exposed. ROMs over 65,280 bytes are rejected.

## Port contract

As in v0, `d0-d1` is the receive vector; `d2-d3` the receive buffer;
`d4` the delivered length; `d5` one during a receive callback;
`d6-d7` the send buffer; `d8` the send length; `d9` the send strobe/status;
and `da-db` the writable callback. Shorts are big-endian.

The routed host adds:

| Register | Meaning |
| --- | --- |
| `dc` | Sender-local route selector for the next send |
| `dd` | Read-only actual sender node ID during a receive callback; `ff` otherwise |
| `de` | Read-only local route selector during a writable callback; `ff` otherwise |

`DEI` of `dd`/`de` reads supervisor-owned metadata, even if a ROM tries to
overwrite the device bytes. Payloads cannot forge that metadata. Node IDs are
cartridge-local identities, not portable role names or authorization claims.
Other unused bytes remain passive storage.

A nonzero write to `d9` copies to the route selected by `dc`. Readback is one
on success, zero on a full queue. An undeclared selector is a terminal protocol
fault, not a queue-full result. A zero strobe does nothing. No implicit peer,
broadcast, message format, reply routing, or global service registry is added.
Existing v0 ROMs are not silently switched to this ABI.

## Scheduling and backpressure

All declared ROMs must load once before boot. Boots run in node-list order,
at `0100`, before any delivery; messages sent during boot remain queued. Turns
then cycle through node IDs starting at zero, independent of wall-clock time.
Each evaluation has a mandatory nonzero instruction ceiling.

A turn does at most one of the following:

1. Invoke an eligible writable callback for one outgoing route.
2. Deliver one message from one incoming route and invoke its receive callback.
3. Record an idle turn without executing guest code.

Incoming routes are scanned cyclically in declaration order, starting just
after the last route serviced by that node. This keeps one continuously busy
incoming route from starving another. Messages remain FIFO within a route;
there is no global FIFO across routes.

A failed send arms one pending notification **on that route**. Once it has
space, a subsequent sender turn clears that flag and calls `da-db` with the
original selector in `de`. Changing `dc` or successfully using a different
route cannot redirect the notification. Repeated failures coalesce per route.
Writable notifications also rotate through routes. The ROM owns pending data
and must retry explicitly; there is no automatic resend or reserved slot.

Writable callbacks take priority over receives, matching v0's policy. This
does not guarantee application-level progress: a ROM that continually rearms
notifications can defer its own incoming work. The experiment claims fairness
among eligible incoming routes, not freedom from every starvation/deadlock
pattern. Hosts must bound total turns as well as individual evaluations.

## Faults, inspection, and replay

Invalid declarations are rejected before boot. Duplicate loads, missing loads
at boot, repeat boots, or loads after boot are rejected without running code.
Undeclared routes, missing required vectors, instruction ceilings, and trace
exhaustion are terminal for the entire host. No rollback is performed.

The bounded 128-event pending trace includes boots, turns, sends, full sends, deliveries,
route-specific wake-ups, idle turns, and fault reasons. Message events contain
the actual source, destination, source-local selector, and exact copied payload.
Bad-route fault events retain the attempted selector and use `ff` for the
unknown destination, even if the ROM subsequently changes its selector register.
The independent `trace_exhausted` flag marks any omitted event while preserving
the first fault reason. Filling the last available slot alone does not set it;
an attempted event beyond capacity does, including an omitted fault event.
If exhaustion occurs inside a callback, further sends are suppressed while that bounded evaluation returns;
no later callback runs. The first fault reason is retained.

### Consuming bounded batches

Every recorded event has a zero-based 64-bit `sequence`. Taking a batch does
not reset this sequence; reinitializing the session does. Sequence exhaustion
also raises `ROUTED_TRACE_FULL` and marks the record incomplete rather than
wrapping its numbering.

Between boot/step calls, `routed_take_trace(host, output, capacity, count)` copies
the entire pending batch to caller-owned storage, then zeros the internal
buffer and resets only its pending count. It does not change machines, queues,
scheduler position, the next sequence number, or fault state. Callers must use
valid disjoint output/count storage that does not overlap the host or its owned
state, and must not call this concurrently with an evaluation.

If output capacity is too small, the count pointer is null, or a nonempty batch
has no output buffer, the call fails without changing the host or outputs.
An empty batch succeeds with a count of zero. Taking a failed session's pending
events is allowed for inspection; it cannot clear faults or repair truncation.

The caller owns the copied batch and is responsible for comparing, writing, or
otherwise retaining it before reusing that storage. Sequence numbers reveal
gaps/reordering; they do not themselves preserve discarded event contents.
The caller must also preserve the session's fault and incomplete-record status
when exporting evidence. There is no automatic disk logger or callback sink.

Capacity is still a hard limit **within** a boot or turn. A single evaluation
can exhaust the buffer before the caller gets control back; consuming between
turns does not silently relax that bound. A caller that consumes too infrequently
still gets a terminal error, never overwritten events.

There are no external inputs in this proof. Replay means a fresh execution of
the same ROM bytes, declaration order, initial memory, and instruction ceiling.
The test compares traces, all allocated RAM, stacks, device bytes, instruction
counts, queues, pending notifications, round-robin cursors, and fault state after
boot and every turn. Host pointers are deliberately excluded. This is not a
serialized snapshot or a replay-file format for the routed host.

## Evidence and next boundary

Run `make constellation-routes`. Drifblim assembles three ordinary Uxn ROMs:

- The burst ROM fills local route 01 with bytes 01–04, retains rejected byte
  05, then successfully sends `aa` on local route 02.
- The relay forwards the five recovered bytes through local route 07.
- The collector records `aa 01 02 03 04 05` and each actual sender ID.

The exchange produces 51 events across 18 turns and 11 deliveries, with one
full-queue event and one wake-up on the correct route. The relay is a fixture
whose downstream queue does not fill, not a general lossless relay library.
Additional checks exercise rewiring unchanged ROMs, a single isolated node,
a self-route, two active incoming routes, multiple pending writable callbacks,
zero/255-byte messages, wraparound, immutable send copies, source/writable
metadata spoof attempts, configuration validation, lifecycle
rejection, and terminal faults. Fault-trace boundary regressions test a retained
bad selector, a fault using the last trace slot, an omitted fault after a full
trace, and trace exhaustion preceding another fault attempt. Each is repeated
in a fresh host with full-state comparison and checked for terminal behavior.

## Sustained cooperation

Run `make constellation-sustained` for a native, bounded-memory long-run test.
The three-node arrangement remains a burst sender, relay, and collector, with
an added route `(2, 09, 0)` for acknowledgements. New cycle versions of the burst
and collector ROMs reuse the original relay ROM. The sender waits for an
acknowledgement before starting each new batch and stops after 1,024 cycles.
These are test protocols, not roles or pacing rules in the host.

Observed on the 2026-09-09 local run:

| Evidence | Result |
| --- | ---: |
| Scheduler turns | 18,433 |
| Completed cycles | 1,024 |
| Recorded events | 50,181 |
| Full sends and successful wake-up retries | 1,024 each |
| Deliveries on routes 0/01, 0/02, 1/07, 2/09 | 5,120 / 1,024 / 5,120 / 1,024 |
| Peak occupancy on those routes | 4 / 1 / 2 / 1 |
| Largest observed delivery gaps, including startup | 6 / 18 / 6 / 19 turns |
| Largest consumed batches in the two runs | 9 / 24 events |

The first execution consumes after boot and every turn; a fresh replay consumes
after boot, every seven turns, and at completion. The test compares **every
event byte for byte** across those different batch boundaries using only a
bounded pending batch. Sequence continuity and a cumulative diagnostic hash
are checked too. The resulting hash was `b9b4190851941c28`; its encoding is
sequence (eight little-endian bytes), kind, reason, source, destination,
selector, length, and payload, folded with 64-bit FNV-1a. It is not a
cryptographic proof or a substitute for retaining a trace.

After every turn, the test compares bank-zero RAM, stacks, device bytes,
instruction counts, queue contents, pending wake-ups, scheduler cursors, fault
state, and next trace sequence. All allocated RAM is compared at boot and
completion. Trace buffers themselves may differ between consumption points;
their concatenated event streams must not. Every payload and route delivery
total is also checked against the fixture's expected protocol, and every route
must make progress within 24 turns throughout this workload. Both runs finish
quiescent with empty queues and no trace truncation.

The short and sustained suites also passed AddressSanitizer and
UndefinedBehaviorSanitizer on this local run. These are logical-turn results,
not a real-time latency, throughput, or hours-long soak benchmark.

This earns a sustained **controlled workload**, not a production console or a
general progress guarantee. The acknowledgement protocol keeps the relay's
downstream queue below capacity; only the sender's route 01 repeatedly fills.
Competing self-rearming writable callbacks, arbitrary cyclic backpressure,
external input replay, browser parity for this host, packaging, and larger
workloads still need targeted tests. Game-specific roles remain outside it.
