# Our Uxn and Varvara emulator

This project is a small virtual computer written in C. It runs Uxn ROMs and
provides the current Varvara 79K devices: System, Console, Screen, four Audio
voices, Controller, Mouse, two File devices, and DateTime.

The code is split into small files so each part can be read and tested without
opening one very large source file.

## Build it on a Mac

The command-line runner only needs a C compiler and Make. The windowed runner
also needs SDL2, a library that opens the window and connects sound and input.

~~~sh
brew install sdl2 pkg-config
make
~~~

The build creates two executables:

- `bin/uxncli` runs text-based ROMs in the terminal.
- `bin/uxnemu` runs graphical and audio ROMs in a native window.

## Build and run the included greeting

~~~sh
make example
~~~

This downloads Drifblim, the official assembler written in Uxntal. Our
command-line emulator runs Drifblim to turn `examples/hello.tal` into a ROM,
then runs that ROM. The result should be:

~~~text
Hello from our Uxn!
~~~

This also proves that the File device works: the assembler is itself a Uxn ROM
that reads the source file and writes the new ROM file.

## Run a ROM

~~~sh
bin/uxnemu program.rom
bin/uxncli program.rom
~~~

Use `--scale 2` or the shorter `-2` to start the window at twice its normal
size. Use `-f` to start fullscreen.

You can also drop another `.rom` file onto the open window. The emulator
restarts with that ROM. If the ROM supplies Varvara metadata, its name appears
in the window title; otherwise the filename is used. These are window
conveniences only. ROMs do not need to know they exist.

The window controls match the reference emulator:

- Arrow keys: direction buttons
- Control, Option, Shift, and Home: A, B, Select, and Start
- F1: change window scale
- F2: print both Uxn stacks
- F3: quit
- F4: hard restart
- F5: soft restart, preserving Uxn's first 256 memory bytes
- F11: toggle fullscreen
- F12: toggle the window border

Text input, mouse input, scrolling, terminal input, and standard SDL game
controllers are also connected to their Varvara devices.

## File access

A ROM can ask Varvara to read or write files. By default, this emulator keeps
those requests inside the folder where it was launched and refuses paths that
escape through `..`, an absolute path, or a symbolic link.

For a trusted ROM that needs the reference emulator's unrestricted file
behavior, opt in explicitly:

~~~sh
bin/uxnemu --allow-filesystem program.rom
bin/uxncli --allow-filesystem program.rom
~~~

## Host commands

Some Uxn programs use the optional Console `exec` ports to run a command on the
host computer. This can do anything your user account can do, so it is denied
unless you give a trusted ROM explicit permission:

~~~sh
bin/uxnemu --allow-exec program.rom
bin/uxncli --allow-exec program.rom
~~~

This extension is separate from the Uxn processor. On hosts where it is not
available, the ROM receives a failed command result.

## Check it

Build both runners and run the checks that need no internet connection:

~~~sh
make check
~~~

This includes 200 processor and device checks plus a short headless window
run. `make test` runs only the 200 checks.

Run the current official Uxn and Varvara tests when internet access is
available:

~~~sh
make compatibility
~~~

The compatibility check downloads the official self-hosted assembler and test
sources into a temporary folder. It checks every opcode, wraparound behavior,
Console and File behavior, and the Screen output pixel by pixel.

Run the current Hundred Rabbits applications as a wider check:

~~~sh
make real-roms
~~~

This downloads the latest source for Dexe, Left, Nebu, Noodle, Nasu, Oekaki,
Turye, and M/PC. Our emulator assembles each application, opens it without a
visible window, and checks that it draws a nonblank screen. It also types and
saves text in Left, draws and saves an icon in Noodle, and plays a note in
CCCC. Left and Dexe are also asked to open real files.

Run both internet-based suites together with `make verify-online`.

## Project map

- `src/uxn.c` is the virtual processor.
- `src/varvara.c` connects the processor to devices.
- `src/varvara_screen.c`, `src/varvara_audio.c`, `src/varvara_file.c`, and
  `src/varvara_datetime.c` implement the larger devices.
- `src/varvara_exec.c` holds the optional host-command extension.
- `src/main.c` is the terminal runner.
- `src/emu.c` is the SDL2 windowed runner.
- `tests/test_uxn.c` contains local behavior checks.
- `tests/compatibility.sh` runs the official compatibility checks.
- `tests/real-roms.sh` builds and opens the current Hundred Rabbits apps.
- `lessons/` contains the first small, runnable recipe examples.

The supporting notes explain the project from three angles:

- [Design promise](DESIGN.md): what we will add and where we stop.
- [User guide](docs/USER-GUIDE.md): everyday commands and permission switches.
- [Architecture](docs/ARCHITECTURE.md): how a byte moves through the program.
- [Compatibility](docs/COMPATIBILITY.md): what has actually been checked.

The full beginner recipe is published as
[A Computer Small Enough to Understand](https://keeping.haus/a-small-computer-we-can-understand/).

## References

- [Uxn](https://wiki.xxiivv.com/site/uxn.html)
- [Uxntal](https://wiki.xxiivv.com/site/uxntal.html)
- [Varvara](https://wiki.xxiivv.com/site/varvara.html)
- [Hundred Rabbits on Uxn](https://100r.ca/site/uxn.html)
