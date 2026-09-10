# The Last Visitor: knowledge-dependent deduction comparison

Local implementation report, 2026-09-10. For Constellation — Project Home to
interpret; this is not an architectural decision.

## Bounded finding

Differing knowledge now affects an actual Last Visitor conclusion. The player
must distinguish an outdated account from a fresh lie after witnessing the
return of the ledger page. Both implementations support the same investigation,
wrong explanation, revision, and ending.

The separate witness machine provides a real but narrow correctness guard: a
store in the mystery machine cannot silently overwrite Bram's belief. Its trace
also makes the teaching delivery explicit. Neither advantage has yet produced
an additional player-facing capability or a demonstrated authorship benefit in
this deduction. The single-ROM control expresses the distinction naturally with
a local belief flag and ordinary read/update routines. This experiment does not
establish that this responsibility earns a machine, or that single-ROM should
be the project default.

## Checkpoint and scope

Repository: `/Users/mxpf/Projects/uxn-constellation-game`, branch
`constellation-game`, HEAD `e98b94f` (`Checkpoint data-defined mystery rules with
accurate startup diagnostics`). The new addition is uncommitted and unpublished.
The preceding standalone account experiment and other pre-existing untracked
files were preserved. Nothing was pushed, deployed, or added to Playinghaus.

This is an opt-in version of the existing mystery, not a second laboratory UI.
The original three-ROM mystery and original single-ROM rule variants remain
available and unchanged in behavior. To isolate the information-boundary cost,
the experimental multi-ROM form uses the existing single-control mystery as
its main ROM plus one private witness ROM. It does **not** extend the older
presentation/director/deduction topology to four machines. That is a comparison
fixture choice, not a replacement architecture or a one-ROM-per-character rule.

Tested ROM SHA-256 identities (the slice is not yet a commit):

```
multi:   67c5edfcb3bd3634521f51d01a6e4c212c0ce56156e9f7ba42f4ca129210b0d4
single:  c5f55eb7ca2c07fad6d825c87341d1b425ab284610c0d93b5f8e57695ecae5cb
witness: 801b5f53a9e73922997ed8319ef0ac8bd56cbbca632e787c7853b99977a396f0
```

No change was made to `src/uxn.c`, `src/constellation_routes.c`,
`src/constellation_runner.c`, the separate Uxn Emulator repository, the north-star
wording, or the narrative system design document. The only C application-shell
change selects explicit local ROM sets and existing routes for two additional
variants. There is no new host capability or narrative interpretation in C/JS.

## What the player does

The same room, five hotspots, key, seals, entry record, answer controls, conclude
button, replay, and restart are retained.

1. Bram's initial account says he gave Cora the sun key, saw her holding the
   page at six, and left then. His last-known account is that she still has it.
2. Reading the desk unlocks the existing Q follow-up. Q has Cora admit taking
   the page and return it at seven, in front of the player. Bram is absent.
3. Field 8 changes from **kept what?** to **why do the accounts differ?** Its old
   answer is cleared. The alternatives are Bram lied, the return was faked,
   and Bram left earlier. The first two fields still identify Cora and the key.
4. Asking Bram now, without teaching him, yields the old account and explicitly
   says he has heard nothing since leaving. Repeated questions, rereading the
   desk/Cora, and changing answers do not teach him.
5. Q is now **show Bram**. This is a distinct learning interaction. A subsequent
   Bram response acknowledges the return and recalls what he believed before
   learning. Showing him before asking is also valid; he can recall his earlier
   account, so that order does not lock the player out.
6. The revised conclusion requires a post-return Bram inquiry and the explanation
   that he left earlier. A pre-return inquiry does not satisfy this comparison.
   An accusation that Bram lied is rejected and can be revised to open the exit.
   Showing Bram is optional: understanding the stale account is sufficient.

