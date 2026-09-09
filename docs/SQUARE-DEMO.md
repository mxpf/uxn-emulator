# Square: three ordinary ROMs, one small interactive demo

Run `make square` from the repository root. SDL2 and the existing local Uxn
assembler are the same dependencies used by the other native demos.

- Arrow keys move the square one cell; keyboard repeat is supported.
- `R` freezes the current recording and plays it from a fresh set of machines.
  The title changes to **REPLAY VERIFIED** only when every event, admission
  result, final machine state, and pixel matches. `R` can replay it again.
- `N` discards the in-memory session and starts a new recording.
- `Esc` or closing the window exits. Recordings are not saved to disk.

The recording starts automatically, including idle turns. Live arrow inputs
are ignored during playback. A full input queue rejects that attempt rather
than overwriting an older action; the window title shows the rejection count.
Rejected attempts are part of replay. This tiny adapter has no extra pending
input queue or automatic retry; press again if an unusually large burst fills
the four slots. This is not a polished game or a general controller adapter.

## Application choices, not platform roles

The demo declares three nodes and routes: external input to node 0, node 0 to
node 1, and node 1 to node 2. Their ordinary Uxn ROMs are:

| ROM | Bytes | Demo responsibility |
| --- | ---: | --- |
| `routes-square-input` | 83 | Validate an external arrow action and forward it |
| `routes-square-world` | 207 | Move within a clamped 16-by-12 board and publish coordinates |
| `routes-square-draw` | 81 | Convert coordinates into clear/rectangle commands |

The native shell maps keys to bytes and paces three scheduler turns per visual
update, roughly every 16 ms. Actual submission boundaries and ordering are
recorded; playback is driven by those logical turns, not the wall clock.
Movement rules live in the world ROM. Drawing instructions live in the draw
ROM. The shell's drawing adapter is attached only to this demo's third node.
It provides a 128-by-96, two-color pixel buffer through ports `20`–`25`:
x, y, width, height, palette index, and command (`1` clear, `2` rectangle).
Invalid rectangles or palette indices stop the demo visibly in the terminal.

These display ports and node assignments are **not added to the routed host**.
Uxn and the routed scheduler/input boundary are unchanged. The demo's one-way pipeline cannot fill an internal four-slot queue
under its normal scheduling; a ROM flags an unexpected downstream rejection
instead of silently continuing. These ROMs are not general lossless relays.

## Bounded recording and exact playback

A session has room for 1,024 input attempts, 16,384 expected trace events,
and at most 8,192 scheduled turns. The shell reserves a complete trace batch
before another operation; it freezes before crossing a recording bound.
The title then reads **RECORDING LIMIT**. `R` still replays the complete
recording, and `N` starts over. This is intentionally a short-session demo,
not an unbounded recorder or a portable replay-file format.

Playback creates fresh machines from the same ROM paths after capture ends.
It compares exact event bytes as it goes, then all allocated guest memory,
stacks, device bytes, instruction counts, queues, scheduler positions and
preferences, frame count, and framebuffer pixels. Host pointers are excluded.
Changing ROM files between capture and playback can invalidate verification.
The final boundary may contain queued work: replay restores that exact point,
not an invented quiescent state. There is no hidden extra turn on replay finish.

## Verification

Run `make square-check`. Native checks cover each arrow, clamping at all board
edges, every expected framebuffer pixel, repeated replay, a 780-turn idle
interval, full input rejection, a recording stopped at boundary zero with
queued input, recording-limit freezes, and detection of altered trace,
admission result, and final pixels. SDL event-queue tests exercise the same
dispatcher as the visible window, including arrow repeat, replay, new session,
ignored live input during playback, and quitting.

The headless window check runs:

```sh
SDL_VIDEODRIVER=dummy ./bin/square --script NNEE.SW --verify-replay --screenshot build/square.bmp
```

