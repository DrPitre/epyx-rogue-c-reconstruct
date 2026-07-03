# Epyx Rogue C Reconstruction

This repository is for a readable C reconstruction of the CoCo 3 Epyx Rogue
program. The goal is not to port the larger Unix/PC C source. The goal is to
translate the compact Epyx OS-9 architecture into maintainable C while keeping
the same external data-area model.

The original executable allocates 24K, reads `rogue.dat` into that arena, then
uses fixed offsets inside the arena for game state, tables, strings, and screen
buffers. This reconstruction keeps that model because it is the main reason the
Epyx executable is about 39K.

## Provenance

The CoCo 3 Epyx port has a small author-credit trail hidden in its own data.
The embedded save-file header in `rogue.dat` names the port authors as
`Mike Leber, Ron Miller, James Long, Ed Rosenzweig`. The original assembly
disassembly corroborates this: save-game creation writes that 50-byte header,
and restore validation checks that a save file starts with the same author
string. In the local `rogue.asm`, see the save logic near line 416 and the
restore validation near line 501.

The in-game version command is more cryptic. It prints
`Rogue version %i.%i (mll and rbm)`, which corresponds to Mike L. Leber and
Ron B. Miller. On a CoCo display this can look like `m1 and r0m`, but the bytes
in `rogue.dat` are `mll and rbm`. The version-command path in the local
disassembly references that string near line 8065.

