# Tracer instruction lengths for prefixed opcodes

Development notes for the fix that taught `trace()` the real length of
`CB`/`DD`/`ED`/`FD` prefixed instructions. This is a history/design record,
not end-user documentation.

## Overview

`aPass1()`'s code tracer assumed **every** prefixed instruction was exactly
two bytes long. Anything longer — `LD (nn),BC` (`ED 43 nn nn`), `LD (IY+d),n`
(`FD 36 d n`), `LD A,(IX+d)` (`DD 7E d`), and all of the HD64180 `IN0`/`OUT0`
forms — left the tracer decoding the tail of one instruction as the start of
the next. Pass 2 was never wrong about these (it decodes from the opcode
tables), so the damage showed up only in the **code/data segmentation**
written to the `.ctl` file, which is exactly what pass 1 exists to produce.

## The problem

Found while disassembling `spengine.rom` as HD64180 (`mdz80 -1 -l -t -xD000
-b spengine.rom`). The reset stub at `D000` is a run of Z180 I/O setup:

```
D000  ED 38 34     in0   a,(34h)      ; 3 bytes
D00A  ED 39 3A     out0  (3ah),a      ; 3 bytes
...
D01F  C3 00 D0     jp    0d000h
```

Each of those is three bytes. Stepping two bytes at a time, the tracer went
out of phase at `D002`, never saw the terminating `jp`, and ran on through
the copyright banner at `D040`, tagging it as reachable code. The `.ctl`
came out with a bogus `c D051-D161` block and the banner text was lost.

The same bug applies to plain Z80 code — it is not HD64180-specific. In
`opsys.rom`:

```
0082  FD 36 26 02  ld    (iy+26h),2   ; 4 bytes, tracer assumed 2
```

which desynchronised the trace and shattered `0000-0169` into eighteen
alternating fragments of "text", "pointers" and "8-bit data".

## What was added

A helper in `mdz80/analyze.c`:

```c
int prefixedLength( int adrs )
```

It returns the instruction length **including the prefix byte**, derived from
the same tables pass 2 decodes with, so the two passes cannot drift apart:

- `CB xx` — always 2.
- `ED xx` — `1 + (edc[xx] & 3)`, where `edc` is `ed1code` for `cputype ==
  c_64180` and `edcode` otherwise. This mirrors pass 2's `opcount`, which is
  built from the same low two bits.
- `DD`/`FD xx` — from `ddcode[xx] & 0xf`: `OPT_DD_2` → 2, `OPT_DD_LOAD` and
  `OPT_DD_ARTH` → 3, `OPT_DD_DIR` and `OPT_DD_CB` → 4.
- Invalid prefixed codes (table entry 0) → 2, which is what the trace
  assumed for everything previously, so undecodable bytes behave as before.

`trace()`'s `code >= 0x100` branch now tags every byte of the instruction and
advances by the real length instead of a hard-coded two.

The table arms were checked by enumeration rather than by sampling, since a
wrong default would reintroduce the same class of bug:

- no nonzero entry in `edcode` or `ed1code` has `(v & 3) == 0`, so the `ED`
  arm can never return a length of 1 (which would have been worse than the
  original bug — it would land the tracer on the ED's own second byte);
- no entry in `ddcode` has `& 0xf` outside `{0,1,2,3,6,7}`, so the `DD`/`FD`
  switch is total over the defined `OPT_DD_*` values and the default arm is
  only reached for invalid codes.

`byte` is `#define byte unsigned char` (`defs.h:69`) and `pgmmem` is `byte *`,
so the table index cannot go negative for second bytes >= 0x80.

## Separate fix: IX/IY operand offsets

In the same branch, `OPCODE_LDIX`/`LDIXI`/`LDIY`/`LDIYI` read their operands
from `tpc + 1` / `tpc + 2`. At that point `tpc` still points at the **prefix**
byte (the `tpc -= 2` at the end of the prefix-decode block puts it back), so
those reads picked up the opcode byte and the low operand byte — e.g. for
`DD 21 lo hi` they computed `ixreg = 0x21 | (lo << 8)`. The non-prefixed
`LD HL,nn` path in the same function is correct, which is what makes the
IX/IY versions visibly off by one. Corrected to `tpc + 2` / `tpc + 3`.

This is **verified by inspection only**. `ixreg`/`iyreg` feed the vstack
vector-reference logic; building a variant with the length fix but the old
offsets produced byte-identical output on all five test ROMs, so nothing in
the available material exercises the difference.

## Verification performed

Baseline (`HEAD`) and fixed binaries were built from the same source tree and
run over every ROM to hand.

- `spengine.rom` (HD64180): the bogus `c D051-D161` block is gone. The banner
  is now classified as text — `t D040-D06D`, `t D06F-D0AD`, `t D0AF-D0FF` —
  and renders as `defb 'Copyright 1989  Robotron P/L. ...'`. The `.ctl`
  splits the `0D 0A` pairs as `b`/`t` rather than keeping them together, but
  the emitted `defb 0dh,0ah` is correct either way.
- `opsys.rom`: `0000-0169` is one clean `c` block instead of eighteen
  fragments; the control file shrank from 1645 to 1579 lines.
- `basic.rom`: at `00C0`, `ED 43 81 C4` (`ld (0c481h),bc`) is now four bytes,
  so the trace correctly stops at the unconditional `jp 0d5edh` at `00C4`
  instead of walking past it.
- `apps.rom`: at `160D`, `DD 7E 01` (`ld a,(ix+1)`) is three bytes; the trace
  now ends at the `jp 0ee3ch` at `1612`.
- `spdata.rom`: byte-identical output, as expected — it contains no traced
  prefixed opcodes.

Segment counts move in both directions and that is the expected outcome, not
a regression: where the old trace desynchronised past an unconditional jump
it covered unreachable bytes as "code" by accident. `apps.rom` goes from 1854
to 1880 segments for exactly that reason — the bytes after `1612` are not
reachable from any traced path, so they are now classified as data.

## Known limitations / TBD

- `check_jump()` keeps its own `needbytes` bookkeeping. It happens to be
  right for the prefixed opcodes it recognises (`RETI`/`RETN`/`PUSH IX` etc.
  are all two bytes, and `JP (IX)` is special-cased), so it was left alone,
  but it is a second place that encodes instruction sizes.
- The `.ctl` text/binary run boundaries are one byte out around `CR`/`LF`
  pairs (`b D06E` / `t D06F`). That is in the gap scanner, not the tracer,
  and does not affect the emitted source.
- `prefixedLength()` is only used inside `analyze.c` and could be `static`;
  it was given a prototype in `analyze.h` to match the file's other entry
  points.
