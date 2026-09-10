# The Last Visitor: playable reasoning study

The original key/lock opening has been extended toward the larger locked-room
experiment in the living kDrive document `Constellation Narrative Game System
Design.md`. It is no longer just the opening interaction.

See [authored mystery and architectural assessment](LAST-VISITOR-MYSTERY.md)
for the six facts, accounts, contradiction, solution and remaining questions.
That document contains spoilers. The next functional slice is documented in
[discovery-gated follow-up](MYSTERY-CONDITIONAL.md): desk discovery unlocks
an optional question whose result changes the accepted conclusion.

The current clue revision identifies visitors through genuine seals, connects
an unnamed key handover to entry order, and replaces the explicit receipt with
a torn-paper match. It changes cartridge prose only, not the room or runtime.

## Play

```sh
make mystery
```

Click five room hotspots or press 1–5 to read the three accounts, desk evidence
and door. Press 6–8 to cycle the three hypothesis fields; press 9 to conclude.
All fields start unanswered. Read every hotspot before submitting; the desk
supplies two facts. Incorrect guesses are revisable without losing evidence.
Q (or the top-right canvas label) asks Cora after inspecting the desk. The
question is optional and can be used once, before a successful conclusion.
Her returned-page result changes the accepted KEPT answer to NOTHING.
R replays, N restarts, Escape closes the native window.

With Emscripten active:

```sh
make mystery-web
python3 -m http.server 8770 --bind 127.0.0.1 --directory build/mystery-web
```

Open `http://127.0.0.1:8770/`. The build remains outside `build/web` and is
not included in `playing-web`, the carousel or the public release. All assets
are local; no external fonts, services or uploads are required.

## Milestone coverage

| Larger experiment | Current study |
| --- | --- |
| One illustrated room | Deliberately simple ROM-drawn study |
| Three characters or accounts | Three written visitor accounts |
| Five inspectable hotspots | Ada note, Bram note, portrait, desk, door |
| Six discoverable facts | Six evidence bits, two learned together at desk |
| One contradiction | A denial conflicts with trustworthy entry evidence |
| Three correct relationships | Player chooses visitor, entry method and object |
| Ending acknowledges conclusion | Identifies the false account and opens the exit |
| Deterministic replay | Exact native state/messages/pixels; browser parity |
| Approximately five minutes, understandable | Requires a fresh-player playtest |

## Three cartridge-owned responsibilities

| Node | Owns | Communicates with |
| --- | --- | --- |
| 0: presentation | Hit testing, layout, text, visible hypothesis and exit | External input; director |
| 1: director | Recovered key, pending interaction, question availability, final exit consequence | Presentation; deduction |
| 2: deduction | Six learned facts, three proposed relations, returned-page result, acceptance | Director only |

Uxn, the shared runner and routed host remain unchanged. Each machine retains
private memory. These roles and protocols belong only to this cartridge.

External selector 7 delivers three bytes to presentation: `0,x,y` for a click
or `1,action,0` for actions 1–10 (10 is Q). Presentation sends a one-byte action to
director on route 1. Director forwards actions 1–9 to deduction on route 2.
For an available Q request it instead sends the explicit result `[10,1]`.
Deduction returns six bytes on route 1: message ID, evidence mask, visitor
choice, entry choice, object choice, verdict. Director authorizes the exit
only for a requested conclusion with the recovered key, then sends seven
bytes to presentation: the six fields with byte 5 representing actual exit
state, followed by question state (0 locked, 1 available, 2 answered).

Every handler checks source, length and ranges. The director accepts a
deduction response only while an interaction is pending. Completed state is
stable; subsequent ordinary actions redisplay the conclusion and Q is ignored. Hypothesis choices start
at zero, then cycle 1–3. All facts and all three correct nonzero choices are
required. Failed proposals reveal no per-field score.

Presentation zero page 00–06 mirrors its seven-byte display status. Director
00 owns key, 01 exit, 02 pending action, 03 question state. Deduction 00 owns
evidence, 01–03 choices, 04 verdict, 05 whether the returned-page result applied. FE in each ROM reports an unexpected send failure.
These offsets are test/implementation details, not a platform ABI.

