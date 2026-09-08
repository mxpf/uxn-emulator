# Tiny Neighbors

A small playable garden built from two cooperating Uxn ROMs. Walk toward the
golden creature and greet it. The right-hand panel shows the actions and world
snapshots that make the encounter happen.

## Play

From this checkout, run:

```sh
make garden
```

The host needs a C compiler, Make, SDL2 and pkg-config, as the windowed emulator
does. Drifblim assembles both ROMs; its first download needs a connection.

- Arrow keys or WASD: move. Holding a direction moves once per tick.
- Space: greet when close (including diagonally adjacent).
- P: pause.
- N while paused: advance one tick, using any pending direction or greeting.
- Escape or close the window: quit.

The coral figure is the player. Trees and the pond block movement. The creature
walks a small four-position patrol, stops when nearby, and responds with a heart.
Greeting it establishes friendship; it then stays in place. This is a single
encounter, with no inventory, combat, saving a world, or larger game progression.

## Two ROMs, distinct responsibilities

`examples/garden-world.tal` owns the map, collisions, player position, creature
patrol, proximity checks and friendship. `examples/garden-view.tal` translates
host input into a one-byte message and draws the world snapshot it receives.
The host implements an eight-by-eight sprite device and a logical input clock.
Terrain, characters and heart bitmaps are defined in the view ROM.

The world boots as endpoint B and sends its initial state. Endpoint A, the
view, boots its receive vector; the scheduler delivers the state to it.
For each subsequent tick:

1. The host invokes the view's input vector with one action.
2. The view sends that action to the world through the existing Port device.
3. The world advances the simulation once and sends a 200-byte snapshot.
4. The view redraws from that snapshot.
5. The host drains the exchange to quiescence before accepting the next tick.

Normal play produces four logical ticks per second. A slow host slows play;
there is no catch-up burst. Neither ROM reads the clock. Paused stepping and
replay execute the same tick function. Evaluations have a 100,000-instruction
ceiling; a tick has a 32-turn ceiling. Errors are terminal for this game session.
The existing Uxn processor, Constellation module and standard runners are
unchanged. The game is a separate experimental host, not a Varvara ROM.

## Message and device contracts

View to world: one action byte, 0 wait, 1 north, 2 east, 3 south, 4 west, 5 greet.

World to view: 200 bytes copied to 0400:

| Offset | Value |
| --- | --- |
| 0–1 | Player tile x, y |
| 2–3 | Creature tile x, y |
| 4 | Mood: 0 wandering, 1 curious, 2 friend |
| 5 | Friendship flag |
| 6 | Tick modulo 256 |
| 7 | Reserved zero |
| 8–199 | 16×12 terrain: 0 grass, 1 tree, 2 water, 3 flower |

Only the view is connected to the host sprite device:

| Device port | Meaning |
| --- | --- |
| 20, 21 | Tile x, y |
| 22–23 | Big-endian address of eight sprite bytes |
| 24 | Palette index 0–5; bit 80 enables transparent background |
| 25 | Nonzero write draws the sprite |
| e0–e1 | Host-input vector |
| e2 | Current action |

These device addresses are local to this host. The existing d0–db message
interface remains intact. The host never passes pointers between ROMs.

## Inspect and replay

The side panel retains recent input events and abbreviated message payloads:
P means player position, C creature position. Pause to read it or single-step
to watch a response. The full v0 trace remains available in memory for the
latest tick. The game hashes each consumed batch before reusing that bounded
buffer; trace overflow within one exchange remains an error.

```sh
bin/garden --record build/walk.txt
bin/garden --replay build/walk.txt
```

Recording refuses to overwrite an existing file. Records contain the initial
ROM/trace fingerprint, then an action and cumulative trace fingerprint per tick.
Replay checks every recorded fingerprint and exits nonzero on divergence.
Replay runs quickly in a hidden window; add `--screenshot build/replayed.bmp`
to inspect its final frame. A recording prefix is a valid shorter session.
The checksum is a diagnostic, not a cryptographic guarantee.

For a reproducible encounter without a keyboard:

```sh
bin/garden --script EEEEESSSSG --screenshot build/friend.bmp
```

Script letters are N/E/S/W, G for greet, and a dot for wait.

## Verification

`make garden-check` runs the real assembled ROMs through collision, wandering,
proximity and greeting tests. It also compares two 1,000-tick runs for identical
traces, RAM, device bytes and pixels, checking empty stacks after each tick.
The SDL host then renders the scripted encounter to `build/garden.bmp`.