Cora's original denial is still the original mystery's false account; the
new legitimate difference concerns Bram's knowledge versus the subsequently
observed return. Cora and the desk acknowledge the return on later rereads.

The no-Q original solution remains available. Thus the new deduction is required
on the return/follow-up branch, not on every route through the entire mystery.
As with the original finite-choice evaluator, selecting the correct answer and
visiting required evidence is observable; actual human understanding cannot be
proven by that gate. This is a simple, strongly signposted deduction, not a claim
of a difficult or user-validated mystery.

## Implementation and information ownership

Primary new files:

- `examples/mystery-knowledge.tal`: shared investigation/return/comparison logic,
  derived from the existing single-ROM control.
- `examples/mystery-knowledge.json`: bounded additions to the existing room's
  messages, third answer, Q labels, and revised solution.
- `examples/mystery-knowledge-local.tal`: two ordinary single-ROM routines.
- `examples/mystery-knowledge-remote.tal`: main-side request/reply handling.
- `examples/mystery-knowledge-witness.tal`: private Bram state and handlers.
- `tools/build_mystery.cjs`: opt-in, exact assembly substitutions; reuses the
  original view, room, data validation and resource packing. No general language.
- `tools/build_mystery_knowledge_web.cjs`: packages the existing mystery browser
  shell with a follow-up button and neutral third-answer wording.
- `tests/test_mystery_knowledge.c`,
  `tests/mystery_knowledge_authoring_check.cjs`, and
  `tests/mystery_knowledge_browser_check.cjs`: evidence below.

Main owns the observed return and investigation/deduction state. Bram's private
RAM byte `09` initially means last-known held by Cora. Only delivery of an
accepted learning message changes it to returned. An inquiry reads it without
updating it. Main does not retain a copy of the returned account value.

The initial observation at six is authored backstory represented by the initial
belief and text, not a separately simulated past scene. Six and seven are
narrative timestamps, not real-time clock readings. The experiment tests the
later opportunity to learn and the consequences of its absence.

Multi has two nodes and three fixed routes: external input to main, main to
witness, witness to main. Two-byte cartridge packets are:

| Direction | Packet | Meaning |
| --- | --- | --- |
| Main → witness | `10 00` | Ask for current account; never teach |
| Main → witness | `20 01` | Show the observed return |
| Witness → main | `30 00` / `30 01` | Old / learned account |
| Witness → main | `31 01` | Learning acknowledgement |

Witness accepts the expected length/tag/value only from node 0. Main accepts
the expected witness response from node 1 with a matching pending operation.
One pending byte is cleared after a reply. There are no identity credentials
inside payloads; origin is supplied by the existing routing device.

Single holds the same independent belief at its own RAM byte `09`:
`ask-bram` reads it; `learn-bram` sets it. It uses no internal packets, fabricated
isolation, mailbox, or scheduler delay. Both variants retain a comparison flag
and Q/UI progression state. The latter records that teaching was acknowledged,
so it overlaps semantically with the learned flag in this monotonic fixture.
Multi adds pending-request state and transient request/reply copies, not a
synchronized second narrative database.

## Exact evidence

### New native, sanitizer, and Wasm tests

`make mystery-knowledge-check` and `make mystery-knowledge-sanitize` pass.
The latter runs the same suite with AddressSanitizer and UndefinedBehaviorSanitizer.
`make mystery-knowledge-web-check` runs the same C suite compiled to Wasm.
Each final run reports **27,620 assertions, 687 exact cross-control checkpoints,
37 paired scenarios**, plus focused boundary/protocol probes outside that scenario
count. Assertion totals include state, replay and trace checks; they are not
27,620 distinct narrative situations.

The intentionally bounded decision matrix is 16 cases: learned/unlearned ×
fresh comparison/no fresh comparison × all four third-answer values, including
unanswered. Supplemental paths cover:

