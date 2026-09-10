# What we have checked

Compatibility is not a guess that every program will work. It is a list of
tests we can repeat.

Last checked: September 10, 2026.

## Offline checks

Run:

~~~sh
make check
~~~

This builds both runners, performs 411 local checks, opens the included pixel
ROM with SDL2's headless video and audio drivers, and saves a screenshot.

The local checks cover instruction modes, stack and memory wraparound, all
three System expansion commands, System stack and debug registers, Console,
Screen pixels and sprites, selected sample output from all four Audio voices,
a three-voice stereo mix, Controller and Mouse registers, File0 and File1
independence, DateTime, restarts, file limits, and the default denial plus
explicit permission of Console `exec`.

The loader checks include the exact boundary between bank zero and bank one. A
65,280-byte ROM ends at bank-zero address `ffff` without changing bank one. The
next file byte lands at bank-one address `0000`. A separate 65,284-byte fixture
stores `BANK` there, then guest code uses System expansion to copy those four
bytes to bank zero and prints exactly `BANK`.

The other System expansion checks compare exact memory after fill (`00`),
copy-left (`01`), and copy-right (`02`) commands. The two copy checks use
overlapping ranges in their intended directions. Four-byte requests beginning
at address `fffe` stop after two bytes, whether the edge is on the source or
destination side. A fill at `fffe` in the last available bank also stops at
`ffff`; sentinels in adjacent banks remain unchanged. An invalid source bank
leaves the valid destination unchanged, and command `7f` produces the exact
documented error line. Separate register checks set
and read the working- and return-stack pointers and compare the System debug
stack dump byte for byte. These are selected boundary cases, not an exhaustive
set of lengths and bank combinations.

The File1 check uses ports `b0`–`bf` directly. File0 and File1 write different
files while their streams are interleaved, producing the exact contents
`abcdef` and `XYZ123`. Interleaved reads retain separate positions, and File1
reports the exact four-byte status `0006` without resetting its open read
stream.

The Audio checks start Audio0, Audio1, Audio2, and Audio3 through their separate
device ports. Each voice produces the same exact chosen sample values as the
current [uxn2 reference renderer](https://git.sr.ht/~rabbits/uxn2), completes
with its own finished bit, and reports the expected final sample position. A
separate check starts two left-only voices and one right-only voice, confirms
that starting one does not change the others, and compares the exact mixed
left and right samples. These checks do not cover every pitch, envelope, loop,
sample rate, clipping case, or SDL audio-queue behavior.

## Current upstream checks

These download current source files, so they require an internet connection:

~~~sh
make verify-online
~~~

`make compatibility` runs the official opcode, Console, File, Screen, and Audio
test material. The opcode test exercises every instruction and mode. The Screen
result is compared pixel by pixel. On the date above, all of those checks
passed.

`make real-roms` assembles and starts Dexe, Left, Nebu, Noodle, Nasu, Oekaki,
Turye, and M/PC from the current
[Hundred Rabbits ROM collection](https://wiki.xxiivv.com/site/roms.html). It
also checks these interactions:

- Left receives keyboard input and saves a text file.
- Noodle receives mouse input and saves an icon.
- CCCC receives keyboard input and produces non-silent sound.
- Left opens Uxntal text and Dexe opens another ROM as bytes.

All eight applications drew nonblank screens and all 22 real-ROM interaction
checks passed on the date above.

## Optional host behavior

| Behavior | Default | How to enable it |
| --- | --- | --- |
| Files inside the launch folder | On | No option needed |
| Files outside that folder | Off | `--allow-filesystem` |
| Console host commands | Off | `--allow-exec` |
| Drop a ROM onto the window | Available in SDL2 host | Drop a `.rom` file |
| ROM metadata in window title | Available in SDL2 host | ROM supplies metadata |

The last two features do not change the virtual machine. A host may omit them.

## Boundaries

Console `exec` is a host extension, not part of the portable processor. It is
implemented on POSIX hosts and reports failure where the process interface is
unavailable.

Drag and drop is checked during ordinary desktop use rather than by the
headless suite. The online tests follow current upstream files; if upstream
changes, a future failure may describe a new compatibility task rather than a
regression in our last checked version.
