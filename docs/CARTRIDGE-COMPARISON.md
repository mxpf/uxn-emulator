# Complete cartridge first; architecture must earn its cost

## Direction

“Constellation is an experimental computer built from compatible Uxn machines,
designed to discover how sophisticated, expressive, and durable software can
become without sacrificing small understandable parts.”

This correction supersedes any interpretation of the earlier experiments as
a commitment to multi-ROM software. Preserve them. Do not extend their
architecture merely because that is the path already started.

## Repository assessment

- The ordinary loader accepts ROM data beyond bank zero (`src/uxn.c`). Varvara
  supplies bounded bank operations (`src/varvara.c`); the core still addresses
  its normal 64 KB. Banked data does not require larger core addresses.
- The routed experiment intentionally rejects ROMs over 65,280 bytes and does
  not expose extra banks. Both its loader and the shared runner enforce this.
  It is not a more capable replacement for ordinary Varvara in every respect.
- Routing, queue pressure, scheduling, external input and replay have useful
  regression coverage. These establish behavior within tested bounds, not a
  general advantage for multi-ROM application design.
- Sketchpad already demonstrates that the shared runner can host one ROM.
  The system does not need multiple machines to count as Constellation work.
- The mystery has three ROMs, five routes and cartridge-owned rules. Its latest
  conditional action successfully changes an accepted conclusion through
  messages without changing the core or shared runner.
- This is a small serialized state machine. No existing mystery behavior has
  been shown to require more than one machine. Physical separation is real;
  practical authoring and maintenance advantages remain unmeasured.
- `mystery-room.json` and its compiler already separate authored prose, labels,
  evidence and solutions from executable code. Authoring tests confirm this.
  That benefit comes from data separation, not inherently from separate ROMs.
  The data is compiled into ROMs today: it is not independently loaded content.

The supplied Oquonie evaluation is further evidence against assuming a low
single-machine ceiling. It reports a substantial illustrated, banked Uxn game
running in the ordinary emulator and a useful older Room/Event/State structure.
This assessment did not repeat that evaluation; its reported room count, screen
dimensions and ROM size are not new measurements here. Neither Oquonie's
success nor its large authored world file decides our architecture by itself.

## First control completed

A [single-ROM reference and measured comparison](MYSTERY-SINGLE.md) now preserve
the same mystery behavior on the existing input/display shell. This isolates
machine count without changing host devices. It is not yet the banked Varvara
candidate or the independently loaded data candidate. The initial measurements
favor one ROM for this mystery; the three-ROM version remains available.

A subsequent [data-defined interaction test](MYSTERY-RULES.md) varies one gate
and consequence with identical executable bytes. This improves authoring within
embedded data; separately loaded content is still unimplemented and must justify
its additional delivery/validation machinery.

## Milestone and immediate step

The milestone is one complete, substantially expressive mystery cartridge:
exploration, persistent facts and consequences, conditional interactions,
player-built conclusions, coherent endings and deterministic replay, running
natively and in the browser. The larger locked-room scope remains the product
target; the key/door opening and the Q follow-up are only functional slices.
Graphical polish and playtime are not the present acceptance gate.

Before adding another platform service, make a single-ROM reference of the
current mystery using cartridge-owned Room/Event/State data. Preserve the
three-ROM version unchanged as the comparison candidate. Keep the same room,
inputs, clues, conditions, conclusions and outputs; do not handicap either
candidate with different content or claim a win from visual polish.

Use the existing ordinary emulator's bank support for the banked candidate.
Do not add banking to the routed host just to conduct this comparison. Keep
content bank-addressable, but cross a bank boundary with useful illustrated
content only when needed; padding a tiny room does not demonstrate sophistication.
Any additional deterministic input or display support needed for comparable
native/browser tests must be explicit, bounded and counted in the host cost.

Once the behavior-equivalent reference works, compare independently loaded data
against embedded data. Do not build three unrelated games or a universal
cartridge framework. Use the result to select the implementation in which to
finish the complete cartridge.

## The three candidates

| Candidate | What it must demonstrate | Cost to count |
| --- | --- | --- |
| One ROM, embedded banked data | Data-driven content and room/state transitions without splitting machines | Bank layout, guest data access, build/authoring work and native/browser support |
| One ROM, independently editable cartridge data | Change validated content without recompiling executable code | Loading, validation, bounds, packaging, portability and content identity during replay |
| Presentation, director and deduction ROMs | A material benefit from independently replaceable or isolated parts | Private RAM, duplicated state, protocol/version coordination, queues, traces and authoring work |

Independent data means a separate validated content payload loaded by a bounded
byte-oriented mechanism, not a host that interprets room or deduction semantics.
It may share an outer distribution package with the ROM. It must work offline.
Replay must identify the exact executable and content bytes or reject a mismatch;
asset changes cannot silently reinterpret a prior recording.

## Fair evidence, not architecture preference

Run the same behavioral oracle and input recordings against each candidate:
premature actions, discoveries in different orders, unlocks, repeated actions,
wrong conclusions and revision, endings, restart and replay. Compare visible
output and domain state across designs. RAM layouts and internal traces will
legitimately differ; require exact replay within each design, not identical
internal traces between different architectures.

Perform the same small authoring exercises: change a clue and its condition,
add one state-dependent interaction, replace presentation without changing
the rules, and introduce malformed content or a failing component. Record:

- Which executable/data files change, and what must be rebuilt and retested.
- Whether a fault is contained and whether the remaining application can
  actually continue; separate RAM alone is not graceful fault recovery.
- What makes an outcome explainable, and whether the trace is useful without
  decoding application-specific numeric messages by hand.
- ROM/data size, allocated and used memory, instructions, turns, trace volume,
  load/validation code, and native/browser differences.
- New host responsibilities, dependencies and explanatory burden.

Use the same assets and scenario for measurements. Do not treat the current
mystery's 7/9-turn examples as a single-ROM comparison or evidence of savings.
Document tradeoffs rather than inventing a numerical winner from code size alone.

## Decision gate

Default to the simplest credible version that meets the cartridge requirements.
Promote multi-ROM only for a demonstrated, material improvement in authoring,
isolation, replaceability, causal inspection/replay, or needed behavior that a
simpler candidate cannot reasonably supply. Its benefit must justify its costs.

If it merely redistributes code, reject it for this cartridge. Retain the tested
routed substrate for future experiments where cooperation may have a real use.
No new compulsory roles, narrative services or emulator instructions follow
from this assessment. No candidate is selected as proven superior yet.

This is the broader comparison plan. The linked single-ROM report records the
first implementation and its limits. No cartridge is published or discarded.