- locked Q, pre-return Bram inquiry, return without sharing, repeated inquiries;
- share before inquiry and inquiry before share; duplicate sharing;
- reread Cora/desk after return without changing Bram;
- old answer clearing, wrong explanation then correct revision;
- wrong identity, wrong entry method, absent desk, absent other evidence;
- original no-Q ending, revised ending, terminal inputs;
- actual canvas-coordinate equivalents of the new successful path;
- 128-input recording boundary and fresh full-state replay.

At every paired action the independent C oracle checks the expected message ID,
evidence, selected answers, exit, question phase, key, learned state, comparison
flag, clear pending state and empty guest stacks. Cross-control comparisons
check exact pixels, text, frame count and relevant shared state. Each variant
replays from fresh ROM state and compares its own full machine memory, queues,
device/stack state, instructions and exact trace, rather than expecting different
architectures' raw machine states to be identical.

Focused probes establish:

- The complete witness RAM is unchanged by return plus repeated Cora/desk
  observations. Absence of communication is tested, not inferred from prose.
- After teaching is queued, Bram is still stale; delivery changes him; main's
  Q state advances only after the acknowledgement. This checks intermediate
  execution boundaries, not merely settled end states.
- A test-only external route delivers a forged learning packet with origin 255;
  the witness rejects it. This route is absent from the playable build.
- Wrong learning value, extra payload byte, wrong tag, and malformed query are
  rejected even when test bytecode sends from the authorized main node.
- A real guest store at address `09` in main, after the return but without sharing,
  makes single erroneously report that Bram was shown the return. The same store
  cannot alter multi's Bram: he still reports hearing nothing. This is deliberate
  fault injection, **not an organically encountered bug** or a measured defect rate.
- A missing witness handler causes the existing global terminal routing fault;
  separation does not provide fault recovery.
- Altering the recorded teaching delivery makes replay fail with
  `Replay trace differs.`

### Causal reconstruction

The tests reconstruct learned/unlearned state from external action history in
single, and from authenticated delivery history in multi, without taking that
reconstruction from Bram's RAM. Nonterminal order/absence scenarios exercise it;
the general behavioral oracle separately handles terminal actions.

For the measured investigation `12345q2q266678889`, multi's exact event sequence
contains the following, with hexadecimal payloads:

- 41: main sends `10 00`; 43: witness receives the query.
- 44: witness sends `30 00`; 46: main receives the still-stale account.
- 52: main sends `20 01`; Bram has not learned merely because it was sent.
- 54: witness receives `20 01` and updates its belief.
- 55: witness sends `31 01`; 57: main receives the acknowledgement.
- 63/65: next query is sent/delivered.
- 66/68: learned account `30 01` is sent/delivered.

This is a more explicit transport-level explanation of the learning boundary.
It still needs the cartridge's protocol meanings and initial conditions. The
trace alone does not establish that a fictional return is true, prove the
authored times, or explain arbitrary English. In single, the external action
sequence plus its two local handlers explains the same behavioral change cleanly.

### Browser and authoring checks

`node tests/mystery_knowledge_browser_check.cjs` checks both local versions:
88 exact native/browser pixel comparisons, the wrong-guess/revision path,
stale and informed accounts, real canvas Q clicking, buttons, keyboard,
replay/restart, no page errors, and responsive fit at 320×568, 390×844,
1440×900 and 667×375. The full deduction is exercised at phone and desktop sizes.
These are automated Chromium viewport checks, not physical-device or human tests.
Browser verification also confirmed initial surfaces and controls visually.

`tests/mystery_knowledge_authoring_check.cjs` changes the stale-account prose in
a disposable source copy. Both generated executable TAL sections remain identical;
only resources change. Eight negative builds reject overlong message lines,
choice labels, Q labels, and an invalid solution. This demonstrates a small prose
revision, not general author productivity or arbitrary narrative revision.

