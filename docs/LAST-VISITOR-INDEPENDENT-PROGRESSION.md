# The Last Visitor: finite independent-progression comparison

Implementation report, 2026-09-10. Submitted to Constellation — Project Home
for interpretation, not an architectural decision.

## Bounded finding

Two responsibilities now perform a finite sequence of actions between player
interactions. Their ordering changes a real Last Visitor deduction: did Bram
seal the ledger before Cora returned its page, or did he wait for her?

The separate desk machine physically protects Bram's progression and sealing
record from stores made by the Cora/mystery ROM. Its delivery trace usefully
exposes an offer in transit, a changed desk record, and a receipt not yet learned
by Cora. Those are stronger enforced boundaries than ordinary local variables.
However, the natural single-ROM control produces the same player-visible
investigation and outcomes with two ordinary local progression functions.

The most informative new result is authoring friction: making Cora offer the
page one progression step earlier changes the single-ROM result but not the
multi-ROM result. The existing incoming-route rotation still delivers Bram's
queued continuation before the offer. The trace explains this, but the author
must understand an additional transport-order constraint to revise the scene.

For this finite incident, separation has not demonstrated enough application
or authorship benefit to establish that Bram's responsibility earns a machine.
This does not establish single-ROM as a default or disprove value for longer
independent behavior, replacement, or failure containment.

## Scope and exact limitations of the experiment

Repository: `/Users/mxpf/Projects/uxn-constellation-game`; branch
`constellation-game`; base commit
`c228e71109d6d78382ad13d7327eff29ab58335a`,
`Checkpoint bounded Last Visitor knowledge-dependent deduction comparison`.
This new slice is uncommitted and local. Nothing was pushed, published, or
added to the public Playinghaus artifact.

The full revised kDrive design document was read before implementation:
`/Private/03 Projects/Uxn Emulator/Constellation Narrative Game System Design.md`.
Its boundary-must-earn-its-cost direction guided the comparison. It was not
edited. Neither the north-star wording nor an architectural default changed.

This is a **finite run-to-quiescence incident**, not an ongoing NPC simulation.
Q starts it once. Cora and Bram subsequently advance via their own finite
continuations without another player event. The player inspects before and
after, not during every scheduler turn. Existing `MysterySession` finishes an
interaction in at most 32 turns before accepting another one; that was retained.
Intermediate states are tested through existing raw routed-step operations.

The single-ROM control intentionally completes its ordinary local scene routines
inside one guest callback. It does not fake distributed mailboxes, private RAM,
or transport latency. Equivalence is claimed at settled player-visible cuts,
not at every internal turn. Consequently this experiment does not establish
equivalence for arbitrary player interruptions during an incident or autonomous
behavior spanning multiple unresolved player interactions. That remains an
evaluation evidence gap, not a hidden platform change.

## Player-facing incident

The existing room, five hotspots, seals, key, original false denial, original
no-Q solution, three answer fields, conclude, replay and restart remain.

1. Reading the desk unlocks Q, now labelled `Q RETURN`.
2. Asking Bram before Q both obtains his sun-key evidence and explicitly asks
   him to wait for Cora's page. His text says that a later request is too late.
3. Q has Cora fetch, approach and offer the page while Bram checks, closes and
   finishes his desk round. No wall-clock duration determines these actions.
4. If the wait request arrived in time, Bram accepts the page into the ledger
   before sealing it. Otherwise he seals first and puts the returned page in
   the sealed inbox. A request after sealing cannot rewrite that history.
5. Afterward, Cora reports her receipt; Bram reports what happened at the desk.
   Both must be consulted again. Old evidence does not satisfy this comparison.
6. Field 8 changes from KEPT to WHY and clears its previous selection. Choices
   are `BRAM SEALED FIRST`, `BRAM WAITED`, and `NO PAGE RETURNED`.
   The first two fields still identify Cora and the sun key.
