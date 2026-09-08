# Design promise

This emulator has two jobs: run Uxn programs and remain small enough to learn
from. A feature that weakens either job needs a very good reason to exist.

## Keep the center fixed

The Uxn processor gives every ROM the same instructions, stacks, and memory.
Varvara gives it the standard screen, sound, input, file, console, and clock
devices. We do not add opcodes, enlarge addresses, or quietly change what a
ROM means. That fixed center is what lets the same ROM travel between different
emulators.

The host layer is allowed to translate those standard ideas into a real window,
speakers, keyboard, mouse, and files. Optional conveniences belong here, where
they cannot leak into the virtual machine.

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