Original regressions rerun successfully: 728,550 three-ROM mystery checks,
424,610 single-ROM checks, 94,277 original cross-architecture checkpoints,
original authoring checks, both SDL scripted endings, and 2,769 runner plus
39 runner-allocation checks. `git diff --check` passes. No claim of a new full
Uxn compatibility run is made; its implementation was not changed.

## Costs, not a substitute for deduction evidence

Same 17-action investigation `12345q2q266678889`, excluding boot, native build:

| Measurement | Main + witness | Single |
| --- | ---: | ---: |
| Guest instructions, including rendering | 29,877 | 29,432 |
| Scheduler turns | 41 | 17 |
| Trace events | 107 | 51 |
| Live node/link allocation bytes | 2,101,922 | 1,050,446 |
| ROM files, actual filesystem byte count | 6,845 + 129 | 6,845 |
| Main code span before fixed rule area | 1,169 | 1,021 |

Witness executable span is 128 bytes. Fixed resource addresses pad both main
ROMs, so equal main file lengths conceal different executable sizes. Wasm has
the same instructions/turns/events and 2,101,874 versus 1,050,422 node/link bytes
because structure layout differs. Allocation counts are two node objects/three
link objects versus one/one, stored in the runner's existing arrays.

These byte counts exclude common display/session/trace structures, temporary
ROM reads, allocator overhead, and a second live machine during replay. Both
browser builds use the same 32 MiB initial Wasm memory setting; this is not a
claim that observed browser process memory doubled. Turns are deterministic
scheduler work, not wall-clock latency or fictional elapsed time.

The marginal work is 445 guest instructions, 24 turns, 56 trace events and
1,051,476 native node/link bytes for this script. Rendering dominates guest
instructions. The single state access/update implementation occupies two source
lines; the multi implementation has 20 lines of main protocol code and 18 lines
of witness code, including comments. That line count excludes shared source,
setup substitutions, shell selection and tests, and is descriptive rather than
a universal complexity score.

## Authoring friction and failures encountered

Content-model friction, affecting both versions:

- The fixed three-answer model forced reuse of field 8. It needed an explicit
  switch from KEPT to WHY, clearing an old answer, and a fresh-comparison guard.
  The new answer has its own authored solution rather than accidentally borrowing
  the original follow-up's coincidentally identical numeric choice.
- The first implementation draft left Cora/desk rereads describing the missing
  page after its return. Inspection caught this before handoff; post-return
  account messages and tests now cover it. Private witness RAM would not have
  fixed that mistake.
- Sharing before asking initially required careful treatment in the design:
  the informed response retains the earlier account, so that legitimate order
  does not make the explanation unavailable.
- The 40-column/four-line/160-byte text limits and indexed resource tables
  require concise prose and explicit index changes for additional messages.
  The Q availability highlight and small-screen follow-up button also needed
  local presentation adjustments.
- This opt-in shared TAL derivative preserves old controls at the expense of
  duplicated baseline investigation logic. Future baseline edits would need to
  consider it too. That is experiment-maintenance cost, not evidence that a
  narrative architecture inherently requires duplication.

Multi-specific friction:

- The local read/set became request tags, authenticated source checks, payload
  validation, a pending operation, a separate reply path, and acknowledgement
  ordering. More tests are needed for intermediate delivery and failure paths.
- The main cannot directly overwrite witness memory, but it can still send a
  valid teaching packet too early if authored incorrectly. Physical isolation
  forces an explicit interaction; it does not validate narrative truth.
- Wording and the final display remain in main. It could ignore a truthful
  reply or render the wrong text. This fixture enforces the state boundary, not
  an unforgeable witness utterance or total narrative correctness.
- Loss of the witness handler faults the whole session. Added boundaries here
  improve neither availability nor recovery.

No failed normal-play scenario remains known in the tested matrix. This does
not imply exhaustive testing of arbitrary traffic, edits, or narratives.

## Answers to the seven experiment questions