## Authored content and display

`examples/mystery-room.json` holds title, messages, choice labels, evidence
masks, baseline/follow-up solutions, question labels, rectangles and five story hit regions. The cartridge-local
build helper packs resources into presentation and deduction ROMs. Native
and web use those same compiled files. Both prose and solution can change
without host or guest executable-code edits; authors must keep their prose,
options and solution semantically consistent.

This is a fixed three-field schema, not a generic narrative language.
Presentation code must remain below its input buffer at 0800. Output is at
0900; immutable presentation resources start at 1000.

The presentation attachment at port 25 still supports only clear, rectangle,
bounded ASCII text and commit. Registers 20/21 hold x/y, 22/23 dimensions,
24 one of seven colors, 26–27 the text address and 28 the length. Each text
command allows at most 160 bytes and validates memory and pixel extents.
Commands render to a staging surface; commit publishes the frame.

The application adapter now accumulates all text rows into a bounded
1,024-byte accessible frame, rather than exposing only the last text command.
This includes the currently selected answers. Overflow fails the frame.
There are no story strings or interpretation in the adapter.

## Scheduling, bounds and replay

One action runs to quiescence before admitting another: at most 32 routed
turns with a 10,000-instruction evaluation ceiling. Idle time runs no guest
instructions. Existing four-slot queues suffice for this single-in-flight
protocol. Unexpected FULL is an explicit error, not silent input loss.

The session records at most 128 inputs and 8,192 trace events, reserving 128
events before each input. At capacity it refuses further actions but retains
the complete replay. Replay boots three fresh ROMs, compares every trace
batch exactly, then compares all guest RAM, stacks, devices, queues, scheduling,
instruction counts, text and display buffers. Shell pacing is not game time.
Restart cancels replay; a successful replay permits continued live play.

Reloading loses the session. There is no portable recording, saved-game
format, clock, audio or resource service. Sketchpad documents are not reused
as universal machine snapshots.

## Verification

```sh
make mystery-check
make mystery-web-check
node tests/mystery_browser_check.cjs http://127.0.0.1:8770/
```

- 728,550 native checks: all 4,096 discoverable evidence-subset/hypothesis
  branches, all 120 inspection orders on both paths, repeated reads, premature guesses,
  wrong-answer revisions, cycling, terminal state, malformed protocols,
  input/click/display bounds (including object-name targets), recording limits
  and repeated exact replay.
- Authoring tests change prose and solution independently, verify executable
  code remains identical, and reject malformed strings, masks, choices,
  rectangles and action IDs.
- 3,853 native/WebAssembly canonical full-state/queue/trace/pixel fingerprints,
  including all 64 unanswered/answered hypothesis combinations on both paths,
  early/repeated questions, and replay. The conditional report explains the protocol.
- Browser: real object and name clicks, keyboard, focused-button activation, incorrect
  proposal and correction, native pixel comparisons, replay/reset cancellation,
  seven responsive viewport sizes and no page errors. The follow-up is exercised
  through canvas and keyboard at desktop and mobile sizes, including changed
  acceptance, repeated requests and reset during replay.
- Shared-runner regression: 2,769 checks and 39 allocation checks.
- AddressSanitizer and UndefinedBehaviorSanitizer: all native checks
  pass with no reported memory-safety or undefined-behavior errors.

Cross-platform fingerprints and pixel hashes are not collision-free exact
byte comparisons; native replay comparisons are exact. Scripted SDL coverage
uses a hidden window, not physical macOS mouse input.

## What remains unproven

This reaches the requested mechanical scope, not a finished commercial game.
The clue logic is intentionally explicit. Fresh-player testing must assess
fairness, discoverability, length and enjoyment. There is no claim yet that
splitting the study across three ROMs is simpler than one ROM: director remains
small, accounts are authored data rather than independently reasoning agents,
and the additional messaging has a real cost. Evaluate that cost before
adding a framework, services or more machines.
