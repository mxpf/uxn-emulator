# How the emulator fits together

A ROM is a file of bytes. The emulator loads those bytes into Uxn memory, reads
one instruction at a time, and lets Varvara connect the virtual computer to the
physical one.

~~~text
ROM file
   |
   v
src/rom.c  ->  src/uxn.c  <->  src/varvara.c
                                  |
                  +---------------+----------------+
                  |               |                |
               screen          audio             files
                  |               |                |
                  +---------- host runner ----------+
                              /          \
                      terminal runner   SDL2 window
~~~

## The center

`src/uxn.c` is the processor. It owns memory, two stacks, and the instruction
loop. It does not open windows or files. When an instruction reads or writes a
device port, it calls the functions attached by the host.

`src/varvara.c` is the switchboard for those ports. Small devices such as
System, Console, Controller, and Mouse are handled there. Larger devices have
their own files:

- `src/varvara_screen.c` stores two drawing layers and a four-color palette.
- `src/varvara_audio.c` turns four voices into stereo samples.
- `src/varvara_file.c` handles two open file or directory streams.
- `src/varvara_datetime.c` reads the host's local clock.
- `src/varvara_exec.c` holds the permission-gated host-command extension.

This split is deliberate. The optional command extension can change without
touching the processor. A new windowing host can reuse the processor and the
device implementations.

## The two hosts

`src/main.c` builds `bin/uxncli`. It connects Console to the terminal and does
not use SDL2.

`src/emu.c` builds `bin/uxnemu`. SDL2 opens the window, queues sound, and turns
keyboard, mouse, controller, and file-drop events into Varvara events. It also
calls the Screen vector about sixty times each second.

## Follow one H

In the greeting ROM, `H` is the byte `0x48`.

1. `src/rom.c` places it in memory.
2. `src/uxn.c` runs the instructions that push it onto the working stack.
3. A `DEO` instruction writes it to Console port `0x18`.
4. `src/varvara.c` routes that port to standard output.
5. The terminal draws `H` using its own font.

The screen path is similar: a ROM writes coordinates and a color to ports;
Varvara changes its two pixel layers; the SDL2 host copies the combined pixels
to the real window.