1. **More naturally correct / harder to violate?** Harder to violate by direct
   accidental memory access: yes, demonstrated in the actual mystery. Naturally
   clearer overall: not established; explicit local belief is already simple.
2. **Clearer causal explanation?** Yes for the exact teaching-delivery boundary
   and origin. It is a narrower advantage than explaining all narrative causality.
3. **Does that advantage matter to the deduction?** The independent knowledge
   distinction matters: it changes the account and required explanation. Physical
   separation has not added a player-visible advantage on correct executions.
   Its fault-injection protection could prevent one class of wrong account, but
   no real defect-rate or comprehension advantage has been measured.
4. **Authoring and revision?** Prose revision is shared and equally simple. The
   main content difficulties occur in both. Communication adds bookkeeping; no
   observed authoring gain compensates for it in this small addition.
5. **Natural single-ROM equivalent?** Yes. One belief flag, direct read/update,
   no internal simulated distribution. Behavior and pixels match.
6. **Understanding, testing, maintenance?** The same mystery is presented. Single
   has fewer protocol obligations; multi localizes knowledge mutation and offers
   explicit delivery evidence. Longer-term maintenance and human understanding
   are not proven by this experiment.
7. **Does the responsibility earn a machine?** Not demonstrated here. There is
   positive evidence for a narrow guard and trace benefit, but not for a necessary
   or materially better player-facing deduction. Project Home should interpret
   that result without promoting either implementation to a default.

## Limitations and classification

| Limitation | Classification |
| --- | --- |
| One monotonic learned bit, one teaching source, initial observation authored rather than played, no forgetting or contested teaching | Cartridge/application fixture |
| Optional branch, finite-choice answer gate, explicit clues; no proof of human comprehension or enjoyable deduction | Cartridge/application and evaluation limits |
| Three answer fields, packed resource indices, short ASCII text, duplicate experimental baseline logic | Experimental authoring/content model |
| Single-flight request/reply; normal adapter settles before next input; malformed missing response can leave pending without a timeout | Cartridge/application protocol, not a new platform requirement |
| Missing handler produces global terminal fault; fixed topology and bounded queues already exist | Constellation platform behavior, exposed but unchanged |
| Per-machine physical Uxn state is relatively large | Existing Uxn representation/platform allocation cost; no newly discovered change requirement |
| Main can teach prematurely or render false prose; host/tests can inspect or mutate all RAM | Limits of this cartridge's trust model; no hostile-host security claim |
| Replay depends on exact initial ROMs and accepted input history; no portable save/archive or cross-version replay promise | Application replay scope on existing platform |

No new underlying Uxn Emulator dependency requiring a change was found. This
continues to depend on its existing instruction execution, per-instance memory
and device callbacks, and on the existing Constellation runner's route-origin,
queue, scheduling and trace semantics.

## Reproduce and current state

From the repository root:

```
make mystery-knowledge-check mystery-knowledge-sanitize
source /Users/mxpf/Projects/keepinghaus-emsdk/emsdk_env.sh
make mystery-knowledge-web-check
```

Serve `build/mystery-knowledge-multi-web` locally on 8774 and
`build/mystery-knowledge-single-web` on 8775, then run
`node tests/mystery_knowledge_browser_check.cjs`. Local previews:
`http://127.0.0.1:8774/` and `http://127.0.0.1:8775/`.
Representative screenshots are `build/knowledge-{multi,single}-{stale,informed}-{390,1440}.png`.

**Current state for Project Home:** The Last Visitor now has a tested local
knowledge-dependent deduction and a behaviorally matching natural single-ROM
control. Separate witness state is genuinely protected from other guest memory
access; explicit delivery gives a useful causal audit point. The same deduction
works cleanly without that separation, and no player-facing or authorship benefit
has yet been shown to justify the additional machine. Tests and findings are
complete for this bounded slice. It remains uncommitted/unpublished, with no
architectural decision or further implementation selected pending Project Home's
interpretation.