`N/E/S/W` represent arrows; `.` advances three idle turns. This script finishes
at board position `(9,5)`, with six inputs, 21 turns, 64 events, and verified
replay. Screenshot inspection confirms the orange square on a plain board.
The core demo tests also pass AddressSanitizer and UndefinedBehaviorSanitizer.
Native keyboard hardware and subjective responsiveness still benefit from a
manual play.

## Browser host: No Escape!

The same demo is published as **No Escape!** at `/no-escape/`, below Tiny
Neighbors on the Playinghaus cartridge homepage. The cartridge illustration
shows an inmate in orange; the actual game deliberately remains an orange
square on the 16-by-12 board. This is a small movement/replay demonstration,
not a prison-escape adventure.

Activate Emscripten 4.0.15 and run `make playing-web square-web-check`.
Serve `build/web/` over HTTP(S). `src/square_web.c` compiles the same demo,
routed host, and Uxn core, with all three ROMs embedded unchanged. Its thin
exports handle reset, input, turns, replay, status, and framebuffer access.
JavaScript handles browser input and converts the ARGB pixels to canvas RGBA.
No game rules or new platform roles are introduced.

Arrow keys or the mobile directional buttons move. `P` pauses/resumes,
`R` replays, and `N` starts a new game. Each touch button submits one action
per tap. Keyboard holds use a 250 ms initial delay and at most ten repeats
per second; OS key-repeat events are ignored. A delayed frame produces at
most one repeat, never a catch-up burst. The most recently pressed held
direction wins. Releasing it resumes any previous held direction.

The browser checks external queue capacity before submitting a discrete
press. If full, it advances at most one three-turn scheduler round for this
demo's three-node pipeline, then submits once. This preserves individual taps
without an unbounded JavaScript queue, raw rejection/retry, or any change to
the routed core's four-slot limit. These extra logical turns are recorded
normally. An unexpectedly still-full queue stops visibly rather than dropping
the move. Recording limits still freeze play explicitly.

Pause freezes both scheduler turns and movement input; directional controls
are disabled until Resume. Held state clears
on pause, blur, reset, replay and recording completion. Blur or
switching tabs pauses; resuming never catches up elapsed wall time. Capture
starts automatically and includes idle turns. Recordings are bounded and
in-memory only; leaving the page discards them. Replay disables live movement
and reports verified only after the native exact-state comparison succeeds.

Both games share a full-width, top-aligned 4:3 canvas and a navy/cream
Constellation-inspired control surface. Mobile controls are centered in the
remaining viewport space, with raised directional buttons, press feedback,
and a bottom return link. Desktop shows keyboard instructions instead of the
directional pad. The screen retains its aspect ratio and may extend below a
short/wide desktop viewport; controls remain directly beneath it, reachable
by scrolling. No clipping, stretching, or page-zoom disabling is used.

`make square-web-check` compares 1,137 native/WebAssembly checkpoints across
movement, edge limits, idle gaps, queue rejection, reset, invalid input, and
replay. The canonical digests cover trace data, all guest memory, stacks,
devices, queues, scheduling state, and pixels; they are diagnostics, not
cryptographic proofs. The live-game canvas is also checked in a browser:

```sh
node tests/console_browser_check.cjs http://127.0.0.1:8766/
```

This checks both games at 320, 390, 760, and 1440 pixels wide, screen alignment,
aspect ratio, overflow, touch-target sizes, bottom navigation, and actual
button press/release depth. No Escape! checks cover touch and keyboard
movement, rendered square pixels, replay, reset, and blur/pause. These are
browser-engine/responsive checks, not physical-device Safari testing.

`make square-web-check` also runs the real JavaScript controls against the
real WebAssembly module with controlled event/frame timing. It exercises 60
same-frame taps, 500 OS repeat events, two-second frame stalls, held-direction
changes, exact-state pause during play and replay, reset, and recording-limit
completion. Every scenario must avoid rejected inputs and replay exactly.
The on-page browser suite repeats burst/repeat input with a blocked main
thread, checks the actual final canvas position, and verifies replay.
