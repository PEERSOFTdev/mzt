# Routines that take their argument inline

Development notes for teaching `mdz80` about calls whose argument sits
immediately after the call instruction. This is a history/design record, not
end-user documentation — the manual documents the `R` directive and `-i`.

## The idiom

```
        CALL PRINT
        DEFB 'Hello',0
        ...execution continues here
```

The callee lifts its return address off the stack, walks it past the data and
jumps back, so the bytes after the call are an argument, not code. Variants
differ only in how the argument ends: a `00` byte, a leading length byte
(Turbo Pascal style), bit 7 set on the last character (a "DC" string), `'$'`
(CP/M BDOS 9), or a fixed count.

## The problem

`trace()` pushed the address after every `CALL` onto its stack as a code
continuation point:

```c
astack[astackPtr++] = tpc;   /* byte after the call operand */
tpc = adrs;
```

For these routines that address is the string. The tracer disassembled the
text as instructions and stayed misaligned until it happened to resynchronise,
and because the region was genuinely *traced*, nothing downstream corrected it.

Two real cases drove the work:

- `mem.com`, a CP/M program: the routine at `054E` does `POP DE` / `LD A,(DE)`
  / … / `LDIR` / `JP (HL)` — it copies a length-prefixed string onto the stack.
  Call sites such as `2C79` were disassembled as `call X054e` followed by a
  stray `nop` (the length byte), with `07 'mem.rec'` a few bytes later decoding
  as instructions.
- `apps.rom` at file offset `015F`, which is address `D15F` — these images are
  loaded at `D000`, so they are disassembled with `-xD000`. The call is
  `CALL EBFA`, followed by a NUL-terminated Czech string. The routine at
  `EBFA` does `EX (SP),HL` / `LD A,(HL)` / `OR A` / print / `INC HL` / loop,
  and puts the advanced pointer back with a second `EX (SP),HL`.

## What was added

**An `r` control directive** naming a routine and its convention
(`z`/`l`/`d`/`$`/hex count). Unlike every other directive it is read *before*
the trace, by `getCTLinline()` in `mdz80.c`, because the tracer needs it while
it is following the code — pass 0 runs far too late. It is re-emitted by
`genAnalysisList()` so it survives the wholesale control-file rewrite that
every `-t` run performs.

**Automatic detection** in `detectInline()`, for routines that are part of the
image being disassembled. The signature is deliberately narrow: the return
address must be taken off the stack by the very first instruction (`POP rr` or
`EX (SP),HL`), and control must leave through `JP (HL)`/`(IX)`/`(IY)`, or a
`RET` that returns to a pointer the routine put back on the stack — either by
pushing it, or with a second `EX (SP),HL`. The convention is then inferred from the body.
Detected routines are reported and written into the control file as `r` lines,
so the guess is visible and can be corrected or deleted. `-i` disables it.

## Two things that went wrong first, and why the code looks as it does

**Negative results must not be cached in the routine table.** The first version
recorded every examined call target, positive or not, and a 64-entry table
overflowed almost immediately on a real program. Only positives are stored now;
re-examining a non-inline routine costs at most 24 decoded instructions.

**A count register alone does not mean a length prefix.** `basic.rom` has a
routine at `D443` that does `EX (SP),HL` / `LD C,(HL)` / `INC HL` /
`EX (SP),HL` — it takes a *single* inline byte and uses it to index a table,
advancing the return address by exactly one. The count-evidence rule read
`LD C,(HL)` as a length prefix and swallowed the following eight bytes. An
`ADD HL,BC` appears in that routine too, but on the table pointer rather than
the return address, so it is no help as a discriminator. What the genuine
length-prefixed routines have and this one does not is a loop over the
argument: `mem.com`'s `054E` uses `LDIR`, its `17BB` uses `DJNZ`. A length
prefix is therefore only believed when a count register *and* one of those
appear. `D443` now goes undetected, which is the right outcome — it can be
declared with `r D443,1` if its single byte should be marked.

