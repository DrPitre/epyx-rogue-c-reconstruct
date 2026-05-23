# Epyx Rogue's OS-9 Memory Trick

The Epyx CoCo 3 port of Rogue uses an OS-9 memory layout that looks odd at
first, but makes a great deal of sense once the 8K block allocator is visible.
The game keeps the program module mostly code, declares almost no OS-9 data,
then allocates and loads the real game data area at runtime from `rogue.dat`.

That changes the tradeoff. Instead of spending four 8K blocks on code and four
8K blocks on declared process data, Epyx shifts the budget toward code. The
external `rogue.dat` file becomes the 24K game arena, and the program module can
grow closer to 40K.

## What The Original Binary Shows

The original Epyx binary reports a tiny declared data size:

```text
Header for : rogue
Module size: $97FC  #38908
Exec. off  : $0013  #19
Data size  : $0013  #19
Prog mod, 6809 Obj, re-ent, R/O
```

The companion file is much larger:

```text
rogue.dat: 24138 bytes
rogue:     38908 bytes
total:     63046 bytes
```

That is the giveaway. The OS-9 module is not carrying normal initialized game
data. The data lives in `rogue.dat`, and the loader code brings it into memory
after startup.

![Original Epyx Rogue PMA and ident output](docs/images/epyx-rogue-pmap-ident.png)

Screenshot: PMA/ident output for the original Epyx Rogue. The module identifies
as a 38908-byte program with only 19 bytes of declared data, even though
`rogue.dat` is another 24138 bytes and must be present at runtime.

## What The C Port Shows

The C port initially has a very different shape:

```text
Header for : roguec
Module size: $7374  #29556
Exec. off  : $000D  #13
Data size  : $7B3E  #31550
Prog mod, 6809 Obj, re-ent, R/O
```

PMA shows the practical consequence: about four 8K blocks for code and four 8K
blocks for data. That is balanced, but it leaves little room for growth on
either side before crossing another 8K boundary.

![C Rogue PMA and ident output](docs/images/roguec-pmap-ident.png)

Screenshot: PMA/ident output for the C port. `roguec` is just under 32K of
module code and just under 32K of declared data, so it occupies roughly four
8K blocks on each side.

## How Epyx Appears To Have Built It

The Epyx program does not look like it treats `rogue.dat` as a loose collection
of records. It looks like `rogue.dat` is a pre-laid-out memory image.

The evidence in `rogue.asm` points that way:

- startup asks OS-9 for about 24K of memory
- startup opens `rogue.dat`
- startup reads the data file into the newly allocated memory area
- many routines reference fixed offsets into that area
- save/restore writes and reads the whole data block as one contiguous region
- pointer tables use fixed-address adjustment expressions tied to the program
  load offset

That suggests the original build process probably assembled or linked code and
data with a known address model, then emitted the program module and the data
image separately. The exact Epyx toolchain is not proven from the source we have,
but the runtime model is clear: the binary expects `rogue.dat` to already have
the shape of the process data area it wants.

## Why This Helps On OS-9

OS-9 Level Two memory is allocated in 8K blocks. Once code or data crosses an 8K
boundary, another whole block is needed.

The C port's current default build is close to a symmetrical split:

```text
roguec
Module size: 29556 bytes
Data size:   31550 bytes
```

That is effectively:

```text
code: 4 blocks, just under 32K
data: 4 blocks, just under 32K
```

The Epyx layout is closer to:

```text
code: up to 5 blocks, just under 40K
data: small declared data plus a 24K loaded arena
```

So the trick is not merely saving memory. It changes which side gets the scarce
8K blocks. Rogue needs a lot of code for behaviors, commands, monsters, and
object effects. Epyx bought more code room by making the large data area
explicitly loaded instead of declared in the module header.

## The C Port Direction

The C port now uses the heap-arena layout for the normal `roguec` build:

```sh
make -C utils/rogue_epyx_c
```

This build heap-allocates `rogue_arena` before reading `rogue.dat`, instead of
declaring the full arena as C module data. `roguec-heap` is kept as an explicit
same-layout comparison binary, and `roguec-static` preserves the old static
arena layout as a fallback.

Latest size comparison:

| Binary | Module size | Data size | Notes |
|---|---:|---:|---|
| old static `roguec` | `$7374` / `29556` | `$7B3E` / `31550` | previous default layout |
| `roguec-static` | `$737B` / `29563` | `$7B3E` / `31550` | old layout kept as fallback |
| `roguec` | `$76AC` / `30380` | `$1B48` / `6984` | normal Epyx-shaped build |
| `roguec-small` | `$6475` / `25717` | `$12C0` / `4800` | feature-trimmed heap build |
| `roguec-heap` | `$76B1` / `30385` | `$1B48` / `6984` | same heap layout under explicit name |

The tradeoff is exactly what we want to study:

```text
roguec heap layout vs old static layout:
code: +829 bytes
data: -24566 bytes
```

That is not a total-memory miracle. The 24K arena still exists at runtime. But
it is no longer part of the module's declared data size, which means the OS-9
block allocation starts to resemble Epyx's split.

## Recommendation

The promising direction is to keep `rogue.dat` external and use the heap-arena
model as the main architecture. It preserves portability better than loading
data at address `$0000`, but it keeps the important idea: bulky game data stays
out of the module's declared data allocation so code has room to grow.

Continue testing the heap layout:

- confirm startup succeeds with the heap arena
- confirm `rogue.dat` still loads before any arena string/table access
- confirm save/restore still writes the full arena
- inspect PMA while `roguec` is running
- compare the observed block map against original Epyx Rogue

`roguec-static` can remain as a conservative fallback while the heap model gets
more runtime coverage.
