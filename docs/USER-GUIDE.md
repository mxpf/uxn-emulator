# Using the emulator

## Use the Mac app

Open the downloaded `.dmg`, then drag **Uxn Emulator** onto its
**Applications** shortcut. Open the app from Applications and drop a `.rom`
file onto its window. The app waits for a ROM instead of closing when it starts
without one.

The app contains the same `uxnemu` program produced by `make`. It also contains
its SDL2 library, so SDL2 does not need to be installed separately on the Mac
that runs the finished app. The source build remains available beside it.

To make and verify the disk image from source, run `make dmg-check`. The file
is written to `dist/Uxn-Emulator-macOS-<arch>.dmg`. The existing ZIP can still
be made with `make package`.

## Start a ROM

Use the windowed runner for graphics or sound:

~~~sh
bin/uxnemu program.rom
~~~

Use the terminal runner for text programs and command-line tools:

~~~sh
bin/uxncli program.rom
~~~

A ROM is the Uxn program file. Starting either command creates a fresh virtual
machine, loads the ROM at memory address `0x0100`, and runs its reset code.

## Adjust the window

Start at double size with `-2` or `--scale 2`. Start fullscreen with `-f`.
While the window is open, F1 changes scale, F3 quits, F4 starts the ROM from a
clean bank-zero memory, F5 preserves its first 256 bytes while restarting, F11
changes fullscreen, and F12 changes the border.

Drop a different `.rom` file onto the window to open it in the same emulator.
The old ROM is restored if the new file cannot be loaded.

## Choose permissions

Normal file reads and writes stay inside the folder where you started the
emulator. A trusted ROM can receive unrestricted file paths with:

~~~sh
bin/uxnemu --allow-filesystem program.rom
~~~

Some programs use the optional Console `exec` ports to start host commands.
Those commands have the same access as your user account. Enable them only for
a ROM you trust:

~~~sh
bin/uxnemu --allow-exec program.rom
~~~

The two permissions are independent and are never saved. A ROM that needs both
must be started with both options.

## Check a change

Run `make check` for the offline build and test suite. Run
`make verify-online` when you also want to compare against current official
sources and current Hundred Rabbits applications. See
[the compatibility record](COMPATIBILITY.md) for the exact coverage.