**A count is not a terminator.** `mem.com`'s routine at `17BB` reads its first
byte, tests it with `OR A` and branches away if zero, *then* moves it into `B`
as a count — it special-cases the empty string. The first detector saw the zero
test and called it NUL-terminated, which made one call site swallow 215 bytes
— several genuine `CALL` instructions among them — up to the next `00`. That
is a worse failure than the original bug, so:

- evidence that the first byte becomes a *count* (`LD B,A`, `LD C,A`,
  `LD B,(HL)`, `LD C,(HL)` near the entry) now outranks a zero test — but only
  counts as a length prefix when the routine also *walks* the argument, with
  `LDIR` or a `DJNZ` loop (see the third mistake below);
- `looksLikeText()` rejects any argument that is not at least four fifths
  printable, so a misjudged routine falls back to the previous behaviour
  instead of eating code.

## Verification

These images map at `D000` through the HD64180 CBR, a 4K page at a time, and
are larger than the window they map into, so they are disassembled in 12K
windows (`-xD000 -k<offset> -z3000`). Measured against the previous build:

| input | detected | effect |
| ----- | -------- | ------ |
| `mem.com` (`-C`) | `054E`, `17BB` | 1906 bytes code → text; text 168 → 1961 |
| `apps.rom` `-k0` | `EBFA`, `EC11` | 1395 bytes code → text; text 916 → 2222 |
| `apps.rom` `-k3000/-k6000/-k9000` | none | unchanged |
| `basic.rom` `-k0` | none | unchanged |
| `opsys.rom` all windows | none | unchanged |
| `spengine.rom` all windows | none | unchanged |

So two routines in `apps.rom` and two in `mem.com`, with every other window
byte-for-byte as before. The `apps.rom` call sites and their printer both live
in the first window; the later windows contain no calls to it.

Also checked: `-i` suppresses detection while still honouring an `r`
declaration; `r` lines round-trip through repeated `-t` runs; and synthetic
images covering the NUL and length-prefixed conventions disassemble correctly.

## Surveying the other images

Rather than trusting the absence of detections, every address actually called
from a region `mdz80` considers code was examined for the signature. That
survey paid for itself twice.

It found `EC11` in `apps.rom` — 10 call sites, a body byte-for-byte identical
to `EBFA`'s — rejected purely because it restores the return pointer with a
second `EX (SP),HL` and returns, where `EBFA` happens to contain a stray
`PUSH HL` that the old rule accepted. That is why the `EX (SP),HL` exit is now
recognised.

It also found a family in `apps.rom` — `FA92` and `FA97` in the first window
(31 call sites), `EAD3` and `EAD8` in the third (42 sites) — shaped
`POP HL` / `LD C,flag` / `CALL worker` / `JP (HL)`, where each worker sets
`B` and ends in `JP C142`, a BIOS device-control entry outside the loaded
window. The convention cannot be read out of the routine, because the code
that walks the argument is the BIOS.

It can be read out of the *data*, though: all 73 call sites are followed by
printable text ending in a `00` byte, and in every case the bytes after that
terminator resume as plausible code (`CALL F895`, `LD HL,DDFF`, `JP E9D3`,
`CALL EB98`). So the BIOS call returns `HL` past the string. Declaring the
four with `r FA92,z`, `r FA97,z`, `r EAD3,z`, `r EAD8,z` recovers a further
664 bytes in the first window and 1355 in the third.

These are the strongest argument for keeping the `r` directive: no amount of
inspection of the callee could have settled it.

## Inferring a convention from the call sites

The `C142` family showed that a routine can have the right shape while the
code that walks its argument sits in a BIOS the disassembler cannot read. For
those the call sites are the only evidence, and they turn out to be plenty.

A routine that matches the shape but yields no convention becomes a
*candidate*, and the tracer records where it was called from. When the trace
finishes, each terminator is tried against every recorded site. A site
**agrees** if the argument is at least three characters, at least 60% plain
ASCII, and — the load-bearing part — the byte after the terminator could begin
an instruction. Guess the wrong terminator and the resume point lands
mid-instruction, so that one check is what separates a real string from a run
of bytes that merely looks like one. A site **contradicts** if the scan runs
into a control byte with no terminator, or the resume point is implausible. A
convention is suggested only when nothing contradicts it and at least two
sites agree; a one or two character argument is counted as neither.

