# Uxn Emulator

This is a small [Uxn](https://wiki.xxiivv.com/site/uxn.html) and
[Varvara](https://wiki.xxiivv.com/site/varvara.html) emulator written in C.
I built it to learn how a virtual computer works by following the whole path
from bytes in a file to words, pictures, and sound.

The project is intentionally readable. The processor, screen, audio, files,
and desktop window live in separate C files, and the documentation explains
why each part exists. It is a learning project, but it is also a working host
for Uxn ROMs.

**[Read the complete beginner tutorial](https://keeping.haus/a-small-computer-we-can-understand/)**

## What it can do

The emulator provides the current Varvara 79K devices: System, Console,
Screen, four Audio voices, Controller, Mouse, two File devices, and DateTime.

It builds two programs:

- `uxncli` runs text programs and tools in the terminal.
- `uxnemu` runs programs with a window, sound, keyboard, mouse, and game
  controller input.

The window can also open a ROM by drag and drop. If the ROM includes a name in
its Varvara metadata, that name appears in the title bar.

## Start locally

The project has been built and tested on macOS. The command-line host needs a
C compiler and Make. The windowed host also needs SDL2.

```bash
brew install sdl2 pkg-config
git clone https://github.com/mxpf/uxn-emulator.git
cd uxn-emulator
make
```

Run a graphical or audio ROM:

```bash
bin/uxnemu program.rom
```

Run a terminal ROM:

```bash
bin/uxncli program.rom
```

To see the whole path working, build and run the included greeting:

```bash
make example
```

This runs the Drifblim assembler inside the emulator, turns readable Uxntal
into a ROM, and then runs that ROM. The result is:

```text
Hello from our Uxn!
```

## A small safety boundary

ROMs can ask Varvara to use files or start commands on the host computer. File
access stays inside the folder where the emulator was started. A trusted ROM
can receive wider access for one run:

```bash
bin/uxnemu --allow-filesystem program.rom
bin/uxnemu --allow-exec program.rom
```

The permissions are separate, off by default, and never remembered.

## Check the work

Run the complete offline build and test suite:

```bash
make check
```

This performs 200 processor and device checks, then runs a small graphical ROM
without opening a visible window.

When an internet connection is available, compare the emulator with current
official tests and real Hundred Rabbits programs:

```bash
make verify-online
```

The current implementation passes the official opcode and device checks. It
has also assembled and opened all eight applications listed in the Hundred
Rabbits [ROM collection](https://wiki.xxiivv.com/site/roms.html), with added
checks for typing, drawing, saving files, and producing sound.

## Read further

Build its browser version with `make garden-web-check` (Emscripten required),
then serve `build/web/`. See [browser instructions and portability contract](docs/TINY-NEIGHBORS.md#browser-host).

Try the first Constellation game with `make garden`: a two-ROM garden with a
wandering creature, a live message panel, and recorded input replay. See
[Tiny Neighbors](docs/TINY-NEIGHBORS.md) for controls and the protocol.
Its [measured baseline](docs/GARDEN-BASELINE.md) records cross-host tests,
resource use, and what the two-ROM prototype does and does not prove.

Try the separate multi-ROM [routing experiment](docs/CONSTELLATION-ROUTES.md)
with `make constellation-routes`: three unchanged Uxn ROMs, cartridge-declared
routes, independent bounded queues, and full-state deterministic replay checks.
It does not replace v0 or impose game-specific roles.
`make constellation-sustained` checks 1,024 acknowledged cycles, repeated queue
recovery, and exact replay with different bounded trace-consumption intervals.

Try `make square` for a minimal interactive demo on that routed host: arrow
keys move a square through three ROMs, `R` replays the recorded session, and
`N` starts over. See [Square demo](docs/SQUARE-DEMO.md); `make square-check`
checks movement, pixels, window controls, and exact replay.

- [Design promise](DESIGN.md) explains what belongs in the project and where
  it stops.
- [User guide](docs/USER-GUIDE.md) lists everyday commands and controls.
- [Architecture](docs/ARCHITECTURE.md) follows a byte through the emulator.
- [Compatibility](docs/COMPATIBILITY.md) records exactly what has been tested.
- [Constellation v0](docs/CONSTELLATION-V0.md) documents the isolated two-ROM
  message-passing prototype.
- [Why We Built a Small Virtual Computer](https://keeping.haus/why-we-built-a-small-virtual-computer/)
  explains the larger idea behind the project.

## Acknowledgements

Uxn, Varvara, Uxntal, Drifblim, and the programs used for compatibility checks
come from [Hundred Rabbits](https://100r.ca/site/uxn.html) and the wider Uxn
community. This is an independent implementation, not an official Hundred
Rabbits release.

## License

The emulator source and its documentation are available under the
[MIT License](LICENSE).
