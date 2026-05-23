# Epyx Rogue C Port Roadmap

This file is the working handoff plan for `roguec`, the readable C port of
Epyx Rogue for the CoCo 3. It is meant to be the first thing to read when a new
Codex session resumes work.

## Current State

- Working directory: `/Users/boisy/Projects/coco-shelf/cmoc_os9`
- Main source: `utils/rogue_epyx_c/rogue_game.c`
- Epyx references:
  - Assembly: `/Users/boisy/Projects/coco-shelf/nitros9/3rdparty/packages/rogue/rogue.asm`
  - Data: `/Users/boisy/Projects/coco-shelf/nitros9/3rdparty/packages/rogue/rogue.dat`
  - Help/symbol files: `rogue.hlp`, `rogue.chr`
  - Score file: `rogue.scr`
- Build command:

```sh
cd /Users/boisy/Projects/coco-shelf/cmoc_os9/utils/rogue_epyx_c
make clean && make && os9 ident roguec
```

Last known build:

```text
roguec:       Module size $76A3 / 30371, data size $1B48 / 6984
roguec-small: Module size $647B / 25723, data size $12C0 / 4800
roguec-heap:  Module size $76A8 / 30376, data size $1B48 / 6984
```

Only expected warning is the existing builtin macro warning from cmoc/clang:

```text
warning: undefining builtin macro [-Wbuiltin-macro-redefined]
```

## Implemented So Far

- Loads Epyx `rogue.dat` into the arena and uses many data-backed strings.
- Uses Epyx screen/status format strings from `rogue.dat`.
- Startup title flow with Epyx-style corner asterisk animation.
- Player name prompt and greeting.
- Basic terminal option save/restore using `_os_gs_popt` / `_os_ss_popt`.
- Basic two-room discovered-map model:
  - player glyph from Epyx glyph table: `8`
  - floor/corridor/wall/door/stairs glyphs from `rogue.dat` table at `$35A6`
  - door glyph `/`
  - stairs glyph `=`
  - corridor reveal with `#`
  - no full-room redraw on ordinary player movement
- Basic inventory and object pickup/drop.
- Basic food, gold, potion, scroll, weapon, armor support.
- Weapon/armor names read from Epyx tables.
- Basic kobold monster and deterministic combat.
- Death screen with Epyx-style border/tombstone/rankings prompt.
- Hall of Fame display after death, using `rogue.scr`, with basic top-10 score
  insertion and writeback.
- Heap-arena build by default, keeping Epyx-style `rogue.dat` external instead
  of consuming static process data.
- `roguec`, `roguec-small`, and `roguec-heap` recipe binaries.
- Runtime support files live in `ROGUE/`: `rogue.dat`, `rogue.hlp`,
  `rogue.chr`, and `rogue.scr`.

## Current Design Constraints

- Keep source readable C. Do not turn this into a thin assembly wrapper.
- Prefer Epyx `rogue.dat` strings/tables over hardcoded text.
- Check `rogue.asm` before implementing behavior that Epyx already has.
- Watch code size aggressively. Every new feature should report module size.
- Keep build products out of intentional commits unless the project already
  wants them. `rogue.scr` is now copied by the local Makefile and should be
  included on the runtime disk alongside `rogue.dat`, `rogue.hlp`, and
  `rogue.chr`.

## High-Level Goal

The end goal is not a clone from scratch. The end goal is a readable C
reconstruction of the Epyx CoCo 3 Rogue behavior that can be built with cmoc and
run under OS-9.

## Next Feature Sequence

### 1. Continue Rogue.dat String Migration

This is the next preferred pass. Search for embedded user-facing C strings that
already exist in `rogue.dat`, add named offsets in `epyx_offsets.h`, and use
`rogue_string_at()` instead of duplicate literals.

Recent example:

- `SPACE to continue ESC to quit` now comes from `rogue.dat` offset `$2AA4`.
- Full build size changed from 30398 to 30371 bytes, saving 27 bytes.
- Small build was unchanged because help is compiled out there.

Suggested steps:

- Use `rg '"[^"]*[A-Za-z][^"]*"' utils/rogue_epyx_c/*.c` to find remaining
  embedded user-facing strings.
