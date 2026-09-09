# Tiny Neighbors: measured baseline

Measured 2026-09-09 against the game/runtime at commit `51fb614`, with the
test-only probes introduced alongside this report. No ROM, opcode, device,
scheduler, gameplay, or website changes were needed.

## Decision

Keep Tiny Neighbors as the two-ROM regression cartridge. It has done its job as
a small cooperation, inspection, and deterministic replay example. More garden
content is not necessary to establish that baseline.

It is **not** a capacity test or evidence that a larger narrative console is
ready. Normal play never fills even half a queue. The view receives the entire
world snapshot; the game does not test deliberately separate knowledge,
three-node routing, or deductions made from partial information.

The next useful platform experiment is explicit routing among more than two
ROMs, with a fixed declared topology, bounded queues, and deterministic replay.
It should impose no application roles. A cartridge decides what its machines
do and how they connect; the host supplies general communication mechanisms.
The [routed-host experiment](CONSTELLATION-ROUTES.md) now tests that next step
separately, without changing this garden or its two-ROM host.

A locked-room mystery remains one possible application, not the platform's
roadmap or a required presentation/scene/deduction layout. Games should reveal
useful capabilities, not become structures hard-coded into the emulator.
Larger node counts, dynamic topology, and asset services still need their own
evidence; this baseline does not establish their necessity or performance.

## Coverage added

Each scenario resets both ROMs. All 200 snapshot bytes and cumulative message /
canonical pixel fingerprints are compared at boot and after every action in
native C versus Node WebAssembly and real Chromium WebAssembly.

| Scenario | Actions |
| --- | ---: |
| Wandering through four clock wraps | 1,024 |
| Proximity without greeting | 521 |
| Perimeter collision and distant greeting | 5 |
| Water collision | 11 |
| Creature collision and repeated greeting | 13 |
| Seed 42, movement/wait only | 1,024 |
| Seed 2026, movement/wait only | 1,024 |
| Seed 73, mixed actions | 1,024 |
| Friendship followed by seed 42 mixed actions | 1,034 |
| **Total** | **5,680** |

Including nine boots gives **5,689 new checkpoints**, in addition to the
original 1,011-checkpoint suite: **6,700 per host comparison**. Hand-specified
behavioral landmarks supplement parity; agreement between hosts alone is not
an independent game-rules oracle. Native checks also require matching view/world
snapshots, unchanged terrain, empty stacks, and drained queues.

Both Wasm hosts reject -1, 6, 255, 256, and 2147483647 after valid play. Each fault
must preserve the last snapshot and digest, block subsequent valid input,
provide a diagnostic, and reset exactly to boot. The original suite separately
checks action 256 at the end of its run.

Existing suites also passed: 200 processor/device checks, 1,046 Constellation
checks, 24,388 garden assertions including two matching 1,000-tick native runs,
SDL dummy rendering, and 1,438 Constellation recovery checks.

The saved-replay regression now records the ten-action greeting through the SDL
command-line interface, replays the file, and compares the final state/trace
summary and screenshot byte for byte. A syntactically valid corrupted final
fingerprint must fail with a divergence diagnostic at tick 10.
The separate recovery ROMs fill four queue slots and retry the fifth message; the garden
itself does not exercise that pressure. Chromium additionally passed keyboard
greeting, displayed canvas pixels, buttons, play, blur/pause, reset, narrow-screen
overflow, and absence of page errors. Narrow-screen testing is not a physical
phone or Safari test.

## Deterministic work per tick

Ranges below cover the 5,680 non-boot actions, not arbitrary future ROMs.

| Measurement | Observed |
| --- | ---: |
| View instructions, input plus receive/draw | 10,041–10,056 |
| World instructions | 100–193 |
| Total instructions | 10,156–10,249 |
| Messages / payload bytes | 2 / 201 |
| Scheduler turns | 2 |
| Trace events | 6 of 128 available |
| Peak queued messages, view → world | 1 of 4 slots |
| Peak queued messages, world → view | 1 of 4 slots |
| Queue-full events | 0 |

