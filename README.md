# Uxn Emulator and Constellation

This is a small [Uxn](https://wiki.xxiivv.com/site/uxn.html) and
[Varvara](https://wiki.xxiivv.com/site/varvara.html) emulator written in C.
I built it to learn how a virtual computer turns bytes in a file into words,
pictures, and sound. The processor, screen, audio, files, and window each live
in their own C files so you can follow what happens one piece at a time.

Constellation uses this emulator to explore how much a small virtual machine
can do. Some cartridges run one Uxn program. Others let several programs pass
messages to each other. The experiments compare what each approach gives the
person making or playing the game. [Project design](DESIGN.md#constellations-north-star)

## Play in your browser

[Playinghaus](https://playing.haus/) has three cartridges you can open now.

| Cartridge | What you do | Run on macOS |
| --- | --- | --- |
| [Tiny Neighbors](https://playing.haus/tiny-neighbors/) | Walk around a garden and greet a creature. | `make garden` |
| [No Escape!](https://playing.haus/no-escape/) | Move a square with the arrow keys and replay your moves. | `make square` |
| [Sketchpad](https://playing.haus/sketchpad/) | Draw, erase, save, and reopen a small pixel drawing. | `make sketchpad` |

The [Tiny Neighbors](docs/TINY-NEIGHBORS.md), [No Escape!](docs/SQUARE-DEMO.md),
and [Sketchpad guides](docs/SKETCHPAD.md) cover the controls and how each
cartridge works.

## Build the emulator

Get the branch that includes Constellation and the Playinghaus source.

```sh
git clone --branch constellation-game https://github.com/mxpf/uxn-emulator.git
cd uxn-emulator
```

The terminal program needs a C compiler and Make. The windowed program also
needs SDL2. The checks and ROM builds use `xxd`.

### macOS

With Xcode Command Line Tools and Homebrew installed, build and check both runners.

```sh
brew install sdl2 pkg-config
make check
```

### Windows 11

The emulator has been built and tested in the [MSYS2 UCRT64 shell](https://www.msys2.org/docs/environments/)
using MSYS2's POSIX GCC and SDL2. Install the packages below and put `xxd` on
your path. The `export` line uses the default Git for Windows install location.

```sh
pacman -S --needed gcc make mingw-w64-ucrt-x86_64-SDL2
export PATH="$PATH:/c/Program Files/Git/usr/bin"
make check \
  CC='/usr/bin/gcc -Wno-error=enum-compare -Wno-error=restrict' \
  SDL_CFLAGS='-I/ucrt64/include/SDL2 -DSDL_beginthread=0 -DSDL_endthread=0' \
  SDL_LIBS='-L/ucrt64/lib -lSDL2'
```

`make check` builds both programs, runs the emulator and Constellation tests,
and saves a picture from a small test ROM without opening a window. On Windows
11, that ROM has also been checked in a visible emulator window.

## Run a ROM

Use `bin/uxncli program.rom` for terminal programs or `bin/uxnemu program.rom`
for graphics, sound, and keyboard, mouse, or controller input. You can also drop
a `.rom` file onto the window. On Windows, the executables have an `.exe` suffix.

For a first example, run `make example`. The first run downloads the Drifblim
assembler. The emulator uses it to turn `examples/hello.tal` into a ROM, then
runs that ROM to print `Hello from our Uxn!`.

By default, a ROM can read and write files inside the folder where you started
the emulator. Access outside that folder and permission to run host commands
are separate options for trusted ROMs. Both reset when you close the program.
[Emulator controls and permissions](docs/USER-GUIDE.md)

## Build Playinghaus locally

With Emscripten 4.0.15 active, build the three browser cartridges and serve the
output with Python 3.

```sh
make playing-web
python3 -m http.server 8766 --bind 127.0.0.1 --directory build/web
```

Open [the local Playinghaus preview](http://127.0.0.1:8766/). If your shell
uses `python` for Python 3, use that in the server command. The
[release guide](docs/PUBLISHING.md) lists the native, WebAssembly, and browser
checks for the full collection.

## Read further

- [Memory lesson](lessons/01-memory.md) starts with a small C program you can
  read and run. [Emulator architecture](docs/ARCHITECTURE.md) follows a byte
  through the emulator.
- [Compatibility record](docs/COMPATIBILITY.md) lists what has been tested.
  Run `make verify-online` to fetch the upstream tests and try the emulator
  with Hundred Rabbits programs.
- [Constellation v0](docs/CONSTELLATION-V0.md), [routing experiments](docs/CONSTELLATION-ROUTES.md),
  and the [shared runner](docs/CONSTELLATION-RUNNER.md) explain how ROMs send
  messages, take turns, and replay recorded inputs.
- [The Last Visitor](docs/MYSTERY-OPENING.md) is a locked-room mystery built to
  compare how one or several ROMs handle clues and deductions. On macOS,
  `make mystery` opens the local version. Try `make mystery-single` for the
  one-ROM version or `make account-check` for the separate
  [witness account study](docs/ACCOUNT-BOUNDARY.md). These experiments run
  locally, outside the Playinghaus collection.

## Acknowledgements

Thanks to [Hundred Rabbits](https://100r.ca/site/uxn.html) and the wider Uxn
community for Uxn, Varvara, Uxntal, Drifblim, and the programs used in the
compatibility checks. This is an independent implementation.

The source and documentation are available under the [MIT License](LICENSE).