7. The correct WHY depends on the actual incident outcome. A wrong explanation
   is rejected and can be revised. The original investigation is still required;
   the return does not erase the earlier missing-page evidence.

Example complete scripts (digits are existing controls):

- `4q1235666789`: no timely wait request; inbox; sealed-first deduction succeeds.
- `12345q236667889`: timely request; ledger; waited deduction succeeds.

This changes actions and the destination of evidence, rather than merely
teaching a belief flag. The deduction is simple and heavily signposted. Visiting
the right evidence and choosing an answer does not prove human understanding;
no independent player study or comparative comprehension measurement was done.

## Implementation and ownership

New guest sources are `examples/mystery-progression.tal` (common investigation),
`mystery-progression-local.tal`, `mystery-progression-remote.tal`, and
`mystery-progression-desk.tal`. Text and labels are in
`examples/mystery-progression.json`. This is an opt-in alternative follow-up,
not another chapter or a cumulative expansion of every prior fixture.

Multi-ROM uses two machines, not one ROM per character:

- Node 0 owns the existing investigation/presentation, Cora's progress at ZP
  `0c`, and her received outcome at `0d`. `0b` tracks an account request.
- Node 1 owns Bram's phase `10`, timely-wait flag `11`, seal state `12`, and
  authoritative page location `13`. It has no display attachment.
- Cora: 0 idle, 1 fetching, 2 approaching, 3 page offered.
- Bram: 0 idle, 1 checking, 2 closing, 3 finished. Closing seals immediately
  unless a timely request was received; finishing seals regardless.
- Location: 0 no page, 1 sealed inbox, 2 ledger. The receipt is a distinct copy
  of that result, transferred explicitly; it is not shared RAM.

Five fixed routes use existing runner declarations: external/7 to node 0,
0/1 to 1, 1/1 to 0, 0/2 to 0, and 1/2 to 1. Self routes were already legal.
All packets are two bytes:

| Packet | Direction | Meaning |
|---|---|---|
| `10 00` | main to desk | Ask; request a wait if still in time |
| `40 00` | main to desk | Begin Bram's finite round |
| `20 00` | main to desk | Offer the returned page |
| `30 location` | desk to main | Account response |
| `31 location` | desk to main | Receipt |
| `50 phase` | main to itself | Continue Cora from expected phase |
| `60 phase` | desk to itself | Continue Bram from expected phase |

Source is the routed device's origin, not a claimed payload identity. Expected
phase guards reject stale continuations; begin is one-shot; repeated offers do
not replace the recorded location. Send failure marks the cartridge failed;
there is no retry/recovery protocol.

Single-ROM stores the same responsibilities as ordinary local fields and calls
`cora-progress` and `bram-progress` in the three local scene moments. Offering
the page directly calls local logic that checks the seal; asking directly sets
the wait field when permitted. There is no local emulation of a network. Its
own receipt and desk record are retained for clear responsibility/reporting
semantics, although the single form could potentially avoid that duplication.

Only `include/mystery.h` and `src/mystery.c` variant selection/ROM declarations
change in the C application shell. Raster drawing, input validation, settle,
replay, trace consumption and failure handling are unchanged. The builder adds
two explicit opt-in targets using its existing literal assembly substitutions.
The web packager reuses the existing page/input code and adds the Q button and
instructions. No browser-side narrative state or progression timer was added.

No change to `src/uxn.c`, routed host, runner, device facility, separate Uxn
Emulator repository, public page sources, or public packaging target. No loader,
save system, event bus, actor service, dynamic topology or new instructions.

Slice manifest (repository-relative):

- Modified: `Makefile`, `README.md`, `include/mystery.h`, `src/mystery.c`,
  `tools/build_mystery.cjs`. Makefile/README also retain unrelated earlier hunks.
