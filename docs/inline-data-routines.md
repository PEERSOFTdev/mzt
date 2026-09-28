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
`EX (SP),HL`), and control must leave through `JP (HL)`/`(IX)`/`(IY)` or a
pushed address and a `RET`. The convention is then inferred from the body.
Detected routines are reported and written into the control file as `r` lines,
so the guess is visible and can be corrected or deleted. `-i` disables it.

## Two things that went wrong first, and why the code looks as it does

**Negative results must not be cached in the routine table.** The first version
recorded every examined call target, positive or not, and a 64-entry table
overflowed almost immediately on a real program. Only positives are stored now;
re-examining a non-inline routine costs at most 24 decoded instructions.

**A count is not a terminator.** `mem.com`'s routine at `17BB` reads its first
byte, tests it with `OR A` and branches away if zero, *then* moves it into `B`
as a count — it special-cases the empty string. The first detector saw the zero
test and called it NUL-terminated, which made one call site swallow 215 bytes
— several genuine `CALL` instructions among them — up to the next `00`. That
is a worse failure than the original bug, so:

- evidence that the first byte becomes a *count* (`LD B,A`, `LD C,A`,
  `LD B,(HL)`, `LD C,(HL)` near the entry, or `LDIR`) now outranks a zero test;
- `looksLikeText()` rejects any argument that is not at least four fifths
  printable, so a misjudged routine falls back to the previous behaviour
  instead of eating code.

## Verification

- `mem.com` (`-C -l -t`): both inline-data routines detected unaided; 1906
  bytes reclassified from code to text; the call/string chains segment cleanly
  as length byte, text, code, repeating.
- `apps.rom` (`-b -l -t -xD000`): the routine at `EBFA` is detected unaided and
  1283 bytes are reclassified from code to text, the strings at `D162` onward
  among them. No `r` directive is needed for this file.
- `basic.rom`, `opsys.rom`, `spengine.rom`: no inline-data routine detected and
  output byte-for-byte identical to the previous build. Note these particular
  comparisons were run at offset 0 for `basic.rom` and `opsys.rom`, whose load
  addresses were not established; a wrong base hides both call sites and
  callees, so they say less than they appear to.
- Synthetic images covering the NUL and length-prefixed conventions; `-i`
  suppresses detection; `r` lines round-trip through repeated `-t` runs.

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
- Detection cannot reach a callee outside the loaded image — a call into a
  bank or ROM that is not part of the file being disassembled can only be
  declared with `r`. Note that "outside the image" depends entirely on the
  load address: disassembling one of these ROMs without `-xD000` puts its
  callees out of range and makes the routines undetectable, which is an
  artefact of the wrong base rather than a property of the file.
- Non-trace runs rely on the `t`/`b` lines a previous trace wrote; the `r`
  directive itself only affects tracing.
