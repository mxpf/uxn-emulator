# The Last Visitor: private-account boundary experiment

As of 2026-09-10. This is a separate, deliberately small alternate fixture,
not another chapter of the mystery, a new host service, or an architecture decision.
The existing mystery and its two rule-data variants are preserved.

## First: clean checkpoint

Commit `e98b94f` on `constellation-game` checkpoints the preceding rule-data slice.
Malformed packed rules now set application error code 2 and report
`Mystery cartridge rule data is invalid.` rather than a message-send failure.
Code 1 retains the existing send diagnostic.

Verification before committing:
- 780,518 rule assertions under AddressSanitizer and UndefinedBehaviorSanitizer,
  plus all four malformed packed-rule cases asserting the exact diagnostic.
- Existing 424,610 single-ROM and 728,550 three-ROM mystery assertions.
- 94,277 exact cross-architecture state/text/pixel checkpoints.
- Runner: 2,769 behavior checks and 39 allocation-failure checks.
- Authoring regressions and 1,277 native/Wasm checkpoints per rule variant.

Unrelated local artwork/output files were not added to the commit. Nothing pushed.

## Question and experimental control

Does memory-enforced private narrative knowledge offer a useful behavior or
explanatory advantage over separate variables in one ROM?

This fixture reuses Bram, Cora, the missing ledger page and drawer, but is an
explicitly alternate account scenario. It does not amend the full mystery's plot.

Four actions, identical in both versions:

1. Read the desk record.
2. Ask Bram.
3. Return the page to the drawer; the record observes this event.
4. Show Bram the current desk entry.

Initially the record and Bram both say Cora has the page. Action 3 updates only
the authoritative record. Asking Bram still returns his old account. Only
action 4 teaches him what the record currently says. Asking afterward returns
the learned account.

Example `13242` produces this transcript in BOTH versions:

```text
DESK RECORD: CORA KEPT THE LEDGER PAGE.
DESK RECORD: CORA KEPT THE LEDGER PAGE.
DESK RECORD: THE PAGE IS NOW BACK IN THE DRAWER.
BRAM: CORA STILL HAS THE PAGE.
YOU SHOW BRAM THE CURRENT DESK ENTRY.
BRAM: THE PAGE IS BACK IN THE DRAWER.
```

The first line is boot output; the other five follow the five actions.
There is no ending or deduction score in this fixture. It tests information
flow, not play length, entertainment, or a new narrative framework.

## Actual implementation

### Two-ROM form

- Node 0, `examples/account-record.tal`: record at zero-page 10; presentation
  and player actions; pending request at 12.
- Node 1, `examples/account-witness.tal`: Bram's private belief at 11,
  initialized from authored data.
- Routes: external ff/07 to node 0; node 0/01 to node 1; node 1/01 to node 0.
- No additional presentation machine. No third actor, broker or knowledge service.

Node 0 cannot address node 1's RAM. It can obtain an account only by asking and
receiving the reply. Node 1 changes its belief only at boot initialization or
when its handler receives a valid teaching message from node 0.

Cartridge-specific protocol:

| Sender → receiver | Bytes | Meaning |
| --- | --- | --- |
| External → record | 01–04 | Player action |
| Record → Bram | 10 | Ask current account |
| Record → Bram | 20 vv | Learn record snapshot |
| Bram → record | 30 vv | Answer |
| Bram → record | 31 vv | Learning acknowledgement |

Here `vv=00` means kept and `vv=01` means returned.
Handlers check actual sender metadata, length, tag and value. The record also
requires the matching pending request before displaying a reply. Input remains
single-in-flight: the normal adapter drains each action before accepting another.

The witness does not poll the record, read C state, infer truth from UI text,
or share a pointer. Initial state and teaching authority are authored
assumptions. An authenticated record could still lie: provenance is not a truth
oracle. The host and test harness are trusted and can inspect all guest memory;
the enforced restriction is between guest machines, not security from the host.

### Single-ROM control

`examples/account-single.tal` holds record at 10 and belief at 11. Ordinary local
handlers read/update those variables. It has one external route, no internal
message bus, no simulated mailboxes and no artificial scheduler delay.

It preserves two distinct meanings: the current record and Bram's last learned
account. It does not collapse them to one variable merely to make the control
look worse. A local assignment on Show implements the same learning rule.

### Shared application shell and content

`src/account.c` uses the existing runner, router and core unchanged. A bounded
Console-byte sink is attached to node 0 through the existing attachment mechanism.
It stores text emitted by the ROM; it has no story strings, truth state or
deduction code. This is application presentation code, not a new shared device API.

`tools/account_play.c` provides terminal interaction, exact short-session replay,
and thin Wasm exports. `web/account/index.html` supplies four buttons, a variant
selector, transcript, restart/replay, and a raw trace with a protocol legend.
JavaScript does not calculate either account.

