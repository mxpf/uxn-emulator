# Design promise

The current Uxn emulator has two jobs: run Uxn programs and remain small enough
to learn from. A feature that weakens either job needs a very good reason to exist.

## Constellation's north star

**Ambitious software, small permanent machine.**

Constellation aims to become the best ambitious, durable game system we can
make while preserving the qualities exemplified by Uxn. Narrative investigation
is the current leading creative proving ground, not a restriction on future
game genres. Uxn remains the current foundation and preferred starting point:
no experiment has demonstrated a Uxn-level limitation requiring change.

The permanent execution contract should be small enough to understand and
independently reimplement, completely and publicly specifiable, and operationally
testable. Preserve deterministic replay and deterministic execution wherever
game-visible behavior depends on it, with bounded, explicit memory, input,
resources and communication. Self-contained cartridges should run without
required accounts, cloud APIs, package registries,
proprietary services or runtime network dependencies. Preserve portability by
reimplementing the small machine/runtime, not repeatedly porting software through
changing frameworks. Keep most complexity in software and authored content,
rather than accumulating it indefinitely in the permanent machine.

One machine is a valid design; no general single-ROM default has been adopted.
Multiple machines are an available technique, not the project's identity or a
required destination. A boundary must earn its cost through demonstrated
application value. Private state, conceptual separation and implementation
convenience alone are insufficient. Preserve the routing, scheduling, bounded
queues, external-input and replay experiments as tested infrastructure; their
existence does not oblige applications to use them.

Room, character and deduction rules belong to cartridges, never the generic
host. Repository experiment reports, including
[the mystery architecture comparison](docs/CARTRIDGE-COMPARISON.md), are
implementation evidence and history, not standing instructions for future work.
Constellation — Project Home selects future work using the authoritative durable
direction in kDrive:
`/Private/03 Projects/Uxn Emulator/Constellation Narrative Game System Design.md`.
This file is its concise implementation-facing companion, not a separate roadmap.

## Current Uxn compatibility contract

The Uxn processor gives every ROM the same instructions, stacks, and memory.
Varvara gives it the standard screen, sound, input, file, console, and clock
devices. Current work keeps Uxn unchanged: no added opcodes, enlarged addresses,
or silent changes to ordinary ROM behavior. That fixed contract lets the same
ROM travel between emulators. Evaluating a hypothetical successor is separate
from breaking this existing Uxn/Varvara compatibility contract.

The host layer is allowed to translate those standard ideas into a real window,
speakers, keyboard, mouse, and files. Optional conveniences belong here, where
they cannot leak into the virtual machine.

## Classify limitations before expanding the contract

Before expanding the permanent execution contract, determine where a demonstrated
game limitation belongs:

1. Cartridge/application
2. Authoring/tooling
3. Constellation runtime or host
4. Varvara/device model
5. Uxn itself

Only the fifth category warrants evaluating changes to or replacement of Uxn,
and only if actual game development demonstrates a fundamental and recurring
Uxn-level limitation. Implementation inconvenience alone is insufficient.
Unchanged Uxn is the current rule, not an eternal prohibition on reconsideration.
Any extension or small successor execution contract would have to preserve the
durability principles above. None is selected or designed here, and the evidence
does not establish that Uxn has been outgrown.

## The four enhancement lanes

### 1. Stronger testing

Keep it. It is a development aid, not part of the computer a ROM sees. Local
checks run offline. Separate online checks compare our work with current
official sources and real Hundred Rabbits programs. Either suite can be skipped
when its kind of checking is not needed.

### 2. Exact audio checking

Keep it as a test. Audio itself is standard Varvara behavior, so there should
not be a switch for less accurate sound. The extra check compares known sample
values with the reference renderer and makes a subtle mistake easier to find.

### 3. Host commands

Keep the Console `exec` extension, but deny it by default. A trusted ROM can be
given `--allow-exec`. The code lives in its own file and a host that cannot
support it can report failure without changing Uxn. Permission must never be
remembered silently.

### 4. Desktop conveniences

Keep the two thin ones: dropping a ROM onto the window and showing the ROM's
metadata in the title. They translate ordinary desktop actions at the outer
edge. They are optional in the useful sense: another host can leave them out,
and every ROM still behaves the same.

Do not add a ROM library, automatic updater, account, telemetry, network
service, or a permanent preferences system. A native application bundle may be
useful later, but packaging must remain separate from the emulator.

## Other limits

- File access stays inside the launch folder unless a trusted ROM receives
  `--allow-filesystem`.
- The command-line runner stays independent of SDL2. SDL2 is used only for the
  window, sound, and physical input.
- Drifblim remains a ROM run by the emulator, not a second assembler copied
  into the codebase.
- A compatibility claim names the test behind it.
- New parts should be readable in one sitting and documented beside the code.

Optional features still cost code, tests, and explanation. “Optional” is not a
reason to collect every possible feature.

## Sources behind these choices

Hundred Rabbits describe Uxn as intentionally small and encourage independent
reimplementations rather than dependence on one privileged host. Their
[Uxn devlog](https://wiki.xxiivv.com/site/uxn_devlog.html) explains the frozen
specification, redundant implementations, low dependencies, and resistance to
bit rot. [Varvara](https://wiki.xxiivv.com/site/varvara.html) keeps devices
outside Uxn and permits hosts to use or ignore ROM metadata.

Their notes on
[collapse computing](https://wiki.xxiivv.com/site/collapse_computing.html)
favor software that works on existing hardware and is modular, sturdy,
repairable, and well documented. The
[Hundred Rabbits project](https://wiki.xxiivv.com/site/hundred_rabbits.html)
also gives the practical setting: tools made for limited power, intermittent
connections, and long use away from easy replacement.

This project follows those ideas in its own way. It is not an official Hundred
Rabbits emulator, and the links above are references rather than an endorsement.
