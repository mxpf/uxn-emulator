# The Last Visitor: five-minute reasoning study

## Authored mystery

The player is in a locked study after its three visitors have left. A ledger
page is missing. Accounts are written notes, not live characters. There are
no unseen visitors: the study's entry record is explicitly complete. Seals
and records are genuine; one visitor's account is false. These are the
puzzle's stated premises, not forensic claims about real evidence.

Five hotspots expose six facts:

1. Ada's round-sealed note: she left as Bram arrived; the ledger was whole.
2. Bram's square-sealed note: he passed the sun key to the next guest;
   the ledger was still whole when he left. He does not name that guest.
3. Cora's triangle-sealed note: she says she never entered the study.
   The sun key is now behind this portrait; inspecting it acquires the key.
4. Desk, fact one: complete entry order, round then square then triangle.
   The record gives no names; the accounts supply each seal's owner.
5. Desk, fact two: a ledger page is missing and the stub left in the ledger matches the
   scrap in a triangle-sealed envelope. There is no explicit named receipt.
6. Door: intact sun-key-only lock, no other entrance, window sealed.

The contradiction is Cora's denial versus the genuine triangle-sealed entry.
The player
constructs three relationships: Cora was last; she entered with the sun key;
she took the ledger page. Alternatives are Ada/Bram, window/forced door,
portrait/nothing. Each field starts unanswered. All six facts must be read,
all fields filled, and all three relations correct before acceptance. A
wrong proposal receives no per-field correctness score. There is no penalty
or forced inspection order. Notes can always be reread.

The conclusion identifies Cora's false account, traces the square-to-triangle
key handover and the envelope's paper match, and unlocks the door. It does not
invent a murder or motive.

## Clue revision: require connections, not a named answer sheet

The first version named Cora in both a timestamped entry and a signed receipt.
This revision preserves the room, all nine actions, evidence masks, solution,
three ROM responsibilities and host. Only presentation text resources change.

| Relationship | Information the player connects |
| --- | --- |
| Last visitor | The three notes identify seal owners; the complete entry order ends in triangle, so Cora came last. |
| Entry method | Bram owns square; he gave the sun key to the next guest; triangle follows square; the intact door admits no other method. |
| Object taken | The ledger was whole when Bram left; a page is now missing; a matching scrap is in the triangle-sealed envelope; its owner is the final visitor. |
| Contradiction | Cora denies entering, but her genuine seal is in the entry record. |

The seal-to-owner labels are physical metadata, not additional spoken claims.
With genuine records as the stated premise, the entry refutes Cora's denial;
the one-false-account premise then supports using Ada's and Bram's accounts.
There are no repeat or unrecorded entries. The paper match supports possession
and taking in this deliberately closed puzzle, not a claim that a scrap alone
would establish guilt in a real investigation.

This is still an introductory puzzle. The identity mapping is simple and the
runtime still checks three authored answer IDs; this revision asks the human
player to connect clues, not the emulator to become an inference engine.
The mandatory read-every-hotspot gate and the UI are intentionally unchanged.

Verification of this revision: native and authoring tests pass; executable
presentation bytes through 0fff are identical to the prior build, and the
entire director and deduction ROM files are identical. The presentation ROM
was 5,673 on-disk bytes after the seal revision, before the final stub wording
clarification. This is a concrete content-only edit,
not evidence that the three-ROM design is better than a one-ROM alternative.

The revised build also passes 1,906 native/WebAssembly fingerprint checkpoints
and browser object/name/button, keyboard, wrong-answer correction, exact pixel
comparison, replay/reset and seven-viewport checks. Those verify delivery and
behavior, not whether a fresh player finds the revised inference satisfying.

## Architecture and evaluation

Presentation owns the room, five object hit regions, three cycling answer
controls and a conclusion control. Director owns the recovered key and final
exit progression. Deduction owns six learned fact bits and the proposed
three-part hypothesis. Only deduction can return an accepted verdict; only
director can turn that verdict into an open exit. All are cartridge-local
roles. No emulator or shared-runner changes are required.

The three machines are not independent characters with private beliefs.
Accounts are data; deduction holds discovered evidence centrally. This is a
meaningfully richer test of the original split, not proof that three ROMs
are simpler than one. Director is still small. Compare the reasoning and
communication overhead before introducing further services or roles.

The authoring JSON contains the prose, options, evidence masks and solution.
Text and solution revisions compile without host edits. This remains a fixed
three-field cartridge schema, not a generic narrative engine. Native and web
use the same ROMs. Replay compares complete state, messages and pixels.

This is a local milestone candidate. Automated completion cannot establish
five-minute pacing, fair difficulty or whether the mystery is enjoyable;
those require a fresh player. No public deployment is implied.
