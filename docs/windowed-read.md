# Windowed binary reads (`-k` / `-z`)

Development notes for the work that added `-k`/`-z` options to `mdz80`. This
is a history/design record, not end-user documentation — see
`mdz80/manual.html` and `mdz80 -?` for the current option list.

## Overview

`mdz80` gained two new CLI options for binary input:

- **`-k <hex>`** — skip that many bytes at the start of the input file before
  reading.
- **`-z <hex>`** — read at most that many bytes from the input file.

Both follow the same attached-hex-argument style as the existing `-x [nnnn]`
address offset (e.g. `-k1000`, not `-k 1000`). Combined with `-x`, they let a
byte-range window of a larger binary file be disassembled directly and
mapped to an arbitrary address, without pre-processing the file with an
external tool.

## The problem

The user's EPROM images are pre-split, by a separate ROM-validity-scanning
tool, into logical `*.rom` segment files. Even so, an individual segment can
still be considerably larger than the address window it's actually mapped
into on the real hardware (e.g. a 12kB window at `0xC000`), and larger than
what's convenient to test and disassemble incrementally — the user wanted to
look at one 4kB page at a time while getting a feel for what a segment
contains.

`mdz80` had no way to disassemble only part of a binary file: for
`BINFILE`/`CPMFILE` input it always read the entire file into memory starting
at the `-x` address (see `readfile()` in `mdz80/mdz80.c`), with no
start/length option and no hidden facility for it either — the `-x [nnnn]`
option only shifts where the bytes are *placed* in the disassembled address
space, it doesn't change where reading *starts* in the input file. The only
existing option was slicing the file externally (e.g. with `dd`) before every
run, which doesn't scale to frequent, iterative use across many pages.

## What was added

In `mdz80/mdz80.c`:

- Two new globals, `skipbytes` and `maxbytes` (both default `0`, meaning "no
  skip" / "no limit", preserving existing behavior when neither option is
  given).
- CLI parsing for `-k`/`-z`, placed next to the existing `-x` handling and
  using the same `atox(inp)` attached-argument parsing.
- In the binary-file read loop: an `fseek(fp, skipbytes, SEEK_SET)` before
  reading starts (when `-k` is given), and a `bytesread` counter that stops
  the read loop once `maxbytes` bytes have been stored (when `-z` is given).
- Matching `-?` usage text and `-v` verbose-mode status lines.

This only touches the binary/`.com` read path (`fileflag == BINFILE ||
fileflag == CPMFILE`); the Intel-hex path was left untouched (see
Limitations below).

### Usage example

Pulling one 4kB page out of a larger segment file and mapping it to `0xC000`:

```sh
mdz80 -xC000 -k1000 -z1000 -b segment.rom   # bytes 0x1000-0x1FFF of the file
mdz80 -xC000 -k2000 -z1000 -b segment.rom   # next 4kB page
```

If a segment file is already exactly the size of its target window, `-k`/`-z`
aren't needed at all — just `-x` on the whole file, as before.

## Verification performed

- Built a synthetic 12kB image out of three distinctly-marked 4kB pages
  (each starting with `LD A,<page marker>` / `JP <page start>`).
- Confirmed each `-k <hex> -z 1000` combination extracts exactly the intended
  page, correctly mapped via `-x`, with the trailing `JP` correctly resolving
  to an in-window label rather than spilling into the next page — proving
  both the skip and the length cap work together correctly.
- Confirmed a plain `-x` run with no `-k`/`-z` is unaffected.
- Exercised an edge case: `-k` landing 2 bytes from EOF with a `-z` larger
  than what remains. This read to EOF gracefully (no crash, no error) rather
  than reading past the end of the file. The resulting output initially
  looked empty ("just END, no instructions") — tracked down to a pre-existing,
  unrelated heuristic that suppresses long runs of `0x00` bytes, unconnected
  to this feature: reproduced the identical "empty" output on a plain 2-byte
  all-zero file with no `-k`/`-z` involved at all, and confirmed a
  non-zero-content window of the same tiny size disassembles normally.
- Full clean rebuild compiles with no new warnings.

Reference: commit `ac4a48d` ("Add -k/-z options to disassemble a byte window
of a binary file").

## Known limitations / TBD

- **Intel-hex input isn't supported.** `-k`/`-z` only affect the
  `BINFILE`/`CPMFILE` read path; the separate hex-record parsing loop in
  `readfile()` is untouched. Not needed for the user's current `.rom`-file
  workflow, but would need its own implementation (working in terms of
  hex-record addresses rather than a raw file-byte offset) if ever wanted.
- **Two separate flags rather than one combined "window" option.** `-k
  <skip> -z <length>` was chosen to match the existing single-hex-value `-x`
  convention and keep argument parsing trivial, rather than introducing a
  new comma-separated syntax (e.g. a hypothetical `-w <skip>,<length>`).
  Worth revisiting if this turns out to be awkward in practice.