- Check `rogue.asm` comments and `rogue.dat` bytes for matching source strings.
- Prefer existing Epyx text even when capitalization or spacing differs.
- Build after each small batch and compare `roguec`, `roguec-small`, and
  `roguec-heap` sizes against the previous build.

Known candidates to investigate:

- `Pack full.`
- `The armor absorbs the hit.`
- `Loading...`
- simplified object/action/error messages added during recent feature work

### 2. Inventory Formatting Alignment

Epyx object formatter is in `rogue.asm` around the object description routines
near `L72B6` and related code.

Current C inventory display is simplified:

```text
a) some item
```

Epyx inventory includes richer state such as:

- pack letters
- quantities
- weapon in hand
- armor being worn
- identified/unknown object names
- charges for wands/staves
- armor class details

Do a conservative pass first:

- Show quantities for stacked objects.
- Show `(weapon in hand)` / worn armor phrasing using Epyx strings if available.
- Keep the screen paging behavior intact.

### 3. Score File Persistence

Implemented in a conservative first pass.

Epyx score handling:

- `rogue.asm` around `L0434` through `L06AF`
- score file name string at `OFF_SCORE_FILE_NAME` (`rogue.scr`)
- entry size: 43 bytes
- seeded file entries include Romar and Shelly

Implemented:

- Insert current score into top 10.
- Write updated `rogue.scr` back to disk.
- Preserve existing Epyx file format.

Also implemented Epyx's interactive missing-score-file prompt flow:
`No scorefile: Create Retry Abort (C,R or A) ?`

### 4. Full Object Taxonomy Skeleton

Implemented in a conservative first pass.

Add object kinds before adding all effects:

- ring (`o`)
- wand/staff (`!`)
- trap (`"`)
- Amulet (`&`)

Use the Epyx glyph table at `$35A6`; offsets already include
`OFF_RING_TABLE`, `OFF_WAND_TABLE`, `OFF_RING_STONE_PTRS`,
`OFF_WAND_MATERIAL_PTRS`, etc.

First version should make them visible, pick-up-able, listable, and droppable.
Effects can follow.

### 5. Ring Commands

Implemented in a conservative first pass.

Epyx routines:

- `P` put on ring: around `L94E3`
- `R` remove ring: around `L9573`

Add:

- left/right ring slots
- `P` and `R` commands
- messages from `rogue.dat`
- minimal effects first:
  - add strength
  - see invisible
  - aggravate monster

### Later Adaptation: Direct OS-9 Data-Area Load

The original Epyx CoCo 3 Rogue loads `rogue.dat` at process data offset `$0000`,
which is effectively the base of the OS-9 process data area. That means fixed
addresses in `rogue.asm` are usable directly as data-area offsets.

The current C port mirrors this with `rogue_arena[]` and helper macros such as
`rogue_get8()` and `rogue_string_at()`. A later experiment could adapt the C
port to load `rogue.dat` into the process data area the same way Epyx does, so
offset constants could be used directly as pointers or lvalues.

Possible benefits:

- Better match with the original Epyx memory model.
- Removal or simplification of `rogue_arena[]` helper macros.
- Possible modest module-size and performance wins from avoiding explicit
  arena-base indexing.

Risks and checks:

- C treats address `0` specially as the null pointer, so confirm CMOC/OS-9
  code generation is safe before committing to this model.
- Ensure the module still reserves enough process data memory after removing
  the global arena.
- Compare `os9 ident roguec` module/data sizes before and after.
- Inspect generated assembly for common byte/string accesses to verify whether
  the expected simplification actually appears.

### 6. Throw Command

Implemented in a conservative first pass.

Epyx throw routine starts around `L8745`.

Added `t` command:

- choose item
- ask direction
- hit monster if present
- drop missed object at the last open square

Still to revisit later:

- Epyx projectile animation timing
- exact Epyx drop/vanish behavior
- potion throw effects

### 7. Wand/Staff and Zap

Implemented in a conservative first pass.

Epyx zap routine starts around `L7D91`.

Added `z` command with shared direction prompting and these minimal effects:

- light
- magic missile
- striking

Still to revisit later:

- polymorph
- teleport away/to
- cancellation
- haste/slow monster
- lightning/fire/cold bolts
- drain life

### 8. Search, Traps, Hidden Doors

Implemented in a conservative first pass for traps/search/hidden doors.

Epyx has trap and hidden-door behavior scattered around map/search routines.