`examples/account.json` contains five text strings and an initial belief bit.
`tools/build_account.cjs` validates/packs that fixed fixture data. It is not a
runtime loader, narrative language or extensible resource system.

Bounds: 64 player actions, 2,048 archived trace events, 8,192 output bytes,
10,000 instructions per handler, at most 16 scheduler turns per action.
The existing four-slot/255-byte route queues are unchanged. The UI is not an
arbitrary-rate input adapter. Recording refuses additional actions at capacity.

## Exact evidence

`make account-check`, `make account-sanitize` and `make account-web-check`
each pass **139,090 assertions** in native normal, native ASan/UBSan and
WebAssembly execution respectively.

`tests/test_account.c` includes:

- All **1,024 five-action words**, checking every prefix: **5,120 exact
  transcript/domain-state checkpoints**, compared against an independent model
  and against the equivalent control. Both guests keep balanced stacks.
- Capture finishes before creating playback. Fresh executions compare exact
  archived events, all allocated guest RAM, stacks, devices, instruction counts,
  queues, scheduler/notification state and output. Host pointers are excluded.
- Reconstruction from trace deliveries plus known initial data and the
  documented protocol, independently of the guest RAM used for final checking.
  Learning packets and answer values are checked against this reconstruction.
- An extra external route exists ONLY in the adversarial test declaration.
  External `[20,01]` is delivered to Bram but rejected because its source is ff.
  The same teaching payload from node 0 is accepted.
- Return/read events leave the witness's entire allocated RAM byte-identical
  until communication reaches it.
- Stopping after the teaching SEND finds Bram unchanged with a queued packet.
  Only DELIVER changes his belief. The acknowledgement clears record pending state.
- Capture/replay ending while that teaching packet is still queued; exact replay
  restores that point, then continued execution agrees. Altered witness state
  and altered trace evidence are detected.
- `432` versus `342`: showing the entry before versus after returning the page
  yields different final accounts. The order matters, not elapsed wall time.
- Test-only ordinary Uxn store/read bytecode executed in node 0 can overwrite/read
  address 11 locally. In the single control that changes Bram's belief; in the
  two-ROM version it cannot reach Bram's separate address 11. The subsequent
  real Ask demonstrates the difference.
- Authenticated malformed witness packets: wrong lengths, tags and values;
  invalid external actions; 64-action refusal and full-record replay.
- Removing the witness receive vector makes Ask fault the whole routed session.
  Later record interaction fails too. This is not graceful failure containment.

The replay test at intermediate delivery boundaries is a fixture-specific
harness, not an exported arbitrary-turn replay format. The interactive shell
records actions and deterministically drains them, so its recording does not
represent arbitrary concurrent submissions or user-visible mid-action pauses.

`tests/account_browser_check.cjs` passes at 390×844 and 1440×900 for both versions:
buttons, keyboard order contrast, reset and replay, no horizontal overflow and no
reported page errors. The complete displayed transcript AND decoded routed trace
strings match native output exactly for `13242`; this comparison does not rely
on hashes. The full native model suite also runs separately in Wasm. This is
Chromium-engine verification, not physical-phone/Safari or exhaustive accessibility
testing. The browser skills were used to verify the actual page and interactions.

## A causal explanation of the sample trace

For the two-ROM `13242` run, sequence numbers include boot:

- Event 9 delivers external action 03 to the record: page returned.
- Event 17 delivers query 10 to Bram.
- Events 18/20 send/deliver his answer `30 00`: he still believes kept.
- Event 26 SENDS `20 01` from record to Bram. This alone does not teach him.
- Event 28 DELIVERS it; only here does his belief become returned.
- Events 29/31 acknowledge learning with `31 01`.
- Events 40/42 send/deliver the next answer `30 01`.

This is a concrete explanatory advantage of the trace: it identifies the actual
speaker and the learning delivery boundary. It still requires the initial ROM/data
and a cartridge-protocol legend. Raw bytes are not self-explanatory English.

The single trace contains the external actions; knowing its local handlers and
initial state reconstructs the same account evolution. It has no explicit internal
learning delivery event because learning is a local assignment. No human
debugging-time or comprehension study was performed, so “more inspectable” has
not been established as a measured productivity advantage.

## Comparison

Same post-boot `13242` script, excluding replay:

| Measure | Two ROMs | Single ROM |
| --- | ---: | ---: |
| Guest instructions, including Console output | 2,014 | 1,743 |
| Routed turns | 15 | 5 |
| Trace events | 41 | 15 |
| Native node/link allocations | 2,101,922 bytes | 1,050,446 bytes |
| ROM file bytes | 4,176 (4,041 + 135) | 4,041 |
| Handwritten guest source lines, including comments | 64 (42 + 22) | 25 |

These are one scenario and one native ABI, not wall-time, throughput, or general
ratios. Struct allocations exclude the common application record/output storage
and allocator overhead. They are dominated by the existing 1 MiB Uxn RAM
reservation per node, not actual story-state usage. Both browser modes use the
same Wasm module with a 32 MiB initial heap.