This port belongs to the commercial Epyx/A.I. Design family of Rogue releases,
derived from the earlier Unix Rogue lineage by Michael Toy, Ken Arnold, and
Glenn R. Wichman. L. Curtis Boyle's
[Rogue page](https://www.lcurtisboyle.com/nitros9/rogue.html) credits the CoCo
3 port to Mike L. Leber, Ron B. Miller, James Long, and Ed Rosenzweig for Epyx.
It also notes the wonderfully odd historical detail that CoCo 3 Rogue shipped
with an early OS-9 Level II environment, and that some early users treated Rogue
disks as a practical way to bootstrap Level II before the official release fully
landed.

There is also a useful oral-history thread from the Tandy side. In a Discord
chat on May 22, 2026, Mark Siegel of Tandy recalled that Epyx had shown Rogue
for the Apple II at CES, after which he requested an OS-9 version for the CoCo
3. He remembered the CoCo 3 version as using a graphics character set, and noted
that Rogue was already freely distributed on systems such as Coherent and Minix.
He did not know where the root version used for the CoCo port came from, so that
part of the port lineage remains unresolved.

A few cross-checks line up with that author trail. CoCopedia's
[Sub Battle Simulator page](https://www.cocopedia.com/wiki/index.php/Sub_Battle_Simulator)
credits Jesse Taylor and Mike Leber on another CoCo/Epyx title. MobyGames'
[Ed Rose page](https://www.mobygames.com/person/2097/ed-rose/) identifies Ed
Rose as Ed Rosenzweig and notes his Epyx CoCo port work. For broader historical
context and source archaeology, see the
[Rogue Archive](https://britzl.github.io/roguearchive/) and Glenn Wichman's
[A Brief History of Rogue](https://web.archive.org/web/20150217024917/http://www.wichman.org/roguehistory.html).

## OS-9 Memory Model

The Epyx CoCo 3 port of Rogue uses an OS-9 memory layout that looks odd at
first, but makes a great deal of sense once the 8K block allocator is visible.
The game keeps the program module mostly code, declares almost no OS-9 data,
then allocates and loads the real game data area at runtime from `rogue.dat`.

That changes the tradeoff. Instead of spending four 8K blocks on code and four
8K blocks on declared process data, Epyx shifts the budget toward code. The
external `rogue.dat` file becomes the 24K game arena, and the program module can
grow closer to 40K.

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

The C port initially had a very different shape:

```text
Header for : roguec
Module size: $7374  #29556
Exec. off  : $000D  #13
Data size  : $7B3E  #31550
Prog mod, 6809 Obj, re-ent, R/O
```

The `pmap` command output below shows the practical consequence of the Epyx port
of Rogue running on a CoCo 3: five 8K blocks for code and three 8K
blocks for data.

```text
ID   01 23 45 67 89 AB CD EF  Program
---  -- -- -- -- -- -- -- --  -----------
 1   00 .. 13 02 03 04 05 3F  SYSTEM
 2   09 .. .. .. .. .. .. 0A  shell
 3   06 .. .. .. .. .. .. 0A  shell
 4   0B 01 12 0C 0D 0F 10 11  rogue
 5   14 .. .. .. .. .. .. 15  PMAP
```
## Epyx's Data Image

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

OS-9 Level Two memory is allocated in 8K blocks. Once code or data crosses an 8K
boundary, another whole block is needed. The C port's old static-arena build was
effectively:

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

## Current Direction

The C port now uses the heap-arena layout for the normal `roguec` build:

```sh
make
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
| `roguec` | `$7608` / `30216` | `$1B0A` / `6922` | normal Epyx-shaped build |
| `roguec-small` | `$63E0` / `25568` | `$1282` / `4738` | feature-trimmed heap build |
| `roguec-heap` | `$760D` / `30221` | `$1B0A` / `6922` | same heap layout under explicit name |

The tradeoff remains exactly what we want to study. The 24K arena still exists
at runtime, so this is not a total-memory miracle. But the arena is no longer
part of the module's declared data size, which means the OS-9 block allocation
starts to resemble Epyx's split.

The promising direction is to keep `rogue.dat` external and use the heap-arena
model as the main architecture. It preserves portability better than loading
data at address `$0000`, but it keeps the important idea: bulky game data stays
out of the module's declared data allocation so code has room to grow.

## Building

By default the Makefile expects this repository to sit beside the existing
`coco-shelf` checkout:

```sh
~/Projects/epyx-rogue-c-reconstruct
~/Projects/coco-shelf/cmoc_os9
~/Projects/coco-shelf/nitros9
```

Build from this repository root:

```sh
make
```

Set `NITROS9DIR` to the root of the NitrOS-9 checkout if it is not beside
this repository. `CMOC_OS9` defaults to the sibling `cmoc_os9` checkout next to
`NITROS9DIR`.

Create a bootable CoCo 3 NitrOS-9 disk image from this repository:

```sh
make disk
```

That builds (if needed) the minimal Level 2 floppy recipe at
`$(NITROS9DIR)/recipes/coco3/floppy` (`MINIMAL=1`, i.e. `l2_coco3_minimal.dsk`)
and copies it into this repository as `roguec.dsk`. The minimal recipe is used
because the full `coco3` recipe disk has almost no free space left, which
would silently truncate the files copied on below. `roguec` and the
`rogue.dat`/`rogue.hlp`/`rogue.chr`/`rogue.scr` data files are then copied
onto the image with the NitrOS-9 `os9` toolshed utility: the binary goes into
`CMDS`, and the data files go into a `ROGUE` subdirectory, matching the paths
(`ROGUE/rogue.dat`, etc.) the reconstructed C code opens at runtime. Set
`LEVEL` to match the NitrOS-9 level you want built (default `2`), and
`FLOPPY_MINIMAL=0` to use the full recipe instead (not recommended, see
above). The `os9` toolshed binary must be on your `PATH` (or set `OS9` to its
location).

Launch the generated disk in MAME:

```sh
make run
```

Current status:

- `epyx_offsets.h` names the first confirmed data-area offsets.
- `epyx_arena.c` loads `rogue.dat` and exposes typed accessors.
- `main.c` loads the arena and clears the command-line buffer at `$1528`;
  full option parsing is still pending translation.
- `epyx_screen.c` names the first translated terminal primitives from the
  `L63DD`/`L6CDE`/`L6D07`/`L6D6F` cluster.
- `epyx_format.c` translates the compact `L3D23` formatter subset and exposes
  the first readable wrappers for `L6D16`/`L68D8`-style output.
- `rogue_game.c` is the current playable harness. It draws a minimal room,
  moves the hero with `hjklyubn`, shows a Rogue-style status line, handles
  `?` and `/` using the original Epyx `rogue.hlp` and `rogue.chr` files, and
  uses confirmed `rogue.dat` offsets directly for inventory text and object
  names. This is not the translated game loop yet; it is a runnable target for
  testing display, input, data loading, and binary size while the real routines
  are translated.

Next translation targets:

1. Startup flow at `rogue.asm:L405F`.
2. Screen/status helpers around `L63DD`, `L6C95`, and `L6D07`.
3. New-game initialization calls at `L40A0`.
4. Command loop at `L418E`, replacing the temporary harness in `rogue_game.c`.

Keep functions named by behavior when known, and include the original assembly
label in comments while the translation is being verified.