Nothing is acted on. A commented directive goes into the control file with
the evidence behind it, so a five-site suggestion can be judged differently
from a twenty-nine-site one:

```
;r FA92,z    ; SUGGESTED: 23 call sites, 23 strings, avg 19 chars, no counter-example. Remove the ';' to accept.
```

Accepting is `sed 's/^;r /r /'` over the control file, which is what makes 29
call sites a single edit. `getCTLinline()` ignores comment lines, so a
suggestion is inert until the semicolon goes; once it does, the line is read
as a declaration and re-emitted as an accepted one, not suggested again.

A string bound for a terminal carries the occasional control code, so
aborting the scan at the first one loses real strings: `opsys.rom`'s `E794`
prints `ESC * "EurekaDOS"`, and that single `1B` byte was enough to
contradict the convention and suppress the whole routine. Control codes get a
budget of two rather than free rein — machine code is full of bytes below
`20h`, so anything beyond a couple is good evidence this is not text. Raising
it from zero to two admitted `E794` and produced no other suggestion on any
image.

Four versions of the scoring were wrong before this one, each plausible:
masking bit 7 before testing printability turned Czech national characters
into control codes and suppressed every suggestion; finding call sites by
scanning for `CD` bytes invented phantom sites inside text; and requiring
every site to agree threw away `EAD3`, whose 29 sites include three arguments
of one or two characters that are legitimate but too short to judge. Hence
contradictions rather than unanimity, and a scan that does not strip bit 7.

Note that scoring deliberately does **not** reuse `inlineArgLength()`. That
function guards the path that acts and has to stay strict about what it will
swallow; scoring only decides whether the sites agree, and has the resume
check to keep it honest.

What it suggests on the images to hand, none of it acted on:

| image | window | suggestion | evidence |
| ----- | ------ | ---------- | -------- |
| `apps.rom` | `-k0` | `FA92,z`, `FA97,z` | 23 and 7 sites, no counter-example |
| `apps.rom` | `-k6000` | `EAD3,z`, `EAD8,z` | 29 and 13 sites |
| `opsys.rom` | `-k0` | `E794,z`, `F01E,z` | 3 and 5 sites |
| `opsys.rom` | `-k3000` | `E46E,z` | 10 sites |
| `mem.com` | — | `14BB,z` | 8 sites, 6 strings, avg 4 chars |

Accepting the two in `apps.rom -k6000` takes that window from 377 bytes of
text to 1605. Disassembly output is byte-for-byte unchanged until a
suggestion is accepted.

## Known limitations / TBD

- Only the `CALL` path is handled. The same idiom after `RST n` — common in
  CP/M code — is not. That is not a small addition: the tracer does not handle
  `RST` at all today. `OPCODE_RST00` … `OPCODE_RST38` are defined in
  `analyze.h` but referenced nowhere in `analyze.c`, so a restart is currently
  decoded as an ordinary one-byte instruction and its target is never followed.
  Supporting inline data after `RST` means teaching the tracer about restarts
  first.
- The conventions are inferred from a peephole over the callee's first 24
  instructions. A routine that sets up its counter unusually, or ends the
  argument in a way not listed above, will be missed; declare it instead.
- Routines taking a single inline byte rather than a string — `basic.rom`'s
  `D443` is one — are deliberately not detected, because they are hard to tell
  apart from a length prefix without following the return pointer properly.
  Declare them with a fixed count, `r D443,1`.
- Detection reads only the callee. When the argument is walked by code
  outside the window, nothing in the routine says how the argument ends —
  that is what the call-site inference below is for.
- Detection cannot reach a callee outside the loaded image — a call into a
  bank or ROM that is not part of the file being disassembled can only be
  declared with `r`. Note that "outside the image" depends entirely on the
  load address: disassembling one of these ROMs without `-xD000` puts its
  callees out of range and makes the routines undetectable, which is an
  artefact of the wrong base rather than a property of the file.
- Non-trace runs rely on the `t`/`b` lines a previous trace wrote; the `r`
  directive itself only affects tracing.
