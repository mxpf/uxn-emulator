# Constellation v0

Constellation v0 is the smallest test of a larger idea: an application can
grow by composing unchanged Uxn machines instead of enlarging Uxn itself.

The prototype runs exactly two Uxn instances. They do not share memory. Each
instance sees one optional device in the otherwise unused `0xd0` device slot.
The existing Uxn processor and the normal `uxncli` and `uxnemu` runners are
unchanged.

This is a Port-only host, not a Varvara implementation. Ordinary Varvara
applications run in the existing runners. The experimental port number is
local to this prototype; it is not a standardized extension.

## Port contract

The supervisor owns two independent FIFO queues: A to B and B to A. Each queue
holds four messages, and each message contains from zero to 255 bytes. The peer
is implicit because v0 has exactly two endpoints.

The Port device occupies `0xd0` through `0xdf`:

| Port | Meaning |
| --- | --- |
| `d0-d1` | Receive vector |
| `d2-d3` | Address where the next received payload is copied |
| `d4` | Length of the delivered payload |
| `d5` | One while the receive vector is running |
| `d6-d7` | Address of the payload to send |
| `d8` | Length of the payload to send |
| `d9` | Write a nonzero byte to send; reads back one on success or zero when full |
| `da-db` | Queue-space notification vector |

Short registers are big-endian. Unused device bytes are passive storage.
Payload copies wrap within the 64 KB address space at `ffff`. The receiver
must reserve space for 255 bytes at its receive address. Zero-length messages
still occupy a queue slot and invoke the receive vector. No extra memory banks
are exposed; ROMs larger than 65,280 bytes are rejected.

Sending copies bytes out of the sender's memory and into its outgoing queue.
Delivery copies the oldest message into the receiver's memory and invokes the
receiver's Port vector. At no point can one Uxn instance address another
instance's RAM.

## Scheduling and failure

After a fixed boot phase, turns alternate between B and A. A turn delivers at
most one incoming message and runs its receive vector until `BRK`. The first
runtime turn belongs to B because A's boot code sends the initial `PING`.

Every evaluation has a fixed instruction ceiling. Reaching it is a fault. A
send to a full queue fails immediately and leaves zero in `d9`; the ROM may
retry on a later turn.

Boot runs B then A at address `0100`, once each. Runtime turns start with B.
A failed send arms a queue-space notification. When space becomes available,
the sender's next turn invokes `da-db` once, clearing the notification first.
This takes priority over receiving and consumes the turn. The ROM retains its
pending payload and retries from that handler. Another failure arms another
notification. Empty turns execute no ROM code.

A required vector of zero or an instruction-limit failure is terminal for the
pair. A zero instruction ceiling is rejected. After a fault, further boot and
step calls fail without executing instructions. A second boot or a load after
boot is rejected. Each endpoint accepts one successful ROM load per
initialization; a second load is rejected without changing memory. Call
`constellation_init` to start a fresh pair. No rollback is performed.

Fault events include a reason: invalid instruction ceiling, instruction limit,
missing receive vector, or missing queue-space vector. The pair retains its
first fault reason. Trace exhaustion is also available as a fault reason on
the pair even when there is no room to append another event. The runner prints
these reasons.

## Trace and replay

The supervisor records boots, turns, sends, deliveries, full-queue failures,
idle turns, and instruction-ceiling faults. The trace observes a normal run;
it does not control one.

There are no external inputs in v0. Repeating a run with the same ROMs, initial
RAM, and scheduler configuration must produce the same trace and final bank-zero
RAM. The test suite performs that comparison.

Replay tests also compare stacks, device bytes, instruction counts, queues,
pending notifications, fault state, and the next scheduled endpoint.
They repeat queue recovery and each fault path, including trace exhaustion
inside a running receive handler as well as during idle scheduling.
The trace holds 128 events. Exhaustion sets an explicit terminal error and
marks the record incomplete. If it occurs inside an evaluation, subsequent
sends are suppressed; that bounded evaluation returns at BRK or its instruction
ceiling, and no further vector runs. This is not a resumable snapshot format.

## Try it

~~~sh
make constellation
make test
~~~

The demo assembles `examples/constellation-ping.tal` and
`examples/constellation-pong.tal` using Drifblim. The first build downloads
Drifblim if it is not already available. A sends
`PING`, B receives it and sends `PONG`, and A records receipt. The executable
prints trace events as exact hexadecimal payload bytes and reports whether
the pair reached quiescence without faults. Run another pair with
`bin/constellation-v0 A.rom B.rom`. Quiescence alone is not an application
correctness assertion; the tests check the demo's receive buffers.

This version deliberately has no files, network, windows, audio, package
format, general routing table, or permissions system.

## Queue recovery through real ROMs

Run `make constellation-recovery` to assemble and run
`examples/constellation-burst.tal` and `examples/constellation-collect.tal`.
The sender attempts bytes 01 through 05 during boot. Its four-slot queue
accepts the first four and rejects the fifth. The sender reads the failure
status and yields at BRK, retaining the fifth byte.

The collector receives one message without replying. On the sender's next
turn, the queue-space vector retries the pending byte. The collector records
all five bytes in order at 0400; its zero-page counter records five deliveries.
The sender records one failed send, one wake-up, and completion.

The test loads the assembled ROMs into fresh machines and advances only
through boot and scheduler calls. It verifies the entire expected 26-event
trace, exact payload order, ROM-maintained counters, empty queues, and no
faults. A second fresh run must reproduce the trace and final machine state.
The same first-build Drifblim download requirement applies as to the greeting
demo; subsequent builds can run offline.