Boot uses 10,037 view instructions and 17 world instructions, sends one 200-byte
snapshot, and produces seven trace events. The per-tick view counter sums two
evaluations; it is not a measurement of a single handler. The host's limits
remain 100,000 instructions per evaluation and 32 scheduler turns per tick.

Over 98% of guest instructions are in the presentation ROM, which redraws the
whole small scene. That is an instruction distribution, not a CPU-time profile:
host sprite writes and message handling have costs outside the instruction
counter. It suggests measuring rendering again when scenes become richer, not
optimizing or widening the processor now.

## Timing and memory

One local development-machine run, native and Node measured sequentially, then
Chromium in a separate run. Intel Core i7-9750H, x86-64 macOS 15.7.9, Apple clang
17.0.0, `-O2`, Emscripten 4.0.15, Node 26.5.0, HeadlessChrome 151.0.0.0.

| Synchronous step time, microseconds | Median | p95 | Maximum |
| --- | ---: | ---: | ---: |
| Native C | 59 | 93 | 544 |
| Node WebAssembly | 64.591 | 77.294 | 211.422 |
| Chromium WebAssembly | 100 | 200 | 700 |

Each measured host pass includes 5,680 steps, after discarding an earlier pass.
Each scenario starts fresh; native process and Wasm instance startup are not
included. Native timing brackets `garden_step`; Wasm brackets its thin exported
step wrapper. Both include guest drawing into the C pixel buffer and internal
trace/history work, but exclude test digest/readback, canvas presentation, DOM,
input latency, waiting between game ticks, and page loading. Operating-system
interruptions during a measured step remain included in elapsed time.
Boot is reported separately
by the probe and includes native ROM file loading.

Chromium's observed timer granularity was roughly 100 microseconds; some samples
round to zero. These are noisy local observations, not a host ranking, frame-rate
claim, mobile result, maximum throughput, or timing guarantee. There are no
timing-based pass/fail thresholds. The game's four-tick-per-second cadence is a
design choice, not a speed limit demonstrated by this test.

On this native build, `sizeof(Garden)` is **2,186,952 bytes** (about 2.09 MiB),
including the two Uxn structs, queues, traces, pixels, and history. Each guest
addresses 65,536 bytes, but the existing generic Uxn struct reserves 1,048,576
RAM bytes per node for 16 host-managed banks. This garden only exposes bank zero.
Do not describe the implementation as using only 64 KB of physical RAM per node.
These are struct sizes, not process RSS, total Wasm memory, or a memory leak test.

## Reproduce

From the repository root, with Emscripten activated and the existing SDL2 / Node
dependencies available:

```sh
make check garden-check constellation-recovery
make garden-web-check
make garden-measure
python3 -m http.server 8766 --bind 127.0.0.1 --directory build/web
```

With that server running, in another terminal with `agent-browser` installed:

```sh
node tests/garden_browser_check.cjs http://127.0.0.1:8766/tiny-neighbors/
```

The measurement command prints JSON counters and timing distributions. The
browser check prints its engine, parity count, and timing distributions, and
saves `build/garden-browser-check.png`. No measurement tooling is shipped in
`build/web`. Run timing jobs separately to reduce avoidable contention.

The measured ROM SHA-256 values are:

```text
view   307bb65e36d9cc224a30d5627ee772f4ec38ac71ccb0e4666a1fdc2f361d3078
world  7bd3316441cd5499f94eaa78aa0cd06a3cd9a381d6dced072cb7a67502301834
```

Parity fingerprints remain diagnostics, not cryptographic proof of full machine
state equivalence. Full native RAM/device/trace comparisons are in the existing
garden suite; cross-host tests here compare the snapshot and fingerprints, not
every byte of private machine state. Audio, pointer input, narrative assets,
multi-node routing, browser replay-file handling, and long-duration stability
remain outside this baseline.