Added:

- trap placement
- trap discovery
- `I` identify trap type
- `s` search
- hidden doors

Hidden doors now appear in the generated dungeon and are revealed by searching.

### 9. Real Dungeon Generator

Implemented in a conservative first pass.

Epyx generator includes:

- 9 room blocks
- variable room size/position
- passages between rooms
- hidden doors
- traps
- stairs
- per-room visibility/discovery

The current generator now uses a 3x3 room grid, variable rooms, connected
passages, hidden doors, stairs, generated object positions, and map discovery.
Still to revisit later: closer Epyx room-connectivity probabilities and richer
per-room object population.

### 11. Monster Definition Alignment

Implemented in a conservative first pass.

Epyx monster generation uses the two level-based monster strings at `$1FE6` and
`$2001`, then initializes each monster from the 18-byte monster definition
table at `$10FB`.

Added:

- a larger active monster pool
- monster HP rolled from the Epyx monster table level byte
- actual monster/trap death cause tracking for the death and score screens

Still to revisit later:

- exact monster attack dice, armor, flags, and special attacks
- exact monster wake/chase behavior
- monster carried-object generation

### 10. Command Completeness

Implemented in a conservative first pass:

- `S` save
- restore-from-save startup path
- `c` call/rename object
- `D` discovered items
- `I` identify trap type
- `s` search
- `v` version
- `a` repeat last command
- `CTRL-r` repeat last message
- `F`, shifted movement, and `f` fast mode
- `m` / `M` macro execute/define

Still to revisit later:

- exact Epyx save-file layout
- exact Epyx macro editor behavior

## Known Simplifications To Revisit

- Combat still uses simplified hit/damage behavior.
- Monster flags, carried objects, special attacks, and wake/chase rules are
  still simplified.
- Potion colors still use a C string table instead of Epyx assigned pointer
  tables.
- Scroll names are generated in C but should continue to track Epyx behavior.
- Dungeon generation is first-pass 3x3 Rogue-style generation, not yet exact
  Epyx generation.
- `Pack full.` and `The armor absorbs the hit.` are still hardcoded.
- `Loading...` is intentionally non-Epyx text but acceptable for now because the
  Epyx pause likely hides loading.

## Size Profiles

The normal `roguec` binary remains the full feature build. The
`utils/rogue_epyx_c` Makefile also builds `roguec-small` with `ROGUE_SMALL`
defined, so a plain `make` produces both binaries for side-by-side size checks.

The high-score display path is currently disabled in both binaries. The small
profile is an architectural size experiment. It keeps the playable core but
removes additional rarely used or non-core systems from the binary:

- save/restore
- help/character help preloading and display
- called-name storage and `c` command
- discovered-items display
- macros
- version command

Latest measured sizes:

- full `roguec`: module `$76A3` / 30371, data `$1B48` / 6984
- small `roguec-small`: module `$647B` / 25723, data `$12C0` / 4800
- explicit heap `roguec-heap`: module `$76A8` / 30376, data `$1B48` / 6984

## Useful Offsets Already Defined

See `epyx_offsets.h`. Important ones:

- `OFF_ASCII_GLYPH_TABLE` = `$35A6`
- `OFF_WEAPON_NAME_TABLE` = `$004B`
- `OFF_ARMOR_NAME_TABLE` = `$00C7`
- `OFF_SCROLL_TABLE` = `$0156`
- `OFF_POTION_TABLE` = `$0262`
- `OFF_RING_TABLE` = `$034C`
- `OFF_WAND_TABLE` = `$043C`
- `OFF_RANK_NAME_TABLE` = `$0504`
- `OFF_SCORE_FILE_NAME` = `$14C8`
- `OFF_SCORE_HALL_HEADER` = `$19E1`
- `OFF_SCORE_GOLD_HEADER` = `$19FD`
- `OFF_SCORE_ROW_FORMAT` = `$1A07`

## Handoff Advice

When resuming:

1. Build first and record size.
2. Read this file.
3. Inspect `git diff -- utils/rogue_epyx_c`.
4. Continue with "Rogue.dat String Migration" unless Boisy asks otherwise.
5. After every feature, run:

```sh
make clean && make && os9 ident roguec && os9 ident roguec-small && os9 ident roguec-heap
```

6. Report module size and data size.
