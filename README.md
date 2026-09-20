# MZT — My Z80 Tools

A set of Z80 cross-development tools for Linux: a macro cross-assembler, a
linker/librarian, and a "smart" disassembler.

## Origin and credits

**MZT is the work of Piergiorgio Betti.** The project's home — and the
authoritative description of it — is the author's own page:

> **<https://z80cpu.eu/78-data-articles/projects/76-mzt>**

That is where MZT is described properly, by the person who wrote it. This
README does not reproduce or replace that description; please read it there.

How this repository came about:

| When | What |
| ---- | ---- |
| 2012 | Piergiorgio Betti releases MZT 1.0.0 |
| 2020 | [napobear](https://github.com/napobear/mzt) imports the package sources to GitHub, preserving them |
| 2026 | this repository continues from that import, with improvements to `mdz80` |

This repository is **not affiliated with, endorsed by, or connected to the
original author.** All credit for the design and the original code is his. What
is added here is a continuation of his work, offered in that spirit — and it
would gladly go upstream if he ever wants any of it.

The tools also carry substantial code from earlier projects, credited in full
under [Credits](#credits) below.

## What's in the package

| Tool | What it is |
| ---- | ---------- |
| `mzmac` | Macro cross-assembler for the Z80. Descends from Bruce Norskog's 1978 `zmac`, by way of George Phillips' later work. |
| `mld80` | Linker/librarian — a replacement for the old Microsoft L80, reading `.REL` object files from M80-compatible assemblers and `.LIB` libraries. Based on Gábor Kiss's `ld80`, and still identifies itself as `ld80`. |
| `mdz80` | Control-file-driven disassembler for Z80, HD64180/Z180 and 8080 code, with an optional tracer that follows real control flow to tell code from data. Descends from Jeffery L. Post's D52/DZ80. |

Version numbers are per-tool and predate the 1.0.0 package version: `mdz80`
0.9.1-alpha, `mld80` 0.9.5, `mzmac` 0.9.1.

## Building

```sh
make                 # configures CMake into build/, then builds
sudo make install    # installs into /usr/local/bin
```

`make install` honours `INSTALL_PREFIX`, e.g.
`make install INSTALL_PREFIX=$HOME/.local`. Built binaries are left in
`build/mzmac/mzmac`, `build/mld80/mld80` and `build/mdz80/mdz80`.

Requirements: **CMake**, **bison**, and a **C and C++ compiler**. Bison
generates the assembler parser used by both `mzmac` and `mdz80`; the C++
compiler is needed for `mzmac/zi80dis.cpp`. There are no other dependencies.

> **Note for modern systems:** the CMake files declare
> `cmake_minimum_required(VERSION 2.6)`. CMake 3.x accepts this with a
> deprecation warning, but **CMake 4.0 and later reject it outright**. On such a
> system you will need to raise that line, or configure with
> `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`.

## Quick start (mdz80)

`mdz80` reads a binary, Intel hex, or CP/M `.com` image and writes assembler
source. With no `-o`, the output file is named after the input: `.z80` by
default, `.180` with `-1`, `.d80` with `-8`.

```sh
mdz80 rom.bin                      # straight disassembly -> rom.bin.z80
mdz80 -t -l rom.bin                # trace code vs data, lower case
mdz80 -1 -l -t -xD000 rom.bin      # HD64180/Z180, loaded at D000 -> rom.bin.180
mdz80 -xC000 -k1000 -z1000 -b rom.bin   # 4K window at file offset 1000, mapped to C000
```

Numeric option arguments are **attached** to the option letter (`-x1000`, not
`-x 1000`).

Running with `-t` also writes a **control file** (`rom.bin.ctl`) recording which
regions are code, data or text. Edit it and re-run to refine the disassembly —
that iterative loop is the heart of how `mdz80` is meant to be used, and it is
documented in full, along with every option and control directive, in
[`mdz80/manual.html`](mdz80/manual.html).

## Changes in this repository

Everything below is additions to Betti's package; see the linked notes for the
reasoning and the details.

- **HD64180/Z180 support** — a new `-1` option disassembling the Hitachi
  HD64180/Z180 extensions (`IN0`, `OUT0`, `MLT`, `TST`, `TSTIO`, `SLP`,
  `OTIM`/`OTDM`/`OTIMR`/`OTDMR`), with the CPU type threaded through both
  passes. ([notes](docs/HD64180.md))
- **Windowed binary reads** — new `-k` (skip N bytes) and `-z` (read at most N
  bytes) options, so a single page of a larger ROM image can be disassembled and
  mapped to its real address with `-x`. ([notes](docs/windowed-read.md))
- **Tracer correctness fixes** — the tracer assumed every `CB`/`DD`/`ED`/`FD`
  prefixed instruction was two bytes long, which desynchronised code/data
  segmentation; IX/IY operand offsets in its register tracking were also wrong.
  ([notes](docs/tracer-prefix-length.md))
- **Documentation and attribution** — the manual now covers `-1`, `-k` and `-z`;
  the version banner gives the original author and the later modifications their
  own copyright lines; modified files carry GPLv3 §5(a) change notices.
- **One structural change to the upstream tree** — the unused `mdz80/opcodes.h`
  was removed and `opcodes_z180.c` wired into the build, so a diff against
  Betti's original tarball will show that file as deleted.

The files under [`docs/`](docs) are development records of how and why these
changes were made, not end-user documentation.

## Documentation

- [`mdz80/manual.html`](mdz80/manual.html) — the `mdz80` manual: every option,
  the control-file directives, and the iterative disassembly workflow.
- `mdz80 -?`, `mld80 -h`, `mzmac -h` — usage summaries.
- `mzmac --help` prints the full integrated assembler manual; `mzmac --html`
  emits it as HTML.
- `mld80/ld80.html` — the linker's manual page, as shipped with the package.
  (`mld80/ld80.1`, the nroff source of the same page, was added in this
  repository.)

## License

The package-level license is the **GNU General Public License v3**, in
[`License.txt`](License.txt).

What the sources themselves state is not uniform, and this is inherited from
upstream rather than something this repository is in a position to resolve:

| Component | As stated in its own files |
| --------- | -------------------------- |
| `mdz80` | GPL v3 or later — a license header on every source file, consistent with `License.txt`. |
| `mld80` | No source headers. Its manual page says both "This software is copylefted." and "This software is in the public domain." — which are contradictory. |
| `mzmac` | No copyright or license statement in any source file. |

If you intend to redistribute or reuse parts of this package, that table is
worth reading carefully first.

## Credits

- **Piergiorgio Betti** — MZT itself: the package, `mdz80`'s control-file
  disassembly design, and the integration of the tools.
- **Jeffery L. Post** — D52/DZ80, the disassembler `mdz80` is built on
  (© 1990–2007).
- **Bruce Norskog** — the original `zmac` (1978); with later work by
  **John Providenza**, **Colin Kelley**, **Russell Marks**, **Mark RISON**,
  **Chris Smith**, **Matthew Phillips**, **Tim Mann**, **Thierry Jouin**, and
  extensive modifications by **George Phillips**.
- **Gábor Kiss** — `ld80`, the linker behind `mld80`.
- **napobear** — preserving the package on GitHub, which is where this
  repository started.
- HD64180/Z180 opcode and timing details were checked against Cameron W.
  Cotrill's 1988 *Z80 and 64180 Assembly Language Help Guide* (a CP/M `.LBR`
  help library, © 1988, all rights reserved — not included here).