ROM sizes include fixed resource padding. The assembler omits unused trailing
zero data: the default witness's zero initial-belief byte at 0f00 need not extend
its file. Changing that byte to one changes file size without changing code.
Neither file size nor source-line count alone measures maintainability.

### Duplicated and synchronized state

Both versions require two semantic bits: current record and learned belief.
In multi, the record also retains a pending request byte. There are additionally
copied outbound, queued and receive payload bytes; old message bytes can remain
in buffers after use. These are transport copies, not synchronized shared memory.
A legitimately received answer exposes Bram's account at that time, not a live
view into his current private state.

Multi does not need a persistent second “Bram state” in the record. Single does
not need a pending request, acknowledgement, tags, or transport buffers.

### Authoring/revision exercise and friction

`tests/account_authoring_check.cjs` builds revisions in an isolated temporary
tree and runs the same native executable against them:

1. Change Bram's stale-answer sentence. One JSON edit changes record/presentation
   and single ROM resources; witness ROM remains identical. Both modes display
   the revised text and replay exactly.
2. Change initial belief from zero to one. Witness and single data change, record
   ROM remains identical, and executable regions remain byte-identical. Both modes
   now expose an initially conflicting account, as authored.
3. Six malformed configurations are rejected.

An initial test incorrectly expected a length-changing prose edit to preserve
all executable bytes. It failed: direct label addresses relocate when a string's
length changes. The measured result is no handwritten executable SOURCE edit,
but different assembled operands in BOTH presentation executables. This was
recorded rather than adding indirection or an asset service to avoid the cost.

Text/data separation does not inherently require separate machines. The isolated
witness allows its initial-data ROM to change without changing the record file,
but single achieves the same observable revision with one changed ROM. There was
no timed authoring study or protocol-evolution exercise, and no measured authoring
advantage. A more complex learning rule would require cartridge code changes in
the respective witness/control, not automatic use of a general schema.

## Finding, not architecture

The separate machine provides a **real enforced negative capability**:
record-node code cannot accidentally/directly inspect or overwrite Bram's
private variable. It also exposes request, source, learning delivery and response
as explicit causal trace events.

It does **not** enable a player behavior missing from the simple one-ROM control.
The control's distinct belief variable and explicit assignment express this
example naturally in 25 handwritten guest lines. The multi form adds protocol,
scheduling, physical allocation and replay state. The tests do not establish that
its enforcement/trace benefits justify those costs for a complete narrative game.

This is stronger evidence than the earlier three-role mystery, where the split
mainly redistributed responsibilities, but it does not settle the general
information-boundary hypothesis. No platform roles, one-ROM-per-character rule,
or preferred future architecture are promoted from this result.

## Limitations classified

| Observation | Classification | Disposition |
| --- | --- | --- |
| One-bit witness trusts one source; record is authoritative by premise, not independently verified truth | Cartridge/application | Deliberately bounded fixture |
| Four actions serialized to quiescence; no independently advancing actors or concurrent learning protocol | Cartridge/application | No general concurrency claim |
| Pending request and two request/two reply shapes in multi | Cartridge/application | Counted overhead, not platform concepts |
| Prose-length changes relocate operands; numeric/static layout | Authoring | Measured and documented; no loader/compiler framework added |
| No user study of causal readability, revision productivity or playability | Evaluation evidence gap | Not claimed proven |
| Witness failure terminates session despite private memory | Constellation platform's existing fault contract | Demonstrated, unchanged |
| Nodes reserve 1 MiB; practical state uses only a few bytes | Underlying Uxn representation / platform allocation cost | Counted, not changed |
| Replay needs exact initial bytes plus external input and scheduler boundary for in-flight states | Application replay responsibility on existing platform | Tested fixture only; no portable save/archive |
| Only the defined Console/Port profile has native/browser evidence | Application/device profile | Not general Varvara compatibility |

There is **no demonstrated requirement to change the Uxn Emulator**.
Core, Varvara, router, runner, existing Last Visitor sources and public site
behavior remain unchanged by this experiment.

## Run and handoff state

```sh
make account-check account-sanitize
# Terminal interaction:
bin/account-study multi
bin/account-study single
# Script and exact trace:
bin/account-study multi 13242 --trace
# With Emscripten active:
make account-web-check
python3 -m http.server 8773 --bind 127.0.0.1 --directory build/account-web
node tests/account_browser_check.cjs
```

The private preview is http://127.0.0.1:8773/. Use its selector to start either
implementation. Neither version is part of `playing-web` or the public carousel.
Recordings are bounded, in-memory, and lost on reload.

The earlier slice is committed at `e98b94f`. This experiment is implemented,
tested and documented locally; it is not pushed or published. Results are for
Constellation — Project Home to interpret before choosing subsequent work.
No durable design direction was changed.
