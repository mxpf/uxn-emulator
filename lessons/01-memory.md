# Put one byte in memory

Before starting, read [Set up the workshop](00-workshop.md). It explains the
project folder, terminal, compiler, and Make. We will use all four here.

## What we are doing

We are going to place one byte into one memory location and read it back.

This is our first small piece of the emulator. We are not running Uxn
instructions yet.

## Why we are doing it

A program needs somewhere to keep its instructions and values. That place is
memory.

Think of memory as a long row of numbered boxes. Each box holds one byte. The
number on a box is its address.

Uxn has 65,536 boxes. Their addresses run from `0x0000` through `0xffff`.

## Step 1: open the example

Open `lessons/01-memory.c`.

We are looking at a separate learning example, not changing the working
emulator. This lets us study memory without stacks, instructions, or devices
getting in the way.

## Step 2: find the memory

Find this line:

~~~c
uint8_t memory[MEMORY_SIZE] = {0};
~~~

This creates the row of memory boxes.

- `uint8_t` means each box holds an eight-bit whole number: one byte.
- `MEMORY_SIZE` is 65,536, so the row contains 65,536 boxes.
- `{0}` starts every box with the value zero.

We do this first because a byte needs a place to live before we can store it.

## Step 3: find the stored byte

Find this line:

~~~c
memory[ROM_START] = 0x48;
~~~

`ROM_START` is the name we gave address `0x0100`. Later, a ROM—the file holding
a Uxn program’s bytes—will be copied into memory starting here. Uxn uses this
agreed starting place so the first 256 boxes remain available for small pieces
of data a program uses often.

The square brackets choose that one address. The equals sign puts `0x48` there.

ASCII uses the number `0x48` for the letter H. Memory does not know it is a
letter. It only stores the byte.

## Step 4: run the example

From the project folder, run:

~~~sh
make lesson-memory
~~~

`make` asks the C compiler to turn the example into a runnable program. It then
runs that program for us.

You should see:

~~~text
Address 0x0100 holds the byte 0x48.
As a number, that byte is 72.
As an ASCII character, that byte is H.
~~~

All three lines read the same memory box. Only the way the program displays the
byte changes.

## Step 5: change the byte

In `lessons/01-memory.c`, change:

~~~c
memory[ROM_START] = 0x48;
~~~

to:

~~~c
memory[ROM_START] = 0x41;
~~~

Run `make lesson-memory` again. You should now see `0x41`, `65`, and `A`.

We make this change to prove that the address is a box we control. Changing the
stored byte changes what we read back without changing how the memory is built.

Change `0x41` back to `0x48` when you are finished. The next lesson will use H.

## What we have built

We now have the first part of a computer: memory that can store a byte at a
numbered address and return it later.
