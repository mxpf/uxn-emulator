# Single-ROM mystery reference: first comparison

## Result and limits

The current mystery works in one ROM with the same authored content, visible
behavior and exact per-variant replay. The three-ROM version is retained.
For this small serialized application, the extra machines have not earned
their cost. One ROM is the preferred starting point for the next cartridge work.

This is a controlled **machine-count comparison**, not the entire three-way
architecture evaluation. Both variants use the same existing runner, input,
raster attachment and replay shell. The single variant has one external input
route and no inter-machine messages. No core, router or shared-runner changes.

This control deliberately stays within bank zero. It is not a plain Varvara
cartridge: it still uses the experimental input ports and existing raster
attachment. Banked content on ordinary Varvara, independently loaded content,
presentation replacement and fault-isolation exercises remain to be evaluated.
No new bank support was added to the routed host.

## Implementation

- `examples/mystery-single.tal` separates input/hit testing, event handlers and
  state-dependent deduction. State is cartridge-private, not C game logic.
- `examples/mystery-room.json` remains the common room/text/evidence/solution data.
  This is a single-room schema with local event handlers, not a generic engine.
- The compiler appends the existing presentation's view section to the single
  ROM. Sharing those drawing instructions holds layout and output constant.
  The cooperating ROM sources and authored content are unchanged.
- In the single ROM, state is at 00–07 and scratch at 08; input begins at 0800,
  immutable evidence/solutions at 0f00, and view resources at 1000. Code must
  remain below the input buffer. Resource offsets are test-local, not an ABI.
- The application shell can initialize either cartridge declaration. Replay
  remembers that choice and boots the same variant. Default targets still run
  the three-ROM version; single targets use a separate native binary and page.
- No room, suspect, question or deduction rules were moved into C or JavaScript.

The previous implementation and corrected direction were checkpointed locally
in commit `65a8bd3`. The reference is a separate addition, not a replacement
of that checkpoint.

The next [data-defined question experiment](MYSTERY-RULES.md) is now available.
It adds one prerequisite/consequence record without changing the host. The
measurements below describe the reference at checkpoint `1e867ea`.

## Measured scenario

The same 18-input script, `q12345qq666789889q`, exercises an early question,
discovery, a successful follow-up, duplicates, an incorrect proposal, revision
and the revised ending. Execution figures below exclude boot and replay.

| Measure | Three ROMs | One ROM |
| --- | ---: | ---: |
| Guest instructions | 30,294 | 25,938 |
| Routed turns | 142 | 18 |
| Recorded trace events | 362 | 54 |
| Node/link allocations, native ABI bytes | 3,153,398 | 1,050,446 |
| Total ROM file bytes | 6,917 | 6,023 |

These are scenario measurements, not universal ratios or timing benchmarks.
Node/link bytes exclude session, renderer, trace buffers and allocator overhead.
Much of the difference is the current Uxn structure's reserved 1 MiB per node,
not memory actually used by the mystery. Both browser builds still reserve the
same 32 MiB initial WebAssembly heap. ROM totals include padding and the
assembler's trailing byte; they are not executable-code-size measurements.

## Evidence

- Same independent native model: all 4,096 evidence/hypothesis branches and
  all 120 discovery orders on both paths. The single variant passes 424,610
  checks; the cooperating variant passes 728,550 (including its extra protocol
  and per-machine assertions). Assertion totals are not comparative quality scores.
- 94,277 direct cross-architecture checkpoints compare story state, recovered
  key, frame count, exact text and every pixel. Each architecture also verifies
  its own complete RAM/devices/stacks/scheduler/messages during exact replay.
- Both solutions and prose can be edited without changing the single ROM's
  executable bytes. Authoring tests also protect the input-buffer boundary.
- Each variant passes 3,853 native/WebAssembly fingerprint checkpoints using
  the same script. Browser controls pass the shared UI test suite for the single
  variant, including native pixel comparisons. Internal traces are not required
  to match between different architectures.
- Seven browser viewports cover the existing control layout. The conditional
  question is tested by click and keyboard, including early/duplicate input,
  revised conclusion, replay, and restart during replay.

Cross-platform fingerprints are hashes, not collision-free byte comparisons.
Direct native output and per-variant replay comparisons are exact.
The final AddressSanitizer/UndefinedBehaviorSanitizer run passes all 424,610
single-ROM checks with no reported sanitizer errors. An earlier attempt failed
at initialization while a concurrent build was replacing ROM artifacts; the
completed run used stable artifacts. Shared-runner regressions also pass 2,769
behavior checks and 39 allocation checks. Commands below reproduce the main
checks.

## Reproduce

```sh
make mystery-check mystery-single-check mystery-compare-check
make runner-check
# With Emscripten active:
make mystery-single-web-check
make mystery-web-check
python3 -m http.server 8771 --bind 127.0.0.1 --directory build/mystery-single-web
# In another terminal:
node tests/mystery_browser_check.cjs http://127.0.0.1:8771/ ./build/mystery_single_probe
```

Builds must finish before running replay checks: native replay deliberately
reloads ROM files, so rebuilding those files during a test can invalidate the
run. Do not run separate build processes against the same artifacts in parallel.

`make mystery-single` opens the native version. The private single-ROM page
does not change the public carousel or the original mystery page on port 8770.

## Next decision

The [data-defined question experiment](MYSTERY-RULES.md) now demonstrates that
one gate and its consequence can vary as authored data with identical executable
code. It does not need a separate loader. Consider independent content delivery
only for a demonstrated authoring or distribution benefit; keep replay tied to
the exact content and executable, and interpretation in the cartridge.

Do not add platform abstractions or grow the game differently in each candidate
to force a winner. Finish the cartridge in the simplest credible design.
Retain the multi-ROM substrate for cases that need and demonstrate its benefits.
