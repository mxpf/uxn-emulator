# Sketchpad document v1

This is an application-owned drawing format, not a Constellation platform ABI.
The extension is `.sketch`. The file is exactly 784 bytes with no trailing data.

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 8 | ASCII `SKETCH01` (magic and version) |
| 8 | 1 | Columns, exactly 32 |
| 9 | 1 | Rows, exactly 24 |
| 10 | 1 | Cursor column, 0–31 |
| 11 | 1 | Cursor row, 0–23 |
| 12 | 1 | Tool: 0 move, 1 draw, 2 erase |
| 13 | 3 | Reserved, all zero |
| 16 | 768 | Row-major cells: 0 paper, 1 ink |

There are no pointers, executable bytes, paths, compression, timestamps,
host-endian integers, palette definitions, queue state or VM memory dumps.
The amber cursor overlay is not saved as ink. There is no checksum or promise
to detect corruption that happens to produce another valid drawing.

## Boundary and ownership

`sketch_save` serializes the last completely validated display commit and its
cursor/tool. Saving does not run the ROM or change guest state. Save and load
refuse pending queued input, failed/unstarted instances and concurrent imports.

`sketch_load` validates the complete file before changing anything. Bad lengths,
versions, dimensions, bounds, tools, reserved bytes and cell values return false
without faulting the app or altering its drawing, trace or guest state.

For a valid document, the app stages exactly 771 bytes: x, y, mode, then paper.
Its explicitly attached read-only device returns 1 at port 51 while an import
is active; port 50 returns the next staged byte. Inactive or exhausted reads
return zero and cannot read beyond the buffer. These ports are not general
file devices and expose no host path or directory access.

The app admits action 7 over its existing external input route and executes one
bounded turn. The ROM checks the usual source/length and the active-import flag,
copies the document into its own memory, and publishes exactly once without
applying the tool to the restored cursor cell. It retains the 10,000-instruction
ceiling. The staging buffer is cleared afterward. The public movement/tool API
continues to accept only actions 1–6; a raw action 7 without staged data is ignored.

The malformed-file guarantee is not VM rollback: a broken/replaced trusted ROM
that faults during a valid import stops the application and may have modified
its own memory. The supplied ROM's exact imports and post-import editing are
covered by tests. No changes are made to Uxn, the routed host or the runner.

## Replay and portability

An imported document is external device data. The normal message trace observes
the action and scheduling, not the 771 document bytes. A replay harness must
retain those bytes and import them at the same boundary; replaying action 7
alone is insufficient. This does not introduce a universal replay archive.

Native/WebAssembly tests compare full-state/trace/pixel fingerprints across
imports. Native tests also compare exact state between repeated runs. Browser
tests use real downloads and file uploads, compare all 784 bytes against native
output, and compare all displayed pixels after reopening. These document tests
establish portability for this application profile, not arbitrary hosts or ROMs.

Native file reads are bounded to 785 bytes to detect trailing data. New native
files are created exclusively; existing paths are never overwritten. The browser
delegates download storage to the browser and only reads files selected by the
user. No file is sent over the network, and no implicit autosave is performed.
