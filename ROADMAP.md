# Epyx Rogue C Port Roadmap

This file is the working handoff plan for `roguec`, the readable C port of
Epyx Rogue for the CoCo 3. It is meant to be the first thing to read when a new
Codex session resumes work.

## Current State

- Working directory: `/Users/boisy/Projects/epyx-rogue-c-reconstruct`
- Main source: `rogue_game.c`
- Epyx references:
  - Assembly: `/Users/boisy/Projects/coco-shelf/nitros9/3rdparty/packages/rogue/rogue.asm`
  - Data: `/Users/boisy/Projects/coco-shelf/nitros9/3rdparty/packages/rogue/rogue.dat`
  - Help/symbol files: `rogue.hlp`, `rogue.chr`
  - Score file: `rogue.scr`
- Build command:

```sh
cd /Users/boisy/Projects/epyx-rogue-c-reconstruct
make clean && make && os9 ident roguec
```

Last known build:

```text
roguec:       Module size $78F4 / 30964, data size $1B0D / 6925
roguec-small: Module size $6965 / 26981, data size $1285 / 4741
roguec-heap:  Module size $78F9 / 30969, data size $1B0D / 6925
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
  The full heap build should stay below `32768 - 512` (`32256`) bytes; a
  32629-byte build could no longer load `ROGUE/rogue.dat` reliably under OS-9.
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

Recent examples:

- `SPACE to continue ESC to quit` now comes from `rogue.dat` offset `$2AA4`.
- Restore greeting now comes from `rogue.dat` offset `$16EB`.
- Macro prompt now comes from `rogue.dat` offset `$2C16`.
- Wand/Staff wall miss now reuses the Epyx `nothing happens` string at `$3EC2`.
- Amulet name and armor-class inventory format now use Epyx strings at `$3A82`
  and `$3A68`.
- Inventory-empty, discovered-empty, and top-line `Cont` text now use Epyx
  strings at `$38E3`, `$3C87`, and `$3659`.
- Latest pass changed full `roguec` from 30216 to 30130 bytes, saving 86
  bytes. `roguec-small` changed from 25568 to 25489 bytes, saving 79 bytes.
- A follow-up duplicate-string audit identified Epyx object formatter fragments
  at `$399F` through `$3B02`, but direct use of the fragments increased code
  size because CMOC generated more access/formatting code than the literals
  cost. Keep those offsets named for a later exact object-formatter rewrite,
  not for piecemeal substitution.
- Collapsing repeated local literals such as `Cancelled.` and
  `ROGUE/rogue.scr` into named C arrays also increased module size; leave those
  as literals unless a broader string-table strategy replaces them.

Suggested steps:

- Use `rg '"[^"]*[A-Za-z][^"]*"' *.c` to find remaining
  embedded user-facing strings.
- Check `rogue.asm` comments and `rogue.dat` bytes for matching source strings.
- Prefer existing Epyx text even when capitalization or spacing differs.
- Build after each small batch and compare `roguec`, `roguec-small`, and
  `roguec-heap` sizes against the previous build.

Known candidates to investigate:

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
- monster melee damage now rolls the Epyx damage strings from the monster
  definition table instead of doing flat one-point damage
- monster attacks now use a compact d20-style hit chance based on monster table
  level and worn armor instead of always hitting
- Aquator armor rust is implemented for successful attacks:
  - `your armor weakens, oh my!` comes from `rogue.dat` offset `$2105`
  - `the rust vanishes instantly` comes from `rogue.dat` offset `$20E9`
  - zero-damage monster attacks no longer get forced to one HP damage
- Nymph item theft is implemented for successful attacks:
  - `she stole %s!` comes from `rogue.dat` offset `$21A1`
  - worn, wielded, and hand-worn ring items are not stolen
  - plain food and plain equipment are skipped
  - one item is stolen from stacks, and the Nymph disappears after stealing
- Rattlesnake strength poison is implemented for successful attacks:
  - `you feel a bite in your leg and now feel weaker` comes from `$2120`
  - `a bite momentarily weakens you` comes from `$2150`
  - Sustain Strength ring subtype 2 prevents the drain
  - a compact 50% bite-effect check stands in for Epyx's fuller protection
    saving-throw helper
- Ice Monster freeze is implemented as a silent no-damage special:
  - successful Ice Monster attacks set a short frozen-turn counter
  - while frozen, player input burns turns and monsters continue acting
- Leprechaun gold theft is implemented for successful attacks:
  - `your purse feels lighter` comes from `rogue.dat` offset `$2188`
  - stolen gold is clamped at zero and the status line is updated
  - the Leprechaun disappears after stealing
- Venus Flytrap hold is implemented in a compact first pass:
  - successful Flytrap attacks hold the player in place
  - movement burns turns unless attacking the adjacent Flytrap
  - killing a Flytrap clears the hold
- Wraith/Vampire drain is implemented for successful attacks:
  - `you suddenly feel weaker` comes from `rogue.dat` offset `$216F`
  - Vampire has a 30% drain chance; Wraith has a 15% drain chance
  - drain reduces current and maximum hit points
- Defeated monsters use a compact carried-object drop chance:
  - killed monsters can leave food, gold, potions, scrolls, or weapons
  - disappearing thieves do not drop carried objects through this path
- Troll regeneration is implemented as a compact monster-turn behavior.

Still to revisit later:

- exact monster rank/armor calculation, flags, and remaining special attacks
- exact Epyx monster flags, sleep/wake probabilities, and pathing details
- exact Epyx mcarried object allocation, subtype weighting, and drop tables
- compact wake/chase and Epyx mcarried probability were prototyped, but backed
  out because the full heap binary grew to 32629 bytes and crossed the current
  practical size ceiling for loading `ROGUE/rogue.dat`

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

- Combat now uses Epyx monster damage strings and a first-pass monster hit
  chance; Aquator armor rust, Nymph theft, Rattlesnake strength poison, and
  Ice Monster freeze, Leprechaun gold theft, Venus Flytrap hold, Wraith/Vampire
  drain, compact monster drops, and Troll regeneration are implemented. Player
  attack, exact Epyx rank/armor math, and some monster specials are still
  simplified.
- Monster carried objects and wake/chase rules are still simplified. Monsters
  currently move only while visible, which keeps the full heap binary below the
  current `32768 - 512` safety threshold.
- Potion colors still use a C string table. The Epyx assembly has the color
  names near the program-side pointer table, but they are not plain initialized
  strings in the 24K `rogue.dat` image currently loaded by this port.
- Scroll names are generated in C but should continue to track Epyx behavior.
- Dungeon generation is first-pass 3x3 Rogue-style generation, not yet exact
  Epyx generation.
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

- full `roguec`: module `$78F4` / 30964, data `$1B0D` / 6925
- small `roguec-small`: module `$6965` / 26981, data `$1285` / 4741
- explicit heap `roguec-heap`: module `$78F9` / 30969, data `$1B0D` / 6925
- static arena `roguec-static`: module `$75B3` / 30131, data `$7B03` / 31491

Latest size comparison against the previous working baseline:

- full `roguec`: 32086 -> 30964, saving 1122 bytes and leaving 1292 bytes
  under the suspected `32256` ceiling
- small `roguec-small`: 27295 -> 26981, saving 314 bytes
- explicit heap `roguec-heap`: 32091 -> 30969, saving 1122 bytes

Size-preserving-feature optimization notes:

- CMOC `-fomit-frame-pointer` saved 44 bytes in the full heap build.
- Packing save/restore scalar globals into one save-state block saved the
  largest chunk without removing save/restore functionality.
- Consolidating screen-control writes, combat result messages, and inventory
  bonus formatting all reduced code size under CMOC.
- Tested but rejected: `-O1`, direct inventory row output, fixed trap-count
  bounds, and shared worn-ring subtype checks; each increased module size.

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