- Added: this report; the five `examples/mystery-progression*` files listed
  above; `tests/test_mystery_progression.c`,
  `tests/mystery_progression_authoring_check.cjs`,
  `tests/mystery_progression_browser_check.cjs`, and
  `tools/build_mystery_progression_web.cjs`.
- Generated and ignored: the `build/mystery-progression-*` ROMs, TAL, Wasm and
  web directories, native test/probe binaries, and `build/progression-*.png`
  browser evidence. None is a public deployment.

## Tests and exact evidence

Commands run successfully:

- `make mystery-progression-check`: native C suite plus disposable authoring probe.
- `make mystery-progression-sanitize`: same C suite with AddressSanitizer and
  UndefinedBehaviorSanitizer, with warnings treated as errors.
- `make mystery-progression-web-check`: same C suite compiled to WebAssembly
  and run in Node; playable Wasm builds and native pixel probes also built.
- `node tests/mystery_progression_browser_check.cjs`: local pages on 8776/8777.
- `make mystery-knowledge-check mystery-compare-check runner-check`: regressions.
- `git diff --check`: passes.

Final C suite: **17,148 assertions, 582 exact player-visible checkpoints,
44 paired scenarios**, passing natively, under sanitizers, and in Wasm.
The stable-cut oracle is written separately from guest TAL and JSON. It checks
all relevant narrative state, stacks, failure flags, exact text, pixels and frame
counts. Every paired scenario replays with exact recorded trace and final full
machine equality, including RAM, devices, stacks, queues, cursors, instruction
counts and rendered state. This is deterministic execution replay, not saving.

The bounded matrix is two request timings x four post-return account subsets x
four WHY values (32 cases), plus seven targeted order/repeat/baseline paths,
one all-click path, two complete measured paths and two incident measurements.
On the late path, not asking Bram also omits his baseline evidence; early-path
cases independently exercise the fresh-account gate. This limitation is noted
rather than claiming all cells isolate exactly one cause.

Additional raw boundary/fault probes establish:

- Cora starts before Bram; then Cora advances to approaching while Bram remains
  checking; then Bram seals while Cora remains approaching.
- The offer can be queued with neither desk location nor Cora receipt changed.
  Delivery changes the desk record before Cora receives its receipt.
- Bram completes his entire round with no Cora progression and no page offer.
- A wait request sent while Bram is checking can be delivered only after his
  self-continuation seals the desk. Send time is not effective receipt time.
- That delayed boundary script is run twice with identical full final machines
  and byte-identical traces. It uses test-only real guest device sends at exact
  raw-step boundaries, not elapsed time, fabricated queue entries, or spoofed
  origins. This is **not** a claim that the ordinary UI recording represents
  arbitrary mid-incident test injections; it does not.
- Real guest STZ to Bram's wait-field address in the main ROM changes the
  single-ROM outcome but not the separate desk's outcome. This is a deliberate
  mutation probe of enforcement, not evidence of a naturally observed bug rate
  or security against a hostile host process.
- Wrong lengths/values, a main-origin fake desk continuation, unstarted page
  offer, invalid Cora continuation, duplicate begin and stale final continuation
  are ignored without advancing the protected state.
- A missing desk receive vector terminates the whole session with
  `ROUTED_RECEIVE_VECTOR_MISSING`; replay is refused for that failed session.
- Altering a recorded self-delivery causes replay to fail with
  `Replay trace differs.`

A small test decoder reconstructs both progressions, seal, destination and
receipt from authenticated deliveries and the documented transition rules,
without consulting RAM until its final assertion. It checks all 44 normal
multi-ROM scenarios plus the raw timing/absence probes. Trace is not semantic
prose: cartridge rules are still necessary to interpret it.

For example, after `4q`, these exact delivery sequence numbers explain the
late path (SEND, TURN and external input events are omitted here):

