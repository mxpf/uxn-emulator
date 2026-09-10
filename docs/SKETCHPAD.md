# Sketchpad: a non-game application

A 32 × 24 sheet of pixels. Arrows move a cursor, Space toggles Draw, and X
toggles Erase. Browser buttons provide the same six operations. Draw and Erase
are mutually exclusive: turning either on applies it to the current cell and
keeps applying it as the cursor moves. Pressing the active tool again returns
to move-only mode without changing the paper. Both tools start off. Each press
is one operation; holding a key does not repeat. Save downloads a `.sketch`
document; Open replaces the sheet with a previously saved drawing. There is
no autosave, undo history, animation clock or in-app recorder. Closing/reloading
loses unsaved changes. The amber cursor is an overlay, not part of the drawing.

## Run locally

From the repository root:

```sh
make sketchpad
```

For the browser, with Emscripten available:

```sh
make playing-web
python3 -m http.server 8766 --bind 127.0.0.1 --directory build/web
```

Open `http://127.0.0.1:8766/sketchpad/`, or select the third cartridge on the
homepage. The Back to Playinghaus link returns to that homepage. `sketch-web`
alone rebuilds the drawing tool; `playing-web` builds the complete collection.
There are no network dependencies in the page beyond its locally served files.

## Architectural experiment

No Escape! is a three-node input/world/drawing pipeline. Sketchpad uses one
355-byte ROM, one explicitly declared external input route (selector 07), an
explicitly attached display callback and a bounded document input attachment.
There are no inter-node messages
in this application. The runner, routed host and Uxn processor are unchanged.
This tests independence from the demo's roles, not additional concurrency.

The ROM owns cursor bounds, the active tool and the drawing. It stores cursor
x/y at zero-page 00/01, mode at 02 (0 move, 1 draw, 2 erase), and 768 paper
cells at 1000–12ff, one byte per cell (0 paper, 1 ink).
It validates the external message source and length before interpreting
action bytes 1–6; action 7 restores an explicitly staged, validated document.
Neither C nor JavaScript writes this guest state to draw or reopen a sheet.

The app-specific display snapshot is:

| Port | Meaning |
| --- | --- |
| 40–41 | Big-endian address of 768 paper bytes in bank zero |
| 42 | Cursor column, 0–31 |
| 43 | Cursor row, 0–23 |
| 44 | Write 1 to present; 0 does nothing |
| 45 | Current mode: 0 move, 1 draw, 2 erase |

Only port 44 has a callback. The other registers retain normal device-byte
storage. The host checks the complete snapshot before painting: address span,
cursor bounds, mode, cell values and command must all be valid. It then presents
each cell at 4 × 4 pixels, with an amber border around the cursor. The palette,
scale and this snapshot protocol belong to Sketchpad, not to the runner.
Browser ON/OFF indicators and the native window title reflect this committed
mode; JavaScript does not maintain a separate tool state or synthesize marks.

This is **not the Varvara screen ABI**. These ROM bytes run unchanged in the
native and browser versions of this application profile; they are not claimed
to work in an arbitrary Uxn host without these attachments.

## Save and reopen

In the browser, **Save** downloads `drawing.sketch`; **Open** uses the system
file picker. Nothing is uploaded to a server or saved to browser storage.
Opening replaces the current drawing, cursor and tool. Save first if you want
to keep the current sheet. Cancelling the picker does nothing. Controls pause
while a selected file is being read, and recover after a read/validation error.

Natively, drop a `.sketch` file onto the window to open it. Press **S** to
create `sketchpad.sketch` in the current directory. Existing files are never
overwritten; move/rename that file before saving again, or supply a new path:

```sh
./bin/sketchpad --open drawing.sketch --save another.sketch
```

`--save` chooses the destination for S. In the headless script interface it
saves after the script completes, including an empty script for a round trip:

```sh
SDL_VIDEODRIVER=dummy ./bin/sketchpad --open drawing.sketch --script '' --save copy.sketch
```

The [784-byte document format](SKETCHPAD-DOCUMENT.md) belongs to Sketchpad.
It is not a machine snapshot, portable cartridge format, generic storage ABI
or permission to access files from a ROM. Only the native/browser shell opens
user-selected paths; the ROM receives a bounded read-only data stream.

## Scheduling and evidence

The UI admits one action and executes one routed turn synchronously. With one
node and no internal routes, that completes the operation before another
press. It never grows a JavaScript queue or secretly retries rejected input.
The lower-level input API still exposes the unchanged bounded queue, including
FULL. There is no frame-driven guest work while idle.

Pending traces are consumed after boot, input and each turn. The application
retains a bounded rolling fingerprint and counters for diagnostics, not an
ever-growing archive. Replay lives only in the test harness at this stage.

```sh
make sketch-check
make sketch-web-check
node tests/sketch_browser_check.cjs http://127.0.0.1:8766/sketchpad/
node tests/sketch_document_check.cjs http://127.0.0.1:8766/sketchpad/
```

- Native tests compare every paper cell and every rendered pixel with an
  independent reference model. A fixed 382-turn script, including idle gaps,
  also compares two fresh instances' full allocated guest memory, stacks,
  device bytes, queues, scheduling state and display; trace fingerprints match.
- Additional checks cover all edges, draw/erase trails, switching tools,
  toggling back to move-only mode without changing paper, invalid input,
  raw queue saturation, 3,000 consecutive UI actions, malformed messages,
  malformed display snapshots, startup failures and refusal to replace a live
  instance. The SDL shell also runs a hidden-window rendering smoke test.
- 441 native/WebAssembly checkpoints compare canonical fingerprints of full
  guest state, traces and pixels. These are fingerprints, not a collision-free
  byte-for-byte cross-platform proof.
- Real-browser checks cover six buttons, keyboard input, focused-button
  Space/Enter behavior, ignored key repeats, tactile depression, eleven viewport
  widths, return navigation and 287 full-pixel and toggle-state reference comparisons.
- The native sketch suite also passes address/undefined-behavior sanitizers.
- Document tests cover every truncated length, an invalid value at every byte,
  extra bytes, all tools/cursor corners, full-state repeated imports, pending
  input refusal and exact file round trips. Browser tests download real files,
  reopen native files, compare every displayed pixel and all file bytes, reject
  malformed files without changing the sheet, and reopen browser files natively.

The result establishes a second, non-game application without adding rules to
the shared runner. It does not establish a general drawing API, portable
cartridge packaging, universal replay, or a standard device library.
