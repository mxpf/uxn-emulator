# A shared startup boundary, not a game engine

`constellation_runner.c` turns a trusted, in-memory application declaration
into a running routed host. No Escape! now uses this startup path instead of
allocating, wiring and booting its machines itself. Uxn and the routed host
are unchanged. Tiny Neighbors still uses its existing v0 host.

## Declaration and ownership

`RunnerDefinition` supplies:

- Ordered ROM byte spans. List position is node identity, never a role.
- Directed routes, including explicitly declared external inputs.
- Optional device attachments: node, inclusive port range, read/write
  callbacks and an application-owned context.
- The mandatory per-evaluation instruction ceiling.

`runner_start` takes zero-initialized/freed `ConstellationRunner` storage,
allocates exactly the declared node/link counts, loads the ROMs, attaches
devices and boots in the existing node order. It refuses to restart a live
runner. `runner_free` releases its allocations and is repeatable. The runner
and its device contexts must not move while live.

ROM bytes, routes and attachment descriptors are copied; their source
buffers can be freed after startup. Callback contexts are borrowed, not
cloned or freed. Hosts remain responsible for context lifetime, failure
reporting, deterministic device behavior and including device state in replay
comparisons. These are trusted C callbacks, not sandboxed plugins.

The runner validates ROM sizes, counts, attachment ranges and overlapping
attachments before guest execution. One attachment owns a given node/port;
the same port can be attached independently on different nodes. Ports d0–df
are reserved for the existing routed Port device and cannot be intercepted.
Unattached reads/writes and missing callback directions retain the routed
host's behavior and original node context. No device names, central service
registry, presentation-node requirement or fixed palette are introduced.

Startup failure releases all runner-owned allocations and returns false.
The optional third argument is a caller-owned `RunnerDiagnostic`, kept outside
the runner and definition storage. It is reset on each attempt and retains
the failure stage, reason, host fault and relevant node after cleanup. A
successful start clears an old error; a refused live start leaves the runner
untouched. `runner_diagnostic_text` supplies a short readable explanation.
When no node can be identified (including trace exhaustion without a fault
event), the node is `ROUTED_NONE`, not a guessed identity. No Escape! preserves
these messages for both initial startup and replay startup.

A failed boot is not rollback: an earlier ROM may already have called a
device. No external side-effect rollback or automatic restart is promised.
Instruction, queue, node and route limits remain those of the routed host;
the declaration is not permission to grow a queue or bypass a fault.

After startup, the existing `routed_step`, `routed_input` and
`routed_take_trace` APIs operate on `runner.host`. The caller still controls
turn pacing, trace consumption, total-run bounds and input policy. There is
no new scheduling loop hidden in the runner.

## What remains application-specific

No Escape! still declares its three ROMs, routes, drawing attachment and
palette in `square_demo.c`. Its world rules remain in ROMs; its recorder,
arrow mapping, frame pacing and admission policy remain demo-specific.
Its drawing callback now receives its own explicit context instead of
recovering the containing machine through a host-pointer layout assumption.

[Sketchpad](SKETCHPAD.md) independently uses the same runner with one ROM,
one external input route, its own paper-snapshot display attachment and a bounded
document-read attachment for save/reopen. The
runner does not acquire drawing commands or game roles to support it.

The runner does not read files or fetch URLs. Native and embedded-Wasm file
loading remain at the application boundary. There is no cartridge file
format, runtime ROM picker, universal recorder, standard screen contract,
save system or general-purpose browser shell yet.

## Evidence

`make runner-check` runs 2,769 behavior/diagnostic assertions and 39 allocation
failure assertions, including:

- One machine with no routes or attached devices.
- A three-node branching application: one sender targets both a relay and a
  collector, with 18 turns and 51 exact events.
- The same unchanged ROMs reordered into four nodes, including an independent
  idle node: 21 turns and 58 exact events.
- Both applications compared after every turn with an independently assembled
  raw routed host and a fresh runner replay. All guest RAM, stacks, device
  bytes, queue data, trace events and scheduler state are compared.
- Copied-definition lifetime, optional read/write attachments, reserved-port
  protection, metadata preservation, invalid declarations, boot ceilings,
  cleanup, independent per-node contexts, non-rollback of prior boot effects,
  and refusal to overwrite a live runner.
- Durable diagnostics for invalid declarations, boot instruction limits,
  undeclared boot sends and trace exhaustion. Each of the four runner
  allocation sites is independently forced to fail and checked for cleanup.

`make runner-web-check` runs the behavior/diagnostic assertions in WebAssembly. Its test
harness uses a 256 KiB stack (with overflow checking) to hold three complete
trace-bearing hosts during comparison. Browser verification uses only local
test artifacts, outside the publish directory:

```sh
python3 -m http.server 8768 --bind 127.0.0.1 --directory build/runner-web
node tests/runner_browser_check.cjs http://127.0.0.1:8768/
```

The browser's complete test output must match the native run. Native runner
and square suites also pass AddressSanitizer/UndefinedBehaviorSanitizer.

For the migration, No Escape!'s full 1,137-checkpoint diagnostic transcript
was captured before the refactor and compared byte-for-byte afterward: no
change. Its 131,368 native checks, 31 startup-error checks, SDL controls, native/Wasm parity, paced
input, true Pause and exact replay remain covered by the existing suites.
The broader routed-host and Tiny Neighbors tests remain separate regressions.

This proves reusable startup for these arrangements. It does not prove every
topology makes progress, every device is portable, or arbitrary applications
can share No Escape!'s drawing and recording conventions.