| Sequence | Delivery | Interpretation |
|---|---|---|
| 13 | 0 to 1: `40 00` | Bram begins checking |
| 16 | 0 to 0: `50 01` | Cora approaches |
| 19 | 1 to 1: `60 01` | Bram closes and seals without wait request |
| 22 | 0 to 0: `50 02` | Cora offers page |
| 25 | 0 to 1: `20 00` | Desk receives page after sealing: inbox |
| 28 | 1 to 0: `31 01` | Cora receives inbox receipt |
| 30 | 1 to 1: `60 02` | Bram finishes round |

Browser: **82 native/browser SHA-256 pixel comparisons**, both variants,
early/late outcomes, wrong and revised deductions, real Q canvas hotspot,
buttons and keyboard, replay/restart; no page errors. Full-flow checks use
390x844 and 1440x900; all controls also fit at 320x568 and 667x375 without
scrolling, with at least 44x44 targets. Screenshots were visually inspected.
This is desktop Chromium viewport emulation, not physical mobile-device testing.

Regression results: prior knowledge 27,620 assertions / 687 checkpoints / 37
scenarios; old three/single comparison 94,277 checkpoints; runner 2,769 checks;
allocation suite 39. Earlier knowledge ROM SHA-256 values remain exactly those
in its completed report. No full Uxn compatibility campaign was rerun because
the core did not change.

## Measurements

Native build, post-boot. Node/link bytes use actual `sizeof` allocations, not
logical state size or whole-process RSS. They exclude session trace storage,
pixel buffers, device bindings, transient loading buffers and allocator overhead.
Replay creates another machine set. Paint instructions dominate full scripts.

| Script / variant | Guest instructions | Scheduler turns | Trace events | Node/link bytes |
|---|---:|---:|---:|---:|
| Late `4q1235666789`, multi | 21,581 | 31 | 83 | 2,103,982 |
| Same, single | 21,008 | 12 | 36 | 1,050,446 |
| Early `12345q236667889`, multi | 26,883 | 39 | 104 | 2,103,982 |
| Same, single | 26,174 | 15 | 45 | 1,050,446 |

Isolating Q after `4`: multi 2,164 instructions / 9 turns / 26 events;
single 1,832 / 1 / 3. After `24`: multi 2,160 / 9 / 26;
single 1,829 / 1 / 3. Both commit one final incident frame. These compare actual
implementation costs, not equal-granularity scheduler work or benchmarked latency.

Wasm guest instruction/turn/event results are identical. Node/link allocations
are 2,103,934 bytes multi and 1,050,422 single due to ABI layout.

Actual ROM file lengths: main multi 6,880 bytes, main single 6,880 bytes, separate
desk 289 bytes. Fixed resource origins/padding hide code-size differences:
nonpadding executable prefix through last nonzero byte before rule data is
1,301 bytes multi versus 1,133 single, plus 289 desk bytes. Neither prefix is a
general static instruction-count metric. Physical lines including comments:
common TAL 82, local implementation 25, remote 30, desk 37.

Main receipt duplicates the desk destination intentionally. The multi form also
has a pending-account flag, five links rather than one, two send buffers and
four internal continuation messages per incident. No wait/seal/phase replica is
maintained in the main ROM. The single has no pending-account protocol and no
internal transport; both retain local progression fields and reportable receipt.

ROM SHA-256:

```
multi  02e5951e6fe19797ca39ea7410630771b64a5615a0f769cfcedb7a62ed3c3e6b
single ac8745a171eda36cce1072d078ccd16285dd04185d59c9b2df9015007d1b1987
desk   0b708eb747f3a9692f3dc782b319d3f6855b0e9660b0c53e8ae3ff592b8e05b5
```

## Authorship, failures and classified limitations

**Cartridge/application and authoring:** prose is shared between variants, but
the new follow-up needed manual TAL work, explicit message/choice indices,
post-incident evidence guards and state-dependent deduction logic. This is not
a general authoring language. A comma in initial prose failed the inherited
bounded glyph validator; wording was changed instead of expanding the device.
An unused fixed follow-up rule was removed from this fixture's generated output
because the result depends on the actual receipt, not a second fixed answer.

