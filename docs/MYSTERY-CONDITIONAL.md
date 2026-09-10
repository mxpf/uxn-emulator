# The Last Visitor: discovery-gated follow-up

## Functional question

Can discovery in one cartridge component change another component's available
actions, then change the accepted conclusion through an explicit message,
without changing the shared runtime?

Implemented locally. This is a bounded continuation of the larger locked-room
experiment, not a new framework or a replacement definition of the milestone.

## Try the two branches

Inspect 1–5. Choose Cora (6 three times), sun key (7 once), ledger page (8 once).
Without the follow-up, 9 accepts that baseline conclusion.

Restart and repeat, but before concluding press Q or click **Q ASK CORA** at the
top right of the canvas. Inspecting the desk unlocks this action. Cora's authored
answer points to the drawer; the same interaction reports finding the page there.
The old conclusion is now rejected. Cycle 8 twice to **NOTHING**, then conclude.
The entry/key relationships do not change. Replay reproduces either path.

This is a simulated, authored answer and verification result, not an AI character
or a new independent actor. KEPT replaces TOOK to distinguish retaining the page
from previously taking it. The baseline answer represents the original evidence
branch; this test does not establish that closing an investigation without an
optional follow-up is sound puzzle design. A successful conclusion is terminal.

## Ownership and messages

| Component | New responsibility |
| --- | --- |
| Deduction | Learns the desk facts; applies the returned-page result once; selects one of two authored solutions. |
| Director | Unlocks Q from the returned desk evidence; guards requests; delivers the result; acknowledges completion. |
| Presentation | Displays locked/available/answered; rejects unavailable Q input; uses existing text and hit-testing commands. |

The existing five routes and three private-memory machines remain unchanged.
No role names or narrative rules are added to the emulator, router or runner.

1. Desk input travels presentation → director → deduction.
2. Deduction returns its six-byte evidence/choice/verdict snapshot.
3. Director sees both desk bits (0x18) and sets question state 0 → 1.
4. Its seven-byte presentation snapshot includes this availability state.
5. Q sends action 10 to director, which independently checks availability.
6. Director sends `[0x0a,0x01]` to deduction: the returned-page result.
7. Deduction checks source, length, exact payload, prior discovery and duplicate
   status, then applies it once and replies with message 11.
8. Director acknowledges state 1 → 2; presentation shows the answer.

Ordinary actions and deduction replies retain their existing shapes. Only the
director-to-presentation snapshot grows from six to seven bytes. The alternative
solution and prose are authored resources; the result protocol is cartridge code.

Early and repeated Q requests do not alter domain state or repaint the screen.
They still consume input/recording capacity and update execution bookkeeping:
they are not full-machine no-ops. Direct malformed or repeated result messages
are rejected independently by deduction. The session remains single-in-flight.

The native/web application shells only expand their admitted action range and
map Q. Browser code does not inspect ROM memory, infer availability from prose,
or decide which conclusion is correct. The raster attachment is unchanged.

## Cost and assessment

Three additional persistent bytes are used across the ROMs: director question
state, presentation's mirror, and deduction's applied-result flag. There is one
new input action, one two-byte result shape, and two authored solution arrays.
No additional VM, route, queue capacity, rendering command or execution limit.

In the reproducible fresh-session sequence desk → Q → duplicate Q, the native
test measures 7 / 9 / 3 routed turns and 19 / 23 / 7 trace events respectively.
These include scheduler bookkeeping and depend on the preceding scheduler state;
they are not universal per-action constants or a one-ROM performance comparison.
All remain within the existing 32-turn and recording reserves.

This demonstrates conditional cooperation without shared-runtime specialization.
It does **not** establish that three ROMs are cheaper or simpler than one, nor
that authored accounts are independently reasoning agents. The director now
owns a meaningful availability/acknowledgement transition, but this is still a
small, deliberately serialized protocol. Keep it local rather than promoting
it into a platform-wide dialogue or workflow abstraction.

## Verification

- 728,550 native checks pass, including an independent model over 4,096 evidence/hypothesis branches, with all 120
  discovery orders tested both with and without the follow-up.
- Early input, duplicate input, malformed source/length/tag/value, premature
  results, duplicate results, terminal state, input bounds and exact replay.
- Authoring checks change both solutions without changing executable code.
- 3,853 native/WebAssembly full-state fingerprints and actual browser pixel checks
  cover the changed conclusion, optional branch, replay and reset.
- AddressSanitizer/UndefinedBehaviorSanitizer pass all native checks. The shared
  runner regression passes 2,769 checks and 39 allocation checks.
- Browser controls remain checked across seven viewport sizes; the new action
  is exercised through keyboard and canvas at desktop and mobile sizes.

Run the commands in [the study guide](MYSTERY-OPENING.md#verification). The
private preview remains outside the public carousel; no deployment is implied.
