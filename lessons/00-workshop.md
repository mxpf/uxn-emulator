# Set up the workshop

## What we are doing

Before building the emulator, we need to know where our work happens and what
turns our writing into a running program.

That collection of tools is called a **development environment**. We already
have one working on this Mac.

## The project folder

All of our files live in one folder:

~~~text
~/Projects/uxn-emulator
~~~

This is the project folder. Keeping everything together lets our tools find
the code, build instructions, lessons, and tests.

## Codex

Codex is the application we are using to work on the project. It lets us read
and edit files, talk through decisions, and send commands to the computer.

Codex helps with the work, but it is not the program that runs C code.

## The terminal

The **terminal** is a text window for giving the computer commands. It also
shows text produced by our programs.

Commands run from a particular folder. Ours should run from the project folder
shown above so they can find the correct files.

This command shows the current folder:

~~~sh
pwd
~~~

Here it should print:

~~~text
~/Projects/uxn-emulator
~~~

## C and the compiler

We are writing the emulator in **C**, a programming language that lets us work
directly with bytes and memory without needing a large framework.

A C file is still human-readable text. A **compiler** translates that text into
an executable: a file the computer can run.

This Mac already has Apple’s Clang C compiler. The short command `cc` starts
it.

## Make

Compiling a program often requires a long command. **Make** is a small tool
that remembers that command for us.

Make reads this project’s `Makefile`. When we run:

~~~sh
make lesson-memory
~~~

Make follows three steps:

1. Ask the C compiler to translate `lessons/01-memory.c`.
2. Save the executable as `build/lesson-memory`.
3. Run that executable in the terminal.

The `build` folder holds generated files. We can recreate them from the C
source, so the source remains the part we edit and keep.

## Why start this way

We do not need a website, a server, or a large development framework yet. A C
file, a compiler, Make, and a terminal give us the shortest visible path from
code we can read to a program we can run.

Our first program will only create memory, store one byte, and read it back.
That is enough to prove the workshop is ready.

Next: [put one byte in memory](01-memory.md).