**Multi-ROM authoring friction:** continuations need expected-phase validation,
origin checks, source-specific dispatch, receipt guards and send-error handling.
The authored sequencing must account for incoming-route rotation as well as
each responsibility's local phases. Single-ROM uses ordinary local call order.

`tests/mystery_progression_authoring_check.cjs` makes disposable copies only.
It establishes prose-only edits leave both executable TAL prefixes unchanged;
six overlong text/choice/question builds are rejected. Its concrete timing
revision makes Cora offer at her second rather than third progression step.
Single needs one condition edit; remote needs its continuation limit, offer
threshold and receipt guard edited. With neither Bram nor routes changed,
baseline receipts are `[inbox, inbox]`; revised receipts are `[inbox, ledger]`.
In multi, the already-queued Bram continuation is chosen before the earlier
offer. This mismatch is preserved as evidence, not patched away or presented as
parity. Temporary revision files are removed; playable originals are unchanged.
This is one controlled revision probe, not a statistically meaningful authorship
study, and the control's explicit local order is itself an authored choice.

**Constellation platform:** idle scheduler turns execute no guest code. Guests
run on receive/writable events. Existing self routes can express finite guest
continuations, but there is no general autonomous tick. Route rotation and
delivery order are real semantics, not bugs silently fixed for this experiment.
No new scheduling API was introduced. Also, one guest execution fault stops the
whole routed host; separate RAM does not provide application failure containment.

**Cartridge/application:** the existing 32-turn settle limit, 128-input recording,
8,192-event archive and run-to-quiescence player interaction model remain. This
fixture fits them; there is no recovery, persistent background activity, or new
way to save/reload. Resource loading remains the explicit existing ROM list.

**Evaluation evidence gaps:** no arbitrary interleaving of real player input
during the incident, large cast, long-running evolution, measured human debugging
speed, independent author trial, device testing, or generalized fault recovery.
Boundary/state enforcement is directly tested; reduced accidental-bug frequency
and material human causal-inspection benefit are not broadly proven.

**Uxn Emulator:** no new underlying dependency or needed change was discovered.
This uses the same ROM execution, memory, instructions and device callbacks.
Scheduling, routing and whole-session fault behavior belong to Constellation;
the scene's protocol and timing assumptions belong to the cartridge/authoring.

## Comparison and interpretation boundary

Observable behavior is identical at the intended player cuts. Both preserve
independent fields and correctly distinguish early, absent and late requests.
Only multi enforces no cross-responsibility RAM stores. Its trace gives exact
delivery-level explanation, including the demonstrated delayed request, and was
useful in diagnosing the revision mismatch. Single can explain its normal
outcome from local state, external action order and short local routines without
inventing a transport model. We did not measure which explanation humans find
clearer or faster.

The changed destination matters to the deduction in both forms; additional
transport detail is not itself player-facing evidence. Separation adds protocol,
allocation and revision constraints here. It offers real mechanical protection
but no demonstrated new player capability or authorship advantage in this slice.
The core hypothesis about longer independent evolution remains open beyond
this finite incident. Project Home should interpret this result before choosing
more work. Nothing here adopts one-ROM-per-character, a single-ROM default,
self-messaging as platform architecture, or a new north star.

## Current state as of now

Completed local, uncommitted finite-progression comparison over c228e71. Native,
ASan/UBSan, Wasm and browser evidence passes for the original fixture; the
deliberate earlier-return revision exposes a transport-order mismatch and is
retained only as a test/report. Playable previews: `http://127.0.0.1:8776/`
(multi) and `http://127.0.0.1:8777/` (single). No commit, push, public artifact
change, durable-design edit or architecture choice was made. Existing unrelated
account-study Makefile/README hunks and account files, `.DS_Store`, `design/`
and `output/` remain preserved. Implementation stops here for Project Home's
interpretation.
