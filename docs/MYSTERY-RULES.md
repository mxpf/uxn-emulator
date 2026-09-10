# Data-defined question: prerequisite and consequence

## Result

One single-ROM executable now supports two differently behaving authored
question variants. A two-byte record controls which facts unlock Q and which
existing solution applies after the question. The reply text is authored data.

The prior single-ROM reference is checkpointed locally at `1e867ea`. This
extension changes cartridge code and its build-time data compiler only; the
Uxn core, router, runner, C application shell and browser frontend are unchanged.

## The two content variants

| Variant | Q requires | Consequence after Q | Private preview |
| --- | --- | --- | --- |
| Returned | Desk evidence (mask 24) | Use the follow-up solution: Cora, sun key, nothing | Port 8771 |
| Retained | Desk and door evidence (mask 56) | Keep the baseline solution: Cora, sun key, ledger page | Port 8772 |

Before Q, both use the baseline solution. A successful conclusion is terminal.
An early or repeated question leaves story state and pixels unchanged, while
still consuming an input in the bounded recording.

Both are authored alternatives, not two accounts offered in the same playthrough.
The retained answer explicitly confirms that Cora kept the page; the returned
answer reports finding it in the drawer. Human authors remain responsible for
the consistency of their prose, evidence and solutions.

Try the gate difference by restarting, reading the desk, and pressing Q. In
Retained it stays locked until the door is also inspected. Complete all five
inspections and choose the appropriate conclusion to compare the outcomes.

## Contract

The configuration files are:

- `examples/mystery-question-returned.json`
- `examples/mystery-question-retained.json`

Each has exactly three fields:

- `requiredFacts`: integer 1–63, all specified fact bits must be learned.
- `solution`: `baseline` or `followup`, selecting one of the existing authored
  three-field solutions after the question has been answered.
- `answerText`: existing bounded ASCII message format, at most four 40-character
  lines and 160 total bytes.

The compiler rejects missing/extra fields, invalid masks, unknown consequence
selectors and malformed text. The rule is packed at guest address 0f0b:
one prerequisite-mask byte followed by one selector byte (0 baseline, 1 follow-up).
The guest checks those byte ranges before presenting the room. A malformed
packed rule stops startup; it cannot reach interaction handling. Cartridge error
code 2 reports `Mystery cartridge rule data is invalid.`; code 1 retains the
existing send-failure diagnostic. These codes are application-local.

This is a cartridge-local, fixed operation—not an expression evaluator, arbitrary
action list, pointer-writing mechanism, language, or platform-wide narrative API.
Q still has one-shot semantics and operates on the same existing state fields.
Other interactions are still cartridge code. These numeric masks are a small
experimental authoring interface, not a finished editor.

## What “same executable” means

Both assembled ROMs have byte-identical executable regions (0100–0eff in guest
memory), with SHA-256:

`58ef2db6bbe2456714e3a53223b88666a7266145aea21b23464f539492f5666b`

Their other evidence and solution constants also match. The differences are the
two-byte rule and the authored answer's text/resources. The whole ROM files are
not identical: Returned is 6,023 bytes, Retained is 6,017 bytes.

The build still assembles/packages each ROM. This is **not** a separately loaded
content implementation, hot reload, or proof that distribution requires no build.
Native tests run the exact same compiled C executable against each content
snapshot. Browser builds use the same C sources and alias each ROM to the existing
embedded path; no new file-loading or story interpretation code was introduced.
The full WebAssembly bundles differ because they contain different ROM data.

## Verification

- The native model specifies expectations independently of the configuration:
  4,096 evidence/answer branches and 120 discovery orders per variant.
  Returned passes 391,731 checks; Retained passes 388,787.
- Both paths cover early questions, repeated questions, partial evidence,
  terminal behavior and exact replay, including continued play after replay.
- Twelve malformed authored records are rejected by the compiler.
- Four deliberately invalid packed rule records fail before the first frame.
- Both variants pass 1,277 native/WebAssembly full-state fingerprint checkpoints,
  including replay/reset and recording limits.
- Browser tests at 390×844 and 1440×900 exercise the actual controls, different
  gates, wrong-answer revision, outcomes, duplicates, replay/reset, and compare
  displayed-pixel hashes against native output.
- The default Returned variant retains all 94,277 direct state/text/pixel
  comparisons against the existing three-ROM cartridge. The earlier
  single-ROM model's 424,610 checks and authoring checks also pass.

Native replay and direct cross-architecture comparisons use exact byte checks.
Native/WebAssembly fingerprints and browser pixel hashes have the ordinary
collision limitation of hashes. `make mystery-rules-sanitize` runs both complete
native rule variants and all four malformed packed records with AddressSanitizer
and UndefinedBehaviorSanitizer. The rejection cases assert the precise diagnostic.
The diagnostic correction changes the application shell, not the shared runtime.
The 2026-09-10 checkpoint run passed 780,518 rule assertions under both
sanitizers, all four exact diagnostic checks, the existing 424,610 single-ROM
and 728,550 cooperating-ROM checks, 94,277 cross-architecture comparisons,
runner regressions, and 1,277 native/Wasm checkpoints per rule variant.

## Reproduce

```sh
make mystery-rules-check
make mystery-rules-sanitize
make mystery-single-check mystery-compare-check
node tests/mystery_authoring_check.cjs
# With Emscripten active:
make mystery-rules-web-check
# Serve in separate terminals:
python3 -m http.server 8771 --bind 127.0.0.1 --directory build/mystery-single-web
python3 -m http.server 8772 --bind 127.0.0.1 --directory build/mystery-retained-web
node tests/mystery_rules_browser_check.cjs
```

Finish builds before running tests. Variant tests take their own temporary ROM
snapshots so the same native binary can run both without replacing project ROMs.
Neither variant is published to Playinghaus.

## Assessment

This demonstrates useful data-defined behavior with a deliberately small cost:
one two-byte record and cartridge-side interpretation. Separate content delivery
has not been necessary to obtain that benefit. Preserve this boundary and use
it to develop the cartridge; do not automatically turn it into a generic engine.
A separate loader remains an option only if independent content distribution
or editing provides a concrete benefit worth its validation and replay costs.
