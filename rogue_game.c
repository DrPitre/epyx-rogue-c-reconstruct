#include <os.h>
#include <os9abi.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>

#include "epyx_arena.h"
#include "epyx_format.h"
#include "epyx_screen.h"
#include "rogue_game.h"

int read(int path, void *buffer, int count);
int rogue_ignore_signals();

static void redraw_dungeon();
static void show_inventory();
static int draw_inventory_lines(int kind);
static void wait_for_inventory_continue();
#ifndef ROGUE_SMALL
static void show_hall_of_fame();
#endif
static void pickup_here();
static int read_key();
static int starts_with_vowel(const char *s);
static const char *monster_bare_name(int ch);
static const char *monster_death_cause_for(int ch);
static int visible_position(int x, int y);
static int overlay_visible_position(int x, int y);
static int movement_delta(int ch);
static int command(int ch);

#define DUNGEON_MIN_WIDTH  28
#define DUNGEON_MAX_WIDTH  78
#define DUNGEON_MIN_HEIGHT 10
#define DUNGEON_MAX_HEIGHT 18
#define MAP_MAX_WIDTH  80
#define MAP_MAX_HEIGHT 24
#define MAP_CELLS      (MAP_MAX_WIDTH * MAP_MAX_HEIGHT)
#define ROOM_COUNT     9
#define ROOM_COLS      3
#define ROOM_ROWS      3
#define FLOOR_OBJECTS  20
#define MONSTER_MAX    18
#define INVENTORY_MAX  8
#define NO_ITEM_INDEX  127
#define HELP_TEXT_MAX  1024
#define CHR_TEXT_MAX   192
#define SCORE_ENTRY_SIZE 43
#define SCORE_ENTRY_MAX  10
#define SAVE_MAGIC_SIZE  8
#define CALLED_NAME_MAX  16
#define CALLED_KIND_MAX  4
#define CALLED_ITEM_MAX  15
#define MACRO_MAX        64
#define KEY_CTRL_C     3
#define KEY_CTRL_E     5
#define KEY_CTRL_R     18
#define KEY_ESCAPE     27
#define KEY_RETURN     13

#ifndef ROGUE_SMALL
#define ROGUE_WITH_CALLS 1
#define ROGUE_WITH_DISCOVERED 1
#define ROGUE_WITH_HELP 1
#define ROGUE_WITH_MACROS 1
#define ROGUE_WITH_SAVE 1
#define ROGUE_WITH_SCORES 0
#define ROGUE_WITH_VERSION 1
#else
#define ROGUE_WITH_CALLS 0
#define ROGUE_WITH_DISCOVERED 0
#define ROGUE_WITH_HELP 0
#define ROGUE_WITH_MACROS 0
#define ROGUE_WITH_SAVE 0
#define ROGUE_WITH_SCORES 0
#define ROGUE_WITH_VERSION 0
#endif

#ifndef ROGUE_HEAP_MAPS
#define ROGUE_HEAP_MAPS 0
#endif

#define GLYPH_PLAYER   0
#define GLYPH_FLOOR    1
#define GLYPH_HALLWAY  2
#define GLYPH_HORIZ    3
#define GLYPH_VERT     4
#define GLYPH_CORNER1  5
#define GLYPH_DOOR     9
#define GLYPH_GOLD     10
#define GLYPH_FOOD     11
#define GLYPH_SCROLL   12
#define GLYPH_POTION   13
#define GLYPH_WEAPON   14
#define GLYPH_ARMOR    15
#define GLYPH_RING     16
#define GLYPH_WAND     17
#define GLYPH_STAIRS   18

#define glyph(index) rogue_get8(OFF_ASCII_GLYPH_TABLE + (index))

#define OBJ_NONE       0
#define OBJ_FOOD       1
#define OBJ_GOLD       2
#define OBJ_POTION     3
#define OBJ_SCROLL     4
#define OBJ_WEAPON     5
#define OBJ_ARMOR      6
#define OBJ_RING       7
#define OBJ_WAND       8
#define OBJ_TRAP       9
#define OBJ_AMULET     10

#define POTION_HEALING 5
#define SCROLL_MAGIC_MAPPING 1
#define WAND_LIGHT 0
#define WAND_STRIKING 1
#define WAND_MAGIC_MISSILE 6

#define TILE_NOTHING     0
#define TILE_FLOOR       1
#define TILE_HALL        2
#define TILE_HORIZ       3
#define TILE_VERT        4
#define TILE_CORNER      5
#define TILE_DOOR        6
#define TILE_SECRET_DOOR 7
#define TILE_STAIRS      8

typedef struct rogue_object {
  char kind;
  char glyph;
  char subtype;
  char x;
  char y;
  char quantity;
  char known;
} RogueObject;

typedef struct rogue_monster {
  char x;
  char y;
  char hp;
  char type;
} RogueMonster;

typedef struct rogue_room {
  char x;
  char y;
  char w;
  char h;
  char visible;
} RogueRoom;

typedef struct inventory_item {
  char kind;
  char glyph;
  char subtype;
  char quantity;
  char hit_bonus;
  char damage_bonus;
  char armor_class;
} InventoryItem;

typedef struct game_save_state {
  int hero_x;
  int hero_y;
  int player_gold;
  char dungeon_level;
  char player_hp;
  char player_max_hp;
  char player_strength;
  char wielded_weapon_index;
  char worn_armor_index;
  char left_ring_index;
  char right_ring_index;
  char ring_strength_bonus;
  char see_invisible;
  char spawn_clock;
  char frozen_turns;
  char flytrap_hold;
  char inventory_count;
  char dungeon_x;
  char dungeon_y;
  char dungeon_width;
  char dungeon_height;
  char status_y;
  char stair_x;
  char stair_y;
} GameSaveState;

static void apply_ring_effect();
static RogueObject *object_at();
static RogueMonster *monster_at();
static void draw_floor_objects();
static void draw_monsters();
static void draw_map_row();
static void remove_item();
static const char *inventory_object_name();

static int hero_x;
static int hero_y;
static char dungeon_level;
static int player_gold;
static char player_hp;
static char player_max_hp;
static char player_strength;
static char wielded_weapon_index;
static char worn_armor_index;
static char left_ring_index;
static char right_ring_index;
static char ring_strength_bonus;
static char see_invisible;
static char turn_taken;
static char inventory_count;
static char dungeon_x;
static char dungeon_y;
static char dungeon_width;
static char dungeon_height;
static char status_y;
static char stair_x;
static char stair_y;
static char status_invalid;
static char clear_message_next;
static char last_command;
static char reveal_draw_enabled;
static char next_fast_move;
static char persistent_fast_mode;
static char go_over_object;
static char spawn_clock;
static char frozen_turns;
static char flytrap_hold;
static char death_cause;
static char last_status_level;
static char last_status_hp;
static int last_status_gold;
static char last_status_armor;
static RogueObject floor_objects[FLOOR_OBJECTS];
static InventoryItem inventory[INVENTORY_MAX];
static RogueMonster monsters[MONSTER_MAX];
static RogueRoom rooms[ROOM_COUNT];
#if ROGUE_HEAP_MAPS
static char *map_tiles;
static char *map_known;
static char *map_line;
#else
static char map_tiles[MAP_MAX_HEIGHT][MAP_MAX_WIDTH];
static char map_known[MAP_MAX_HEIGHT][MAP_MAX_WIDTH];
static char map_line[MAP_MAX_WIDTH + 1];
#endif
static char object_name_buf[48];
static char death_cause_buf[32];
static char *kobold_name;
static char potion_color_index[POTION_COUNT];
#if ROGUE_WITH_SCORES
static char score_data[SCORE_ENTRY_SIZE * SCORE_ENTRY_MAX];
#endif
static struct sgbuf saved_stdin_opts;
static struct sgbuf game_stdin_opts;
static int have_saved_stdin_opts;
#if ROGUE_WITH_HELP
static char rogue_chr_text[CHR_TEXT_MAX];
static char rogue_help_text[HELP_TEXT_MAX];
#endif
#if ROGUE_WITH_CALLS
static char called_names[CALLED_KIND_MAX][CALLED_ITEM_MAX][CALLED_NAME_MAX];
#endif

static const char save_magic[SAVE_MAGIC_SIZE] = {
  'R', 'O', 'G', 'C', 'S', 'A', 'V', '8'
};

static const char potion_color_text[] =
  "amber\0aquamarine\0black\0blue\0brown\0"
  "clear\0crimson\0cyan\0gold\0green\0"
  "grey\0magenta\0orange\0pink\0plaid\0"
  "purple\0red\0silver\0tan\0tangerine\0"
  "turquoise\0vermilion\0violet\0white\0yellow";

static const unsigned char potion_color_offsets[] = {
  0, 6, 17, 23, 28,
  34, 40, 48, 53, 58,
  64, 69, 77, 84, 89,
  95, 102, 106, 113, 117,
  127, 137, 147, 154, 160
};

static const unsigned int trap_name_offsets[] = {
  OFF_TRAP_TRAPDOOR, OFF_TRAP_ARROW, OFF_TRAP_SLEEPING_GAS,
  OFF_TRAP_BEARTRAP, OFF_TRAP_TELEPORT, OFF_TRAP_POISON_DART
};

static const unsigned int trap_message_offsets[] = {
  OFF_TRAPDOOR_MESSAGE, OFF_ARROW_TRAP_MESSAGE, OFF_SLEEPING_GAS_MESSAGE,
  OFF_BEARTRAP_MESSAGE, OFF_TELEPORT_TRAP_MESSAGE, OFF_DART_TRAP_MESSAGE
};

#if ROGUE_HEAP_MAPS
#define map_index(x, y) ((int) (y) * MAP_MAX_WIDTH + (x))
#define map_tile(x, y) map_tiles[map_index(x, y)]
#define map_seen(x, y) map_known[map_index(x, y)]

static int init_map_storage()
{
  if (!map_tiles) map_tiles = (char *) malloc(MAP_CELLS);
  if (!map_known) map_known = (char *) malloc(MAP_CELLS);
  if (!map_line) map_line = (char *) malloc(MAP_MAX_WIDTH + 1);
  return map_tiles && map_known && map_line;
}
#else
#define map_tile(x, y) map_tiles[(y)][(x)]
#define map_seen(x, y) map_known[(y)][(x)]
#endif

static void seed_random()
{
  _os_time t;
  unsigned seed;

  seed = 1;
  if (_os_getime(&t) == 0) {
    seed = t.seconds;
    seed = seed * 60 + t.minutes;
    seed = seed * 24 + t.hours;
    seed = seed * 31 + t.day;
    seed = seed * 12 + t.month;
    seed += t.year;
  }
  srand(seed);
}

static int random_range(limit)
int limit;
{
  if (limit <= 1) return 0;
  return rand() % limit;
}

static int random_monster_type(table)
int table;
{
  char *choices;
  int index;
  int ch;

  choices = rogue_string_at(table ? OFF_MONSTER_LEVEL_TABLE2 :
                            OFF_MONSTER_LEVEL_TABLE1);
  while (1) {
    index = random_range(5) + random_range(6) - 5 + dungeon_level;
    if (index < 1) index = 1 + random_range(6);
    if (index > 26) index = 22 + random_range(5);
    ch = choices[index - 1];
    if (ch != ' ') return ch;
  }
}

static int monster_table_byte(ch, offset)
int ch;
int offset;
{
  if (ch < 'A' || ch > 'Z') ch = 'K';
  return rogue_get8(OFF_MONSTER_TABLE +
                    (ch - 'A') * MONSTER_ENTRY_SIZE + offset);
}

static int monster_initial_hp(ch)
int ch;
{
  int level;
  int hp;
  int i;

  level = monster_table_byte(ch, 10);
  if (dungeon_level > 26) level += dungeon_level - 26;
  if (level < 1) level = 1;

  hp = 0;
  for (i = 0; i < level; i++) hp += 1 + random_range(8);
  return hp;
}

static int monster_damage(ch)
int ch;
{
  char *text;
  int dice;
  int sides;
  int damage;
  int i;

  if (ch < 'A' || ch > 'Z') ch = 'K';
  text = rogue_string_at(rogue_get16(OFF_MONSTER_TABLE +
                         (ch - 'A') * MONSTER_ENTRY_SIZE + 14));
  damage = 0;
  while (*text) {
    while (*text && (*text < '0' || *text > '9')) text++;
    if (!*text) break;
    dice = 0;
    while (*text >= '0' && *text <= '9')
      dice = dice * 10 + *text++ - '0';
    if (*text != 'd') break;
    text++;
    sides = 0;
    while (*text >= '0' && *text <= '9')
      sides = sides * 10 + *text++ - '0';
    for (i = 0; i < dice; i++)
      if (sides > 0) damage += 1 + random_range(sides);
    while (*text && *text != '/') text++;
    if (*text == '/') text++;
  }
  return damage;
}

static int arena_string_len(off)
unsigned int off;
{
  int len;

  len = 0;
  while (rogue_get8(off + len)) len++;
  return len;
}

static char *arena_table_string(base, index, stride)
unsigned int base;
int index;
int stride;
{
  return rogue_string_at(rogue_get16(base + index * stride));
}

static int arena_random_char(off)
unsigned int off;
{
  return rogue_get8(off + random_range(arena_string_len(off)));
}

static void init_scroll_title(index)
int index;
{
  unsigned int off;
  int pos;
  int words;
  int syllables;

  off = OFF_RANDOM_SCROLL_NAMES + index * SCROLL_TITLE_SIZE;
  pos = 0;
  words = 2 + random_range(5);

  while (words-- > 0 && pos < SCROLL_TITLE_SIZE - 1) {
    syllables = 1 + random_range(3);
    while (syllables-- > 0 && pos < SCROLL_TITLE_SIZE - 2) {
      if (random_range(3) != 0)
        rogue_put8(off + pos++, arena_random_char(OFF_SCROLL_CONSONANTS));
      rogue_put8(off + pos++, arena_random_char(OFF_SCROLL_VOWELS));
    }
    if (words > 0 && pos < SCROLL_TITLE_SIZE - 1)
      rogue_put8(off + pos++, ' ');
  }
  rogue_put8(off + pos, 0);
}

static void init_object_names()
{
  char used[25];
  int i;
  int color;

  kobold_name = arena_table_string(OFF_MONSTER_TABLE, 'K' - 'A',
                                   MONSTER_ENTRY_SIZE);
  for (i = 0; i < 25; i++) used[i] = 0;
  for (i = 0; i < POTION_COUNT; i++) {
    do {
      color = random_range(25);
    } while (used[color]);
    used[color] = 1;
    potion_color_index[i] = (char) color;
    rogue_put8(OFF_POTION_KNOWN_FLAGS + i, 0);
  }

  for (i = 0; i < SCROLL_COUNT; i++) {
    rogue_put8(OFF_SCROLL_KNOWN_FLAGS + i, 0);
    init_scroll_title(i);
  }
}

static void terminal_game_mode()
{
  if (have_saved_stdin_opts) return;
  if (_os_gs_popt(0, &saved_stdin_opts) != 0) return;
  if (_os_gs_popt(0, &game_stdin_opts) != 0) return;

  game_stdin_opts.sg_echo = 0;
  if (_os_ss_popt(0, &game_stdin_opts) != 0) return;
  have_saved_stdin_opts = 1;
}

static void terminal_restore()
{
  if (!have_saved_stdin_opts) return;
  _os_ss_popt(0, &saved_stdin_opts);
  have_saved_stdin_opts = 0;
}

static void terminal_finish()
{
  int y;

  epyx_reverse_off();
  epyx_cursor_on();
  y = rogue_get8(OFF_SCREEN_HEIGHT) - 1;
  if (y < 0) y = 0;
  epyx_move_cursor(0, y);
  epyx_clear_to_eol();
  terminal_restore();
  epyx_write_string("\r\n");
}

static void put_at(x, y, ch)
int x;
int y;
int ch;
{
  epyx_put_at(x, y, ch);
}

static int string_width(text)
const char *text;
{
  int len;

  len = 0;
  while (text[len]) len++;
  return len;
}

static void centered_text(y, text)
int y;
const char *text;
{
  int x;

  x = (rogue_get8(OFF_SCREEN_WIDTH) - string_width(text)) / 2;
  if (x < 0) x = 0;
  epyx_move_cursor(x, y);
  epyx_write_string(text);
}

static void short_delay()
{
  int ticks;

  ticks = 3;
  _os9_sleep(&ticks);
}

static void show_corner_stars()
{
  int max_x;
  int max_y;
  int half_y;
  int step_x;
  int y;
  int left_x;
  int right_x;

  epyx_clear_window();
  max_x = rogue_get8(OFF_SCREEN_MAX_X);
  max_y = rogue_get8(OFF_SCREEN_MAX_Y);
  half_y = max_y / 2;
  step_x = 2;
  if (max_y) step_x = rogue_get8(OFF_SCREEN_WIDTH) / max_y;
  if (step_x < 1) step_x = 1;

  left_x = 1;
  right_x = max_x - 1;
  for (y = 0; y < half_y + 1; y++) {
    put_at(left_x, y, '*');
    put_at(right_x, y, '*');
    put_at(left_x, max_y - y, '*');
    put_at(right_x, max_y - y, '*');
    short_delay();
    left_x += step_x;
    right_x -= step_x;
  }
}

static void show_title_screen()
{
  int max_y;
  int half_y;

  show_corner_stars();
  max_y = rogue_get8(OFF_SCREEN_MAX_Y);
  half_y = max_y / 2;
  epyx_clear_window();
  centered_text(half_y, rogue_string_at(OFF_TITLE_ROGUE));
  if (rogue_get8(OFF_SCREEN_WIDTH) >= 26)
    centered_text(max_y - 5, rogue_string_at(OFF_TITLE_COPYRIGHT));
  centered_text(max_y - 3, rogue_string_at(OFF_TITLE_PRESS_SPACE));
  while (read_key() != ' ') ;
  epyx_clear_window();
}

static const char *monster_death_cause()
{
  return monster_death_cause_for(death_cause);
}

static const char *monster_name(monster)
RogueMonster *monster;
{
  return monster_death_cause_for(monster->type);
}

static const char *monster_bare_name(ch)
int ch;
{
  if (ch >= 'A' && ch <= 'Z')
    return arena_table_string(OFF_MONSTER_TABLE, ch - 'A',
                              MONSTER_ENTRY_SIZE);
  return kobold_name;
}

static const char *monster_death_cause_for(ch)
int ch;
{
  char *name;

  if (ch == 'a') name = rogue_string_at(OFF_DEATH_ARROW);
  else if (ch == 'b') name = rogue_string_at(OFF_DEATH_BOLT);
  else if (ch == 'd') name = rogue_string_at(OFF_DEATH_DART);
  else if (ch == 's') name = rogue_string_at(OFF_DEATH_STARVATION);
  else if (ch == 'f') name = rogue_string_at(OFF_DEATH_FALL);
  else if (ch >= 'A' && ch <= 'Z')
    name = arena_table_string(OFF_MONSTER_TABLE, ch - 'A',
                              MONSTER_ENTRY_SIZE);
  else
    name = rogue_string_at(OFF_DEATH_GOD);
  epyx_format(death_cause_buf, sizeof(death_cause_buf),
              starts_with_vowel(name) ? "an %s" : "a %s", name);
  return death_cause_buf;
}

#if ROGUE_WITH_SCORES
static unsigned int score_gold(entry)
char *entry;
{
  return ((unsigned int) (unsigned char) entry[37] << 8) |
         (unsigned char) entry[38];
}

static void clear_score_data()
{
  int i;

  for (i = 0; i < SCORE_ENTRY_SIZE * SCORE_ENTRY_MAX; i++) score_data[i] = 0;
}

static int load_score_file()
{
  path_id fd;
  int count;

  clear_score_data();
  if (_os_open("ROGUE/rogue.scr", FAM_READ, &fd) != 0)
    return 0;

  count = SCORE_ENTRY_SIZE * SCORE_ENTRY_MAX;
  _os_read(fd, score_data, &count);
  _os_close(fd);
  return 1;
}

static void copy_score_entry(to, from)
char *to;
char *from;
{
  int i;

  for (i = 0; i < SCORE_ENTRY_SIZE; i++) to[i] = from[i];
}

static void make_current_score_entry(entry)
char *entry;
{
  char *name;
  int i;

  for (i = 0; i < SCORE_ENTRY_SIZE; i++) entry[i] = 0;

  name = rogue_string_at(OFF_DEFAULT_PLAYER_NAME);
  for (i = 0; i < 35 && name[i]; i++) entry[i] = name[i];

  entry[36] = 1;
  entry[37] = (char) (player_gold >> 8);
  entry[38] = (char) player_gold;
  entry[39] = 'K';
  entry[40] = dungeon_level;
}

static int insert_current_score()
{
  char current[SCORE_ENTRY_SIZE];
  unsigned int gold;
  int i;
  int j;

  gold = player_gold;
  if (gold == 0) return 0;

  make_current_score_entry(current);
  for (i = 0; i < SCORE_ENTRY_MAX; i++) {
    if (score_gold(score_data + i * SCORE_ENTRY_SIZE) == 0 ||
        gold > score_gold(score_data + i * SCORE_ENTRY_SIZE)) {
      for (j = SCORE_ENTRY_MAX - 1; j > i; j--) {
        copy_score_entry(score_data + j * SCORE_ENTRY_SIZE,
                         score_data + (j - 1) * SCORE_ENTRY_SIZE);
      }
      copy_score_entry(score_data + i * SCORE_ENTRY_SIZE, current);
      return 1;
    }
  }
  return 0;
}

static void write_score_file()
{
  path_id fd;
  int count;
  int entries;

  entries = 0;
  while (entries < SCORE_ENTRY_MAX &&
         score_gold(score_data + entries * SCORE_ENTRY_SIZE) != 0)
    entries++;
  if (entries == 0) return;

  _os_delete("ROGUE/rogue.scr", FAM_WRITE);
  if (_os_create("ROGUE/rogue.scr", FAM_WRITE, &fd,
                 FAP_READ | FAP_WRITE | FAP_PREAD) != 0)
    return;

  count = entries * SCORE_ENTRY_SIZE;
  _os_write(fd, score_data, &count);
  _os_close(fd);
}
#endif

#if ROGUE_WITH_SAVE
static int write_block(fd, data, size)
path_id fd;
const void *data;
int size;
{
  int count;

  count = size;
  return _os_write(fd, data, &count) == 0 && count == size;
}

static int read_block(fd, data, size)
path_id fd;
void *data;
int size;
{
  int count;

  count = size;
  return _os_read(fd, data, &count) == 0 && count == size;
}

static int save_game_state()
{
  path_id fd;
  char *path;
  GameSaveState state;

  path = rogue_string_at(OFF_SAVE_FILE_NAME);
  _os_delete(path, FAM_WRITE);
  if (_os_create(path, FAM_WRITE, &fd, FAP_READ | FAP_WRITE | FAP_PREAD) != 0)
    return 0;

  state.hero_x = hero_x;
  state.hero_y = hero_y;
  state.player_gold = player_gold;
  state.dungeon_level = dungeon_level;
  state.player_hp = player_hp;
  state.player_max_hp = player_max_hp;
  state.player_strength = player_strength;
  state.wielded_weapon_index = wielded_weapon_index;
  state.worn_armor_index = worn_armor_index;
  state.left_ring_index = left_ring_index;
  state.right_ring_index = right_ring_index;
  state.ring_strength_bonus = ring_strength_bonus;
  state.see_invisible = see_invisible;
  state.spawn_clock = spawn_clock;
  state.frozen_turns = frozen_turns;
  state.flytrap_hold = flytrap_hold;
  state.inventory_count = inventory_count;
  state.dungeon_x = dungeon_x;
  state.dungeon_y = dungeon_y;
  state.dungeon_width = dungeon_width;
  state.dungeon_height = dungeon_height;
  state.status_y = status_y;
  state.stair_x = stair_x;
  state.stair_y = stair_y;

  if (!write_block(fd, save_magic, SAVE_MAGIC_SIZE) ||
      !write_block(fd, rogue_arena, ROGUE_ARENA_SIZE) ||
      !write_block(fd, &state, sizeof(state)) ||
      !write_block(fd, floor_objects, sizeof(floor_objects)) ||
      !write_block(fd, inventory, sizeof(inventory)) ||
      !write_block(fd, monsters, sizeof(monsters)) ||
      !write_block(fd, rooms, sizeof(rooms)) ||
#if ROGUE_HEAP_MAPS
      !write_block(fd, map_tiles, MAP_CELLS) ||
      !write_block(fd, map_known, MAP_CELLS) ||
#else
      !write_block(fd, map_tiles, sizeof(map_tiles)) ||
      !write_block(fd, map_known, sizeof(map_known)) ||
#endif
      !write_block(fd, called_names, sizeof(called_names))) {
    _os_close(fd);
    return 0;
  }

  _os_close(fd);
  return 1;
}

static int load_game_state()
{
  path_id fd;
  char magic[SAVE_MAGIC_SIZE];
  char *path;
  GameSaveState state;
  int i;

  path = rogue_string_at(OFF_SAVE_FILE_NAME);
  if (_os_open(path, FAM_READ, &fd) != 0) return 0;

  if (!read_block(fd, magic, SAVE_MAGIC_SIZE)) {
    _os_close(fd);
    return 0;
  }
  for (i = 0; i < SAVE_MAGIC_SIZE; i++) {
    if (magic[i] != save_magic[i]) {
      _os_close(fd);
      return 0;
    }
  }

  if (!read_block(fd, rogue_arena, ROGUE_ARENA_SIZE) ||
      !read_block(fd, &state, sizeof(state)) ||
      !read_block(fd, floor_objects, sizeof(floor_objects)) ||
      !read_block(fd, inventory, sizeof(inventory)) ||
      !read_block(fd, monsters, sizeof(monsters)) ||
      !read_block(fd, rooms, sizeof(rooms)) ||
#if ROGUE_HEAP_MAPS
      !read_block(fd, map_tiles, MAP_CELLS) ||
      !read_block(fd, map_known, MAP_CELLS) ||
#else
      !read_block(fd, map_tiles, sizeof(map_tiles)) ||
      !read_block(fd, map_known, sizeof(map_known)) ||
#endif
      !read_block(fd, called_names, sizeof(called_names))) {
    _os_close(fd);
    return 0;
  }

  _os_close(fd);
  hero_x = state.hero_x;
  hero_y = state.hero_y;
  player_gold = state.player_gold;
  dungeon_level = state.dungeon_level;
  player_hp = state.player_hp;
  player_max_hp = state.player_max_hp;
  player_strength = state.player_strength;
  wielded_weapon_index = state.wielded_weapon_index;
  worn_armor_index = state.worn_armor_index;
  left_ring_index = state.left_ring_index;
  right_ring_index = state.right_ring_index;
  ring_strength_bonus = state.ring_strength_bonus;
  see_invisible = state.see_invisible;
  spawn_clock = state.spawn_clock;
  frozen_turns = state.frozen_turns;
  flytrap_hold = state.flytrap_hold;
  inventory_count = state.inventory_count;
  dungeon_x = state.dungeon_x;
  dungeon_y = state.dungeon_y;
  dungeon_width = state.dungeon_width;
  dungeon_height = state.dungeon_height;
  status_y = state.status_y;
  stair_x = state.stair_x;
  stair_y = state.stair_y;
  return 1;
}
#endif

#if ROGUE_WITH_SCORES
static void show_score_line(y, name, rank, gold, status, level)
int y;
char *name;
int rank;
unsigned int gold;
int status;
int level;
{
  const char *rank_name;

  epyx_move_cursor(0, y);
  epyx_printf(rogue_string_at(OFF_SCORE_ROW_FORMAT), gold, name);
  if (rank > 1 && rank <= RANK_COUNT) {
    rank_name = arena_table_string(OFF_RANK_NAME_TABLE, rank - 1, 2);
    epyx_printf(rogue_string_at(OFF_SCORE_RANK_FORMAT), rank_name);
  }
  epyx_format(object_name_buf, sizeof(object_name_buf),
              rogue_string_at(OFF_SCORE_KILLED_BY_FORMAT),
              monster_death_cause_for(status));
  epyx_printf(rogue_string_at(OFF_SCORE_ON_LEVEL_FORMAT),
              object_name_buf, level);
}

static void show_hall_of_fame()
{
  char *entry;
  int i;
  int row;
  int ch;

  epyx_clear_window();
  epyx_move_cursor(0, 0);
  epyx_write_string(rogue_string_at(OFF_SCORE_HALL_HEADER));
  epyx_move_cursor(2, 2);
  epyx_write_string(rogue_string_at(OFF_SCORE_GOLD_HEADER));

  while (!load_score_file()) {
    epyx_message(rogue_string_at(OFF_NO_SCORE_FILE_PROMPT));
    ch = read_key();
    if (ch == 'a' || ch == 'A' || ch == KEY_ESCAPE || ch == KEY_CTRL_E)
      return;
    if (ch == 'c' || ch == 'C') {
      clear_score_data();
      break;
    }
  }
  if (insert_current_score()) write_score_file();

  row = 4;
  for (i = 0; i < SCORE_ENTRY_MAX && row < rogue_get8(OFF_SCREEN_HEIGHT) - 1;
       i++) {
    entry = score_data + i * SCORE_ENTRY_SIZE;
    if (score_gold(entry) == 0) break;
    show_score_line(row++, entry, (unsigned char) entry[36],
                    score_gold(entry), entry[39], entry[40]);
  }
}
#endif

static void show_death_screen()
{
  const char *prompt;
  int bottom;
  int max_x;
  int prompt_x;
  int top;
  int x;
  int y;
  epyx_clear_window();
  max_x = rogue_get8(OFF_SCREEN_MAX_X);
  top = 1;
  bottom = rogue_get8(OFF_SCREEN_HEIGHT) - 2;
  if (bottom < 14) bottom = rogue_get8(OFF_SCREEN_MAX_Y);

  put_at(1, top, '*');
  for (x = 2; x < max_x; x++) put_at(x, top, glyph(GLYPH_HORIZ));
  put_at(max_x, top, '*');
  for (y = top + 1; y < bottom; y++) {
    put_at(1, y, glyph(GLYPH_VERT));
    put_at(max_x, y, glyph(GLYPH_VERT));
  }

  y = top + (bottom - top) / 2 - 3;
  centered_text(y + 1, rogue_string_at(OFF_DEATH_TOP));
  centered_text(y + 2, rogue_string_at(OFF_DEATH_SHOULDER));
  centered_text(y + 3, rogue_string_at(OFF_DEATH_RIP));
  centered_text(y + 4, rogue_string_at(OFF_DEATH_BLANK1));
  centered_text(y + 5, rogue_string_at(OFF_DEATH_BLANK2));
  centered_text(y + 6, rogue_string_at(OFF_DEATH_BLANK3));
  epyx_format(object_name_buf, sizeof(object_name_buf),
              rogue_string_at(OFF_DEATH_EPITAPH),
              rogue_string_at(OFF_DEFAULT_PLAYER_NAME),
              monster_death_cause());
  centered_text(y + 8, object_name_buf);
  epyx_format(object_name_buf, sizeof(object_name_buf),
              rogue_string_at(OFF_DEATH_TOTAL_WORTH), player_gold);
  centered_text(y + 10, object_name_buf);

  prompt = "";
  prompt_x = max_x;
#if ROGUE_WITH_SCORES
  prompt = rogue_string_at(OFF_DEATH_RANKINGS_PROMPT);
  prompt_x = (rogue_get8(OFF_SCREEN_WIDTH) - string_width(prompt)) / 2;
  if (prompt_x < 2) prompt_x = 2;
#endif
  put_at(1, bottom, '*');
  for (x = 2; x < prompt_x; x++) put_at(x, bottom, glyph(GLYPH_HORIZ));
#if ROGUE_WITH_SCORES
  epyx_move_cursor(prompt_x, bottom);
  epyx_write_string(prompt);
  x = prompt_x + string_width(prompt);
  while (x < max_x) put_at(x++, bottom, glyph(GLYPH_HORIZ));
#endif
  put_at(max_x, bottom, '*');

#if ROGUE_WITH_SCORES
  while (read_key() != KEY_RETURN) ;
  show_hall_of_fame();
#else
  while (read_key() != KEY_RETURN) ;
#endif
}

static void init_layout()
{
  int screen_width;
  int screen_height;

  screen_width = rogue_get8(OFF_SCREEN_WIDTH);
  screen_height = rogue_get8(OFF_SCREEN_HEIGHT);

  dungeon_width = (char) (screen_width > MAP_MAX_WIDTH ? MAP_MAX_WIDTH :
                          screen_width);
  dungeon_height = (char) (screen_height > 7 ? screen_height - 3 :
                           screen_height - 1);
  if (dungeon_width > DUNGEON_MAX_WIDTH) dungeon_width = DUNGEON_MAX_WIDTH;
  if (dungeon_height > DUNGEON_MAX_HEIGHT) dungeon_height = DUNGEON_MAX_HEIGHT;
  if (dungeon_width < DUNGEON_MIN_WIDTH) dungeon_width = DUNGEON_MIN_WIDTH;
  if (dungeon_height < DUNGEON_MIN_HEIGHT) dungeon_height = DUNGEON_MIN_HEIGHT;

  dungeon_x = (char) ((screen_width - dungeon_width) / 2);
  if (dungeon_x < 1 && screen_width > dungeon_width) dungeon_x = 1;
  dungeon_y = 1;
  status_y = (char) (screen_height > 1 ? screen_height - 1 : 0);
  stair_x = 0;
  stair_y = 0;
}

static int tile_glyph(tile)
int tile;
{
  if (tile == TILE_FLOOR) return glyph(GLYPH_FLOOR);
  if (tile == TILE_HALL) return glyph(GLYPH_HALLWAY);
  if (tile == TILE_HORIZ) return glyph(GLYPH_HORIZ);
  if (tile == TILE_VERT) return glyph(GLYPH_VERT);
  if (tile == TILE_CORNER) return glyph(GLYPH_CORNER1);
  if (tile == TILE_DOOR) return glyph(GLYPH_DOOR);
  if (tile == TILE_SECRET_DOOR) return glyph(GLYPH_HORIZ);
  if (tile == TILE_STAIRS) return glyph(GLYPH_STAIRS);
  return ' ';
}

static int map_in_bounds(x, y)
int x;
int y;
{
  return x >= 0 && x < MAP_MAX_WIDTH && y >= 0 && y < MAP_MAX_HEIGHT &&
         x < rogue_get8(OFF_SCREEN_WIDTH) && y < status_y;
}

static void set_tile(x, y, tile)
int x;
int y;
int tile;
{
  if (map_in_bounds(x, y)) map_tile(x, y) = (char) tile;
}

static int walkable_tile(tile)
int tile;
{
  return tile == TILE_FLOOR || tile == TILE_HALL || tile == TILE_DOOR ||
         tile == TILE_STAIRS;
}

static int room_at_position(x, y)
int x;
int y;
{
  int i;

  for (i = 0; i < ROOM_COUNT; i++) {
    if (rooms[i].w &&
        x > rooms[i].x && x < rooms[i].x + rooms[i].w - 1 &&
        y > rooms[i].y && y < rooms[i].y + rooms[i].h - 1)
      return i;
  }
  return -1;
}

static int room_at_door(x, y)
int x;
int y;
{
  int i;

  for (i = 0; i < ROOM_COUNT; i++) {
    if (rooms[i].w &&
        x >= rooms[i].x && x < rooms[i].x + rooms[i].w &&
        y >= rooms[i].y && y < rooms[i].y + rooms[i].h)
      return i;
  }
  return -1;
}

static int hero_visibility_room()
{
  int room;

  room = room_at_position(hero_x, hero_y);
  if (room < 0 && map_in_bounds(hero_x, hero_y) &&
      map_tile(hero_x, hero_y) == TILE_DOOR)
    room = room_at_door(hero_x, hero_y);
  return room;
}

static int position_visibility_room(x, y)
int x;
int y;
{
  int room;

  room = room_at_position(x, y);
  if (room < 0 && map_in_bounds(x, y) && map_tile(x, y) == TILE_DOOR)
    room = room_at_door(x, y);
  return room;
}

static void draw_room_terrain(index)
int index;
{
  int y;
  RogueRoom *room;

  if (index < 0 || index >= ROOM_COUNT) return;
  room = &rooms[index];
  if (!room->w || !room->visible) return;
  for (y = room->y; y < room->y + room->h; y++)
    draw_map_row(y, room->x, room->x + room->w - 1);
}

static void draw_floor_object_at(x, y)
int x;
int y;
{
  RogueObject *obj;

  if (!overlay_visible_position(x, y)) return;
  obj = object_at(x, y);
  if (obj && obj->kind != OBJ_TRAP)
    put_at(x, y, obj->glyph);
  else if (obj && obj->kind == OBJ_TRAP && obj->known)
    put_at(x, y, obj->glyph);
  else if (monster_at(x, y))
    put_at(x, y, 'K');
}

static void draw_map_cell(x, y)
int x;
int y;
{
  if (!map_in_bounds(x, y) || !map_seen(x, y) ||
      map_tile(x, y) == TILE_NOTHING)
    return;
  put_at(x, y, tile_glyph(map_tile(x, y)));
  draw_floor_object_at(x, y);
}

static void draw_map_row(y, x1, x2)
int y;
int x1;
int x2;
{
  int x;
  int pos;

  if (y < 0 || y >= MAP_MAX_HEIGHT) return;
  if (x1 < 0) x1 = 0;
  if (x2 >= MAP_MAX_WIDTH) x2 = MAP_MAX_WIDTH - 1;
  if (x2 >= rogue_get8(OFF_SCREEN_WIDTH)) x2 = rogue_get8(OFF_SCREEN_WIDTH) - 1;
  if (x1 > x2) return;

  pos = 0;
  for (x = x1; x <= x2; x++) {
    if (map_seen(x, y) && map_tile(x, y) != TILE_NOTHING)
      map_line[pos++] = (char) tile_glyph(map_tile(x, y));
    else
      map_line[pos++] = ' ';
  }
  map_line[pos] = 0;
  epyx_move_cursor(x1, y);
  epyx_write_string(map_line);
}

static void reveal_room(index)
int index;
{
  int x;
  int y;
  RogueRoom *room;

  if (index < 0 || index >= ROOM_COUNT) return;
  room = &rooms[index];
  if (!room->w) return;
  if (room->visible) return;
  room->visible = 1;
  for (y = room->y; y < room->y + room->h; y++)
    for (x = room->x; x < room->x + room->w; x++)
      if (map_in_bounds(x, y) && !map_seen(x, y)) {
        map_seen(x, y) = 1;
      }
  if (reveal_draw_enabled) {
    for (y = room->y; y < room->y + room->h; y++)
      draw_map_row(y, room->x, room->x + room->w - 1);
    draw_floor_objects();
    draw_monsters();
  }
}

static void reveal_around(x, y)
int x;
int y;
{
  int dx;
  int dy;
  int room;

  room = room_at_position(x, y);
  if (room >= 0) {
    reveal_room(room);
    return;
  }
  if (map_in_bounds(x, y) && map_tile(x, y) == TILE_DOOR) {
    room = room_at_door(x, y);
    if (room >= 0) {
      reveal_room(room);
      return;
    }
  }

  for (dy = -1; dy <= 1; dy++)
    for (dx = -1; dx <= 1; dx++)
      if (map_in_bounds(x + dx, y + dy) &&
          map_tile(x + dx, y + dy) != TILE_NOTHING &&
          !map_seen(x + dx, y + dy)) {
        map_seen(x + dx, y + dy) = 1;
        if (reveal_draw_enabled) draw_map_cell(x + dx, y + dy);
      }
}

static void draw_status()
{
  int armor;
  int screen_width;
  char redraw;

  redraw = status_invalid;
  screen_width = rogue_get8(OFF_SCREEN_WIDTH);
  armor = worn_armor_index != NO_ITEM_INDEX ?
      11 - inventory[worn_armor_index].armor_class : 5;

  if (screen_width >= 8 && (redraw || dungeon_level != last_status_level)) {
    epyx_move_cursor(0, status_y);
    epyx_printf(rogue_string_at(OFF_STATUS_LEVEL_FORMAT), dungeon_level);
    last_status_level = dungeon_level;
  }
  if (screen_width >= 23 && (redraw || player_hp != last_status_hp)) {
    epyx_move_cursor(9, status_y);
    epyx_printf(rogue_string_at(OFF_STATUS_HITS_FORMAT), player_hp,
                player_max_hp);
    last_status_hp = player_hp;
  }
  if (screen_width >= 36 && redraw) {
    epyx_move_cursor(23, status_y);
    epyx_printf(rogue_string_at(OFF_STATUS_STRENGTH_FORMAT),
                player_strength + ring_strength_bonus, 16);
  }
  if (screen_width >= 48 && (redraw || player_gold != last_status_gold)) {
    epyx_move_cursor(38, status_y);
    epyx_printf(rogue_string_at(OFF_STATUS_GOLD_FORMAT), player_gold);
    last_status_gold = player_gold;
  }
  if (screen_width >= 62 && (redraw || armor != last_status_armor)) {
    epyx_move_cursor(52, status_y);
    epyx_printf(rogue_string_at(OFF_STATUS_ARMOR_FORMAT), armor);
    last_status_armor = (char) armor;
  }
  status_invalid = 0;
}

static void draw_floor_objects()
{
  int i;
  RogueObject *obj;

  for (i = 0; i < FLOOR_OBJECTS; i++) {
    obj = &floor_objects[i];
    if (obj->kind == OBJ_NONE || !overlay_visible_position(obj->x, obj->y))
      continue;
    if (obj->kind != OBJ_TRAP || obj->known) put_at(obj->x, obj->y, obj->glyph);
  }
}

static void draw_monsters()
{
  int i;

  for (i = 0; i < MONSTER_MAX; i++)
    if (monsters[i].hp > 0 &&
        overlay_visible_position(monsters[i].x, monsters[i].y))
      put_at(monsters[i].x, monsters[i].y, monsters[i].type);
}

static void draw_hero()
{
  put_at(hero_x, hero_y, glyph(GLYPH_PLAYER));
  rogue_put16(OFF_HERO_POS, hero_y * 256 + hero_x);
}

static int is_walkable(x, y)
int x;
int y;
{
  if (!map_in_bounds(x, y)) return 0;
  return walkable_tile(map_tile(x, y));
}

static int visible_position(x, y)
int x;
int y;
{
  return map_in_bounds(x, y) && map_seen(x, y);
}

static int overlay_visible_position(x, y)
int x;
int y;
{
  int hero_room;
  int target_room;

  if (!visible_position(x, y)) return 0;
  hero_room = hero_visibility_room();
  target_room = position_visibility_room(x, y);
  if (target_room >= 0)
    return hero_room == target_room && rooms[target_room].visible;
  if (hero_room >= 0) return 0;
  return x >= hero_x - 1 && x <= hero_x + 1 &&
         y >= hero_y - 1 && y <= hero_y + 1;
}

static RogueObject *object_at(x, y)
int x;
int y;
{
  int i;

  for (i = 0; i < FLOOR_OBJECTS; i++) {
    if (floor_objects[i].kind != OBJ_NONE &&
        floor_objects[i].x == x && floor_objects[i].y == y)
      return &floor_objects[i];
  }
  return 0;
}

static RogueMonster *monster_at(x, y)
int x;
int y;
{
  int i;

  for (i = 0; i < MONSTER_MAX; i++) {
    if (monsters[i].hp > 0 && monsters[i].x == x && monsters[i].y == y)
      return &monsters[i];
  }
  return 0;
}

static int adjacent_monster()
{
  int i;

  for (i = 0; i < MONSTER_MAX; i++) {
    if (monsters[i].hp > 0 &&
        monsters[i].x >= hero_x - 1 && monsters[i].x <= hero_x + 1 &&
        monsters[i].y >= hero_y - 1 && monsters[i].y <= hero_y + 1)
      return 1;
  }
  return 0;
}

static int floor_glyph_at(x, y)
int x;
int y;
{
  RogueObject *obj;

  obj = object_at(x, y);
  if (obj && (obj->kind != OBJ_TRAP || obj->known) &&
      overlay_visible_position(x, y))
    return obj->glyph;
  if (!map_in_bounds(x, y) || !map_seen(x, y)) return ' ';
  return tile_glyph(map_tile(x, y));
}

static RogueObject *free_floor_object()
{
  int i;

  for (i = 0; i < FLOOR_OBJECTS; i++) {
    if (floor_objects[i].kind == OBJ_NONE) return &floor_objects[i];
  }
  return 0;
}

static int occupied_floor_position(x, y)
int x;
int y;
{
  if (x == hero_x && y == hero_y) return 1;
  if (x == stair_x && y == stair_y) return 1;
  if (monster_at(x, y)) return 1;
  return object_at(x, y) != 0;
}

static void random_room_position(xp, yp, room_index)
int *xp;
int *yp;
int room_index;
{
  RogueRoom *room;
  int tries;
  int x;
  int y;

  room = &rooms[room_index];
  for (tries = 0; tries < 40; tries++) {
    x = room->x + 1 + random_range(room->w - 2);
    y = room->y + 1 + random_range(room->h - 2);
    if (!occupied_floor_position(x, y)) {
      *xp = x;
      *yp = y;
      return;
    }
  }

  *xp = room->x + 1;
  *yp = room->y + 1;
}

static void random_floor_position(xp, yp)
int *xp;
int *yp;
{
  random_room_position(xp, yp, random_range(ROOM_COUNT));
}

static RogueMonster *free_monster()
{
  int i;

  for (i = 0; i < MONSTER_MAX; i++) {
    if (monsters[i].hp <= 0) return &monsters[i];
  }
  return 0;
}

static int room_has_object(room_index)
int room_index;
{
  int i;

  for (i = 0; i < FLOOR_OBJECTS; i++) {
    if (floor_objects[i].kind != OBJ_NONE &&
        room_at_position(floor_objects[i].x, floor_objects[i].y) == room_index)
      return 1;
  }
  return 0;
}

static int add_random_monster(room_index, table)
int room_index;
int table;
{
  RogueMonster *mon;
  int x;
  int y;
  int tries;
  int hero_room;

  mon = free_monster();
  if (!mon) return 0;
  if (room_index >= 0) {
    random_room_position(&x, &y, room_index);
  } else {
    hero_room = hero_visibility_room();
    for (tries = 0; tries < 20; tries++) {
      random_floor_position(&x, &y);
      if (room_at_position(x, y) != hero_room && !overlay_visible_position(x, y))
        break;
    }
  }
  mon->x = (char) x;
  mon->y = (char) y;
  mon->type = (char) random_monster_type(table);
  mon->hp = (char) monster_initial_hp(mon->type);
  return 1;
}

static void set_random_floor_object(slot, kind, glyph, subtype, quantity)
int slot;
int kind;
int glyph;
int subtype;
int quantity;
{
  int x;
  int y;

  random_floor_position(&x, &y);
  floor_objects[slot].kind = (char) kind;
  floor_objects[slot].glyph = (char) glyph;
  floor_objects[slot].subtype = (char) subtype;
  floor_objects[slot].x = (char) x;
  floor_objects[slot].y = (char) y;
  floor_objects[slot].quantity = (char) quantity;
  floor_objects[slot].known = 0;
}

static void clear_map()
{
  int x;
  int y;

  for (y = 0; y < MAP_MAX_HEIGHT; y++)
    for (x = 0; x < MAP_MAX_WIDTH; x++) {
      map_tile(x, y) = TILE_NOTHING;
      map_seen(x, y) = 0;
    }
  for (x = 0; x < ROOM_COUNT; x++) {
    rooms[x].x = 0;
    rooms[x].y = 0;
    rooms[x].w = 0;
    rooms[x].h = 0;
    rooms[x].visible = 0;
  }
}

static void make_room(index, grid_x, grid_y, cell_w, cell_h)
int index;
int grid_x;
int grid_y;
int cell_w;
int cell_h;
{
  RogueRoom *room;
  int x0;
  int y0;
  int x;
  int y;

  room = &rooms[index];
  room->w = (char) (5 + random_range(cell_w - 5));
  room->h = (char) (4 + random_range(cell_h - 4));
  if (room->w > cell_w - 2) room->w = (char) (cell_w - 2);
  if (room->h > cell_h - 1) room->h = (char) (cell_h - 1);
  if (room->w < 4) room->w = 4;
  if (room->h < 4) room->h = 4;

  x0 = dungeon_x + grid_x * cell_w + 1;
  y0 = dungeon_y + grid_y * cell_h;
  if (cell_w > room->w + 1) x0 += random_range(cell_w - room->w - 1);
  if (cell_h > room->h) y0 += random_range(cell_h - room->h);

  room->x = (char) x0;
  room->y = (char) y0;
  room->visible = 0;

  for (y = y0; y < y0 + room->h; y++)
    for (x = x0; x < x0 + room->w; x++) {
      if (y == y0 || y == y0 + room->h - 1) {
        if (x == x0 || x == x0 + room->w - 1) set_tile(x, y, TILE_CORNER);
        else set_tile(x, y, TILE_HORIZ);
      } else if (x == x0 || x == x0 + room->w - 1) {
        set_tile(x, y, TILE_VERT);
      } else {
        set_tile(x, y, TILE_FLOOR);
      }
    }
}

static void carve_hline(x1, x2, y)
int x1;
int x2;
int y;
{
  int x;

  x = x1;
  while (x != x2) {
    set_tile(x, y, TILE_HALL);
    x += x < x2 ? 1 : -1;
  }
  set_tile(x, y, TILE_HALL);
}

static void carve_vline(x, y1, y2)
int x;
int y1;
int y2;
{
  int y;

  y = y1;
  while (y != y2) {
    set_tile(x, y, TILE_HALL);
    y += y < y2 ? 1 : -1;
  }
  set_tile(x, y, TILE_HALL);
}

static void carve_horizontal_connection(x1, y1, x2, y2)
int x1;
int y1;
int x2;
int y2;
{
  int mid;

  if (y1 == y2) {
    carve_hline(x1, x2, y1);
    return;
  }

  mid = (x1 + x2) / 2;
  carve_hline(x1, mid, y1);
  carve_vline(mid, y1, y2);
  carve_hline(mid, x2, y2);
}

static void carve_vertical_connection(x1, y1, x2, y2)
int x1;
int y1;
int x2;
int y2;
{
  int mid;

  if (x1 == x2) {
    carve_vline(x1, y1, y2);
    return;
  }

  mid = (y1 + y2) / 2;
  carve_vline(x1, y1, mid);
  carve_hline(x1, x2, mid);
  carve_vline(x2, mid, y2);
}

static void set_room_door(room_index, x, y)
int room_index;
int x;
int y;
{
  set_tile(x, y, TILE_DOOR);
  if (rooms[room_index].visible) map_seen(x, y) = 1;
}

static void connect_rooms(a, b)
int a;
int b;
{
  RogueRoom *ra;
  RogueRoom *rb;
  int ax;
  int ay;
  int bx;
  int by;
  int min;
  int max;

  ra = &rooms[a];
  rb = &rooms[b];
  ax = ra->x + ra->w / 2;
  ay = ra->y + ra->h / 2;
  bx = rb->x + rb->w / 2;
  by = rb->y + rb->h / 2;

  if (a / ROOM_COLS == b / ROOM_COLS) {
    min = ra->y + 1;
    if (rb->y + 1 > min) min = rb->y + 1;
    max = ra->y + ra->h - 2;
    if (rb->y + rb->h - 2 < max) max = rb->y + rb->h - 2;
    if (min <= max) {
      ay = min + random_range(max - min + 1);
      by = ay;
    }
    if (ra->x < rb->x) {
      ax = ra->x + ra->w - 1;
      bx = rb->x;
    } else {
      ax = ra->x;
      bx = rb->x + rb->w - 1;
    }
    set_room_door(a, ax, ay);
    set_room_door(b, bx, by);
    carve_horizontal_connection(ax, ay, bx, by);
  } else {
    min = ra->x + 1;
    if (rb->x + 1 > min) min = rb->x + 1;
    max = ra->x + ra->w - 2;
    if (rb->x + rb->w - 2 < max) max = rb->x + rb->w - 2;
    if (min <= max) {
      ax = min + random_range(max - min + 1);
      bx = ax;
    }
    if (ra->y < rb->y) {
      ay = ra->y + ra->h - 1;
      by = rb->y;
    } else {
      ay = ra->y;
      by = rb->y + rb->h - 1;
    }
    set_room_door(a, ax, ay);
    set_room_door(b, bx, by);
    carve_vertical_connection(ax, ay, bx, by);
  }

  set_room_door(a, ax, ay);
  set_room_door(b, bx, by);
}

static void generate_map()
{
  int cell_w;
  int cell_h;
  int row;
  int col;
  int room;

  clear_map();
  cell_w = dungeon_width / ROOM_COLS;
  cell_h = dungeon_height / ROOM_ROWS;
  if (cell_w < 7) cell_w = 7;
  if (cell_h < 4) cell_h = 4;

  for (row = 0; row < ROOM_ROWS; row++)
    for (col = 0; col < ROOM_COLS; col++) {
      room = row * ROOM_COLS + col;
      make_room(room, col, row, cell_w, cell_h);
    }

  for (row = 0; row < ROOM_ROWS; row++)
    for (col = 0; col < ROOM_COLS - 1; col++)
      connect_rooms(row * ROOM_COLS + col, row * ROOM_COLS + col + 1);
  for (row = 0; row < ROOM_ROWS - 1; row++)
    for (col = 0; col < ROOM_COLS; col++)
      connect_rooms(row * ROOM_COLS + col, (row + 1) * ROOM_COLS + col);
}

static void populate_level()
{
  int i;
  int x;
  int y;
  int chance;

  reveal_draw_enabled = 0;
  for (i = 0; i < FLOOR_OBJECTS; i++) floor_objects[i].kind = OBJ_NONE;

  generate_map();
  random_room_position(&hero_x, &hero_y, 0);
  reveal_room(0);
  spawn_clock = 0;

  for (i = 0; i < MONSTER_MAX; i++) monsters[i].hp = 0;

  random_floor_position(&x, &y);
  stair_x = (char) x;
  stair_y = (char) y;
  set_tile(stair_x, stair_y, TILE_STAIRS);

  set_random_floor_object(0, OBJ_GOLD, glyph(GLYPH_GOLD), 0,
                          20 + random_range(60));
  set_random_floor_object(1, OBJ_FOOD, glyph(GLYPH_FOOD), 0, 1);
  set_random_floor_object(2, OBJ_POTION, glyph(GLYPH_POTION),
                          random_range(POTION_COUNT), 1);
  set_random_floor_object(3, OBJ_SCROLL, glyph(GLYPH_SCROLL),
                          random_range(SCROLL_COUNT), 1);
  set_random_floor_object(4, OBJ_WEAPON, glyph(GLYPH_WEAPON),
                          random_range(WEAPON_COUNT), 1);
  set_random_floor_object(5, OBJ_ARMOR, glyph(GLYPH_ARMOR),
                          random_range(ARMOR_COUNT), 1);
  set_random_floor_object(6, OBJ_RING, glyph(GLYPH_RING),
                          random_range(RING_COUNT), 1);
  set_random_floor_object(7, OBJ_WAND, glyph(GLYPH_WAND),
                          random_range(WAND_COUNT), 1);
  set_random_floor_object(8, OBJ_TRAP, '"', random_range(6), 1);
  set_random_floor_object(9, OBJ_AMULET, '&', 0, 1);

  for (i = 1; i < ROOM_COUNT; i++) {
    chance = room_has_object(i) ? 80 : 25;
    if (random_range(100) < chance) add_random_monster(i, 0);
  }
}

static void draw_known_area()
{
  int x;
  int y;
  int start;
  int width;

  width = rogue_get8(OFF_SCREEN_WIDTH);
  if (width > MAP_MAX_WIDTH) width = MAP_MAX_WIDTH;
  for (y = 0; y < status_y && y < MAP_MAX_HEIGHT; y++) {
    x = 0;
    while (x < width) {
      while (x < width &&
             (!map_seen(x, y) || map_tile(x, y) == TILE_NOTHING))
        x++;
      start = x;
      while (x < width && map_seen(x, y) && map_tile(x, y) != TILE_NOTHING)
        x++;
      if (start < x) draw_map_row(y, start, x - 1);
    }
  }
  draw_floor_objects();
  draw_monsters();
}

static void reveal_after_move()
{
  reveal_around(hero_x, hero_y);
}

static void erase_hero()
{
  put_at(hero_x, hero_y, floor_glyph_at(hero_x, hero_y));
}

static void erase_monster(monster)
RogueMonster *monster;
{
  if (!overlay_visible_position(monster->x, monster->y)) return;
  put_at(monster->x, monster->y, floor_glyph_at(monster->x, monster->y));
}

static void no_appropriate()
{
  epyx_message(rogue_string_at(OFF_NO_APPROPRIATE_OBJECT));
}

static void message_defeated(monster)
RogueMonster *monster;
{
  epyx_message(rogue_string_at(OFF_COMBAT_JOIN_FORMAT),
               rogue_string_at(OFF_COMBAT_DEFEATED),
               monster_name(monster));
}

static void message_you_hit()
{
  epyx_message(rogue_string_at(OFF_COMBAT_YOU_VERB),
               rogue_string_at(OFF_COMBAT_HIT));
}

static void defeat_monster(monster)
RogueMonster *monster;
{
  RogueObject *obj;
  int kind;

  if (monster->type == 'F') flytrap_hold = 0;
  erase_monster(monster);
  if (random_range(5) != 0 || object_at(monster->x, monster->y)) return;
  obj = free_floor_object();
  if (!obj) return;
  kind = random_range(5);
  obj->x = monster->x;
  obj->y = monster->y;
  obj->quantity = 1;
  obj->known = 0;
  obj->kind = (char) (kind + OBJ_FOOD);
  if (obj->kind == OBJ_GOLD) {
    obj->quantity = (char) (5 + random_range(dungeon_level * 10 + 20));
    obj->glyph = glyph(GLYPH_GOLD);
  } else if (obj->kind == OBJ_POTION) {
    obj->glyph = glyph(GLYPH_POTION);
    obj->subtype = (char) random_range(POTION_COUNT);
  } else if (obj->kind == OBJ_SCROLL) {
    obj->glyph = glyph(GLYPH_SCROLL);
    obj->subtype = (char) random_range(SCROLL_COUNT);
  } else if (obj->kind == OBJ_WEAPON) {
    obj->glyph = glyph(GLYPH_WEAPON);
    obj->subtype = (char) random_range(WEAPON_COUNT);
  } else {
    obj->glyph = glyph(GLYPH_FOOD);
    obj->subtype = 0;
  }
  draw_floor_objects();
}

static int nymph_steal(monster)
RogueMonster *monster;
{
  InventoryItem *item;
  int chosen;
  int count;
  int i;
  char quantity;

  chosen = NO_ITEM_INDEX;
  count = 0;
  for (i = 0; i < inventory_count; i++) {
    item = &inventory[i];
    if (i == wielded_weapon_index || i == worn_armor_index ||
        i == left_ring_index || i == right_ring_index ||
        item->kind == OBJ_FOOD ||
        (item->kind == OBJ_WEAPON && !item->hit_bonus &&
         !item->damage_bonus) ||
        (item->kind == OBJ_ARMOR && !item->hit_bonus &&
         !item->armor_class))
      continue;
    if (random_range(++count) == 0) chosen = i;
  }
  if (chosen == NO_ITEM_INDEX) return 0;

  item = &inventory[chosen];
  quantity = item->quantity;
  item->quantity = 1;
  inventory_object_name(item);
  item->quantity = quantity;
  remove_item(item);
  erase_monster(monster);
  monster->hp = 0;
  epyx_message(rogue_string_at(OFF_NYMPH_STOLE), object_name_buf);
  return 1;
}

static void monster_hit_player(monster)
RogueMonster *monster;
{
  int chance;
  int damage;

  chance = 12 + monster_table_byte(monster->type, 10);
  if (worn_armor_index != NO_ITEM_INDEX)
    chance -= inventory[worn_armor_index].armor_class / 2;
  if (chance < 2) chance = 2;
  if (chance > 19) chance = 19;

  if (random_range(20) >= chance) {
    epyx_message(rogue_string_at(OFF_COMBAT_THE_MONSTER_VERB),
                 monster_bare_name(monster->type),
                 rogue_string_at(OFF_COMBAT_MISSES));
    return;
  }

  if (monster->type == 'A' && worn_armor_index != NO_ITEM_INDEX) {
    if ((left_ring_index != NO_ITEM_INDEX &&
         inventory[left_ring_index].subtype == 13) ||
        (right_ring_index != NO_ITEM_INDEX &&
         inventory[right_ring_index].subtype == 13)) {
      epyx_message(rogue_string_at(OFF_RUST_VANISHES));
      return;
    }
    if (inventory[worn_armor_index].armor_class < 9) {
      inventory[worn_armor_index].armor_class++;
      draw_status();
      epyx_message(rogue_string_at(OFF_ARMOR_WEAKENS));
      return;
    }
  }

  if (monster->type == 'N' && nymph_steal(monster)) return;
  if (monster->type == 'F') {
    if (flytrap_hold < 9) flytrap_hold++;
    return;
  }
  if (monster->type == 'I') {
    frozen_turns = 2;
    return;
  }
  if (monster->type == 'L') {
    if (player_gold) {
      player_gold -= 1 + random_range(dungeon_level * 10 + 50);
      if (player_gold < 0) player_gold = 0;
      rogue_put16(OFF_PLAYER_GOLD, player_gold);
      draw_status();
      epyx_message(rogue_string_at(OFF_PURSE_FEELS_LIGHTER));
    }
    erase_monster(monster);
    monster->hp = 0;
    return;
  }
  if ((monster->type == 'V' && random_range(100) < 30) ||
      (monster->type == 'W' && random_range(100) < 15)) {
    damage = 1 + random_range(monster->type == 'V' ? 5 : 10);
    player_max_hp = (char) (player_max_hp - damage);
    if (player_max_hp < 1) player_max_hp = 1;
    player_hp = (char) (player_hp - damage);
    if (player_hp < 1) player_hp = 1;
    status_invalid = 1;
    draw_status();
    epyx_message(rogue_string_at(OFF_SUDDENLY_WEAKER));
    return;
  }
  if (monster->type == 'R' && !random_range(2)) {
    if ((left_ring_index != NO_ITEM_INDEX &&
         inventory[left_ring_index].subtype == 2) ||
        (right_ring_index != NO_ITEM_INDEX &&
         inventory[right_ring_index].subtype == 2)) {
      epyx_message(rogue_string_at(OFF_SNAKE_BITE_MOMENTARY));
      return;
    }
    if (player_strength > 3) player_strength--;
    status_invalid = 1;
    draw_status();
    epyx_message(rogue_string_at(OFF_SNAKE_BITE_WEAKENS));
    return;
  }

  damage = monster_damage(monster->type);
  if (worn_armor_index != NO_ITEM_INDEX && damage > 0) damage--;
  if (damage > 0) {
    player_hp = (char) (player_hp - damage);
    draw_status();
  }
  if (player_hp <= 0) {
    death_cause = monster->type;
    epyx_message(rogue_string_at(OFF_DEATH_YOU_DIED));
  } else {
    epyx_message(rogue_string_at(OFF_COMBAT_THE_MONSTER_VERB),
                 monster_bare_name(monster->type),
                 rogue_string_at(OFF_COMBAT_HITS));
  }
}

static const char *formatted_name(fmt, name)
const char *fmt;
const char *name;
{
  epyx_format(object_name_buf, sizeof(object_name_buf), fmt, name);
  return object_name_buf;
}

static const char *potion_color(subtype)
int subtype;
{
  if (subtype < 0 || subtype >= POTION_COUNT) return "clear";
  return potion_color_text + potion_color_offsets[potion_color_index[subtype]];
}

static int starts_with_vowel(s)
const char *s;
{
  return *s == 'a' || *s == 'e' || *s == 'i' || *s == 'o' || *s == 'u';
}

#if ROGUE_WITH_CALLS
static int called_kind_index(kind)
int kind;
{
  if (kind == OBJ_SCROLL) return 0;
  if (kind == OBJ_POTION) return 1;
  if (kind == OBJ_RING) return 2;
  if (kind == OBJ_WAND) return 3;
  return -1;
}

static char *called_name(kind, subtype)
int kind;
int subtype;
{
  int index;

  index = called_kind_index(kind);
  if (index < 0 || subtype < 0 || subtype >= CALLED_ITEM_MAX) return 0;
  if (called_names[index][subtype][0] == 0) return 0;
  return called_names[index][subtype];
}
#endif

static const char *object_name(kind, subtype)
int kind;
int subtype;
{
  const char *color;
#if ROGUE_WITH_CALLS
  char *called;
#endif

  if (kind == OBJ_FOOD) return rogue_string_at(OFF_SOME_FOOD);
  if (kind == OBJ_POTION) {
    color = potion_color(subtype);
    if (rogue_get8(OFF_POTION_KNOWN_FLAGS + subtype)) {
      epyx_format(object_name_buf, sizeof(object_name_buf),
                  "a potion of %s(%s)",
                  arena_table_string(OFF_POTION_TABLE, subtype, 4),
                  color);
      return object_name_buf;
    }
#if ROGUE_WITH_CALLS
    called = called_name(kind, subtype);
    if (called) return formatted_name("a potion called %s", called);
#endif
    if (starts_with_vowel(color)) return formatted_name("an %s potion", color);
    return formatted_name("a %s potion", color);
  }
  if (kind == OBJ_SCROLL) {
    if (rogue_get8(OFF_SCROLL_KNOWN_FLAGS + subtype))
      return formatted_name("a scroll of %s",
                            arena_table_string(OFF_SCROLL_TABLE, subtype, 4));
#if ROGUE_WITH_CALLS
    called = called_name(kind, subtype);
    if (called) return formatted_name("a scroll called %s", called);
#endif
    return formatted_name("a scroll titled '%s'",
                          rogue_string_at(OFF_RANDOM_SCROLL_NAMES +
                                          subtype * SCROLL_TITLE_SIZE));
  }
  if (kind == OBJ_WEAPON) {
    if (subtype < 0 || subtype >= WEAPON_COUNT) subtype = 0;
    color = arena_table_string(OFF_WEAPON_NAME_TABLE, subtype, 2);
    return formatted_name(starts_with_vowel(color) ? "an %s" : "a %s", color);
  }
  if (kind == OBJ_ARMOR) {
    if (subtype < 0 || subtype >= ARMOR_COUNT) subtype = 0;
    return arena_table_string(OFF_ARMOR_NAME_TABLE, subtype, 2);
  }
  if (kind == OBJ_RING) {
    if (subtype < 0 || subtype >= RING_COUNT) subtype = 0;
#if ROGUE_WITH_CALLS
    called = called_name(kind, subtype);
    if (called) return formatted_name("a ring called %s", called);
#endif
    color = arena_table_string(OFF_RING_STONE_PTRS, subtype, 2);
    return formatted_name(starts_with_vowel(color) ? "an %s ring" :
                          "a %s ring", color);
  }
  if (kind == OBJ_WAND) {
    if (subtype < 0 || subtype >= WAND_COUNT) subtype = 0;
#if ROGUE_WITH_CALLS
    called = called_name(kind, subtype);
    if (called) return formatted_name("a wand called %s", called);
#endif
    color = arena_table_string(OFF_WAND_MATERIAL_PTRS, subtype, 2);
    return formatted_name(starts_with_vowel(color) ? "an %s" : "a %s",
                          color);
  }
  if (kind == OBJ_TRAP) return "a trap";
  if (kind == OBJ_AMULET) return rogue_string_at(OFF_AMULET_NAME);
  return "something";
}

static void format_bonus(dest, value)
char *dest;
int value;
{
  epyx_format(dest, 6, value < 0 ? "-%d" : "+%d",
              value < 0 ? -value : value);
}

static const char *inventory_object_name(item)
InventoryItem *item;
{
  const char *name;
  int subtype;
  char hit[6];
  char damage[6];

  if (!item) return "something";

  subtype = item->subtype;
  if (item->kind == OBJ_WEAPON && item->quantity <= 1) {
    if (subtype < 0 || subtype >= WEAPON_COUNT) subtype = 0;
    name = arena_table_string(OFF_WEAPON_NAME_TABLE, subtype, 2);
    format_bonus(hit, item->hit_bonus);
    format_bonus(damage, item->damage_bonus);
    epyx_format(object_name_buf, sizeof(object_name_buf),
                "A %s,%s %s", hit, damage, name);
    return object_name_buf;
  }
  if (item->kind == OBJ_ARMOR && item->armor_class) {
    name = object_name(item->kind, item->subtype);
    format_bonus(hit, item->hit_bonus);
    epyx_format(object_name_buf, sizeof(object_name_buf),
                rogue_string_at(OFF_ARMOR_CLASS_FORMAT), hit, name,
                item->armor_class);
    return object_name_buf;
  }
  if (item->quantity <= 1) return object_name(item->kind, item->subtype);

  if (item->kind == OBJ_FOOD) {
    epyx_format(object_name_buf, sizeof(object_name_buf),
                rogue_string_at(OFF_FOOD_RATIONS_FORMAT), item->quantity);
    return object_name_buf;
  }
  if (item->kind == OBJ_WEAPON) {
    if (subtype < 0 || subtype >= WEAPON_COUNT) subtype = 0;
    name = arena_table_string(OFF_WEAPON_NAME_TABLE, subtype, 2);
    format_bonus(hit, item->hit_bonus);
    format_bonus(damage, item->damage_bonus);
    epyx_format(object_name_buf, sizeof(object_name_buf),
                "%d %s,%s %ss", item->quantity, hit, damage, name);
    return object_name_buf;
  }
  if (item->kind == OBJ_POTION) {
    name = potion_color(subtype);
    if (rogue_get8(OFF_POTION_KNOWN_FLAGS + subtype)) {
      epyx_format(object_name_buf, sizeof(object_name_buf),
                  "%d potions of %s(%s)", item->quantity,
                  arena_table_string(OFF_POTION_TABLE, subtype, 4), name);
      return object_name_buf;
    }
    epyx_format(object_name_buf, sizeof(object_name_buf),
                "%d %s potions", item->quantity, name);
    return object_name_buf;
  }
  if (item->kind == OBJ_SCROLL) {
    if (rogue_get8(OFF_SCROLL_KNOWN_FLAGS + subtype)) {
      epyx_format(object_name_buf, sizeof(object_name_buf),
                  "%d scrolls of %s", item->quantity,
                  arena_table_string(OFF_SCROLL_TABLE, subtype, 4));
      return object_name_buf;
    }
    epyx_format(object_name_buf, sizeof(object_name_buf),
                "%d scrolls titled '%s'", item->quantity,
                rogue_string_at(OFF_RANDOM_SCROLL_NAMES +
                                subtype * SCROLL_TITLE_SIZE));
    return object_name_buf;
  }
  return object_name(item->kind, item->subtype);
}

static InventoryItem *inventory_item(kind, subtype)
int kind;
int subtype;
{
  int i;

  for (i = 0; i < inventory_count; i++) {
    if (inventory[i].kind == kind && inventory[i].subtype == subtype)
      return &inventory[i];
  }
  return 0;
}

static int inventory_index(item)
InventoryItem *item;
{
  if (!item) return NO_ITEM_INDEX;
  return item - inventory;
}

static int has_inventory_kind(kind)
int kind;
{
  int i;

  for (i = 0; i < inventory_count; i++) {
    if (inventory[i].kind == kind) return 1;
  }
  return 0;
}

static InventoryItem *add_item(kind, glyph, subtype, quantity)
int kind;
int glyph;
int subtype;
int quantity;
{
  InventoryItem *item;

  item = inventory_item(kind, subtype);
  if (item) {
    item->quantity = (char) (item->quantity + quantity);
    return item;
  }
  if (inventory_count >= INVENTORY_MAX) return 0;

  item = &inventory[inventory_count++];
  item->kind = (char) kind;
  item->glyph = (char) glyph;
  item->subtype = (char) subtype;
  item->quantity = (char) quantity;
  item->hit_bonus = 0;
  item->damage_bonus = 0;
  item->armor_class = 0;
  return item;
}

static void update_equipment_after_remove(index, item)
int index;
InventoryItem *item;
{
  if (wielded_weapon_index == index) {
    wielded_weapon_index = NO_ITEM_INDEX;
  } else if (wielded_weapon_index > index && wielded_weapon_index < NO_ITEM_INDEX) {
    wielded_weapon_index--;
  }

  if (worn_armor_index == index) {
    worn_armor_index = NO_ITEM_INDEX;
  } else if (worn_armor_index > index && worn_armor_index < NO_ITEM_INDEX) {
    worn_armor_index--;
  }

  if (left_ring_index == index) {
    apply_ring_effect(item, 0);
    left_ring_index = NO_ITEM_INDEX;
  } else if (left_ring_index > index && left_ring_index < NO_ITEM_INDEX) {
    left_ring_index--;
  }

  if (right_ring_index == index) {
    apply_ring_effect(item, 0);
    right_ring_index = NO_ITEM_INDEX;
  } else if (right_ring_index > index && right_ring_index < NO_ITEM_INDEX) {
    right_ring_index--;
  }
}

static void remove_item(item)
InventoryItem *item;
{
  int index;

  if (!item) return;
  if (item->quantity > 1) {
    item->quantity--;
    return;
  }

  index = item - inventory;
  update_equipment_after_remove(index, item);
  inventory_count--;
  while (index < inventory_count) {
    inventory[index] = inventory[index + 1];
    index++;
  }
}

static void pickup_object(obj)
RogueObject *obj;
{
  if (obj->kind == OBJ_TRAP) {
    no_appropriate();
    return;
  }
  if (obj->kind == OBJ_GOLD) {
    player_gold += obj->quantity;
    rogue_put16(OFF_PLAYER_GOLD, player_gold);
    epyx_message(rogue_string_at(OFF_FOUND_GOLD_MESSAGE), obj->quantity);
  } else {
    if (!add_item(obj->kind, obj->glyph, obj->subtype, obj->quantity)) {
      epyx_message(rogue_string_at(OFF_PACK_FULL));
      return;
    }
    epyx_message(rogue_string_at(OFF_FOUND_OBJECT_MESSAGE),
                 object_name(obj->kind, obj->subtype));
  }

  obj->kind = OBJ_NONE;
}

static const char *trap_name_with_article(subtype)
int subtype;
{
  if (subtype < 0 || subtype >= sizeof(trap_name_offsets) /
                              sizeof(trap_name_offsets[0]))
    subtype = 0;
  return rogue_string_at(trap_name_offsets[subtype]);
}

static const char *trap_message(subtype)
int subtype;
{
  if (subtype < 0 || subtype >= sizeof(trap_message_offsets) /
                              sizeof(trap_message_offsets[0]))
    subtype = 0;
  return rogue_string_at(trap_message_offsets[subtype]);
}

static void trigger_trap(obj)
RogueObject *obj;
{
  int damage;

  if (!obj || obj->kind != OBJ_TRAP) return;
  obj->known = 1;
  damage = 1 + random_range(2);
  if (obj->subtype == 1 || obj->subtype == 5) damage++;
  player_hp = (char) (player_hp - damage);
  if (player_hp < 0) player_hp = 0;
  status_invalid = 1;
  draw_status();
  draw_hero();
  if (player_hp <= 0) {
    death_cause = obj->subtype == 1 ? 'a' : obj->subtype == 5 ? 'd' : 'G';
    epyx_message(rogue_string_at(OFF_DEATH_YOU_DIED));
  } else {
    epyx_message(trap_message(obj->subtype));
  }
}

static void try_move(dx, dy)
int dx;
int dy;
{
  int old_room;
  int new_room;
  int nx;
  int ny;
  RogueMonster *mon;
  RogueObject *obj;

  old_room = hero_visibility_room();
  nx = hero_x + dx;
  ny = hero_y + dy;
  mon = monster_at(nx, ny);
  if (flytrap_hold && (!mon || mon->type != 'F')) {
    turn_taken = 1;
    return;
  }
  if (mon) {
    mon->hp -= wielded_weapon_index != NO_ITEM_INDEX ? 2 : 1;
    turn_taken = 1;
    if (mon->hp <= 0) {
      defeat_monster(mon);
      message_defeated(mon);
    } else {
      message_you_hit();
    }
    return;
  }

  if (!is_walkable(nx, ny)) {
    return;
  }

  erase_hero();
  rogue_put16(OFF_OLD_HERO_POS, hero_y * 256 + hero_x);
  hero_x = nx;
  hero_y = ny;
  turn_taken = 1;
  reveal_after_move();
  new_room = hero_visibility_room();
  if (old_room != new_room) {
    draw_room_terrain(old_room);
    draw_room_terrain(new_room);
    draw_floor_objects();
    draw_monsters();
  }
  draw_hero();
  obj = object_at(hero_x, hero_y);
  if (obj) {
    if (obj->kind == OBJ_TRAP)
      trigger_trap(obj);
    else if (!go_over_object)
      pickup_here();
  }
  go_over_object = 0;
}

static void monster_turn_one(monster)
RogueMonster *monster;
{
  int dx;
  int dy;
  int nx;
  int ny;

  if (monster->hp <= 0) return;
  if (monster->type == 'T' && monster->hp < 30 && !random_range(3))
    monster->hp++;
  if (!overlay_visible_position(monster->x, monster->y)) return;
  if (monster->x >= hero_x - 1 && monster->x <= hero_x + 1 &&
      monster->y >= hero_y - 1 && monster->y <= hero_y + 1 &&
      (monster->x != hero_x || monster->y != hero_y)) {
    monster_hit_player(monster);
    return;
  }

  dx = 0;
  dy = 0;
  if (monster->x < hero_x) dx = 1;
  else if (monster->x > hero_x) dx = -1;
  if (monster->y < hero_y) dy = 1;
  else if (monster->y > hero_y) dy = -1;

  nx = monster->x + dx;
  ny = monster->y + dy;
  if (!is_walkable(nx, ny) || object_at(nx, ny) || monster_at(nx, ny)) return;
  if (nx == hero_x && ny == hero_y) {
    monster_hit_player(monster);
    return;
  }

  erase_monster(monster);
  monster->x = (char) nx;
  monster->y = (char) ny;
  draw_monsters();
  draw_hero();
}

static void monster_turn()
{
  int i;

  for (i = 0; i < MONSTER_MAX; i++) monster_turn_one(&monsters[i]);
}

static void maybe_spawn_monster()
{
  spawn_clock++;
  if (spawn_clock < 6) return;
  spawn_clock = 0;
  if (random_range(6) == 0) add_random_monster(-1, 1);
}

static void pickup_here()
{
  RogueObject *obj;

  obj = object_at(hero_x, hero_y);
  if (!obj) {
    no_appropriate();
    return;
  }

  pickup_object(obj);
  clear_message_next = 1;
  draw_status();
  draw_hero();
}

static int read_key()
{
  char ch;

  if (read(0, &ch, 1) != 1) return -1;
  return ch;
}

static int action_auto_lists(action)
const char *action;
{
  return action == rogue_string_at(OFF_ACTION_DROP) ||
         action == rogue_string_at(OFF_ACTION_EAT) ||
         action == rogue_string_at(OFF_ACTION_QUAFF);
}

static int read_line_text(dest, max)
char *dest;
int max;
{
  int ch;
  int len;

  len = 0;
  dest[0] = 0;
  while (1) {
    ch = read_key();
    if (ch == KEY_RETURN) break;
    if (ch == KEY_ESCAPE || ch == KEY_CTRL_E) {
      len = 0;
      dest[0] = 0;
      break;
    }
    if ((ch == 8 || ch == 127) && len > 0) {
      len--;
      dest[len] = 0;
      epyx_write_char(8);
      epyx_write_char(' ');
      epyx_write_char(8);
    } else if (ch >= 32 && ch < 127 && len < max - 1) {
      dest[len++] = (char) ch;
      dest[len] = 0;
      epyx_write_char(ch);
    }
  }
  return len;
}

static void ask_player_name()
{
  char *name;
  char *default_name;
  int len;

  name = rogue_string_at(OFF_PLAYER_NAME_BUFFER);

  epyx_clear_window();
  epyx_move_cursor(0, 0);
  epyx_write_string(rogue_string_at(OFF_NAME_PROMPT));
  epyx_cursor_on();

  len = read_line_text(name, 24);
  epyx_cursor_off();
  if (len > 0) {
    default_name = rogue_string_at(OFF_DEFAULT_PLAYER_NAME);
    while ((*default_name++ = *name++) != 0) ;
  }
}

#if ROGUE_WITH_CALLS || ROGUE_WITH_MACROS
static int read_prompt_text(prompt, dest, max)
const char *prompt;
char *dest;
int max;
{
  int len;

  epyx_message(prompt);
  epyx_cursor_on();
  len = read_line_text(dest, max);
  epyx_cursor_off();
  return len;
}
#endif

#if ROGUE_WITH_HELP
static void preload_text_file(path, buffer, max)
const char *path;
char *buffer;
int max;
{
  path_id fd;
  int count;

  buffer[0] = 0;
  if (_os_open(path, FAM_READ, &fd) != 0) return;

  count = max - 1;
  if (_os_read(fd, buffer, &count) == 0) buffer[count] = 0;
  _os_close(fd);
}
#endif

static void show_item_prompt(action)
const char *action;
{
  char *buffer;

  buffer = epyx_format_buffer();
  epyx_format(buffer, FORMAT_BUFFER_SIZE,
              rogue_string_at(OFF_OBJECT_ACTION_PROMPT), action);
  if (string_width(buffer) > rogue_get8(OFF_SCREEN_MAX_X))
    buffer[rogue_get8(OFF_SCREEN_MAX_X)] = 0;
  epyx_move_cursor(0, 0);
  epyx_write_string(buffer);
  epyx_clear_to_eol();
  epyx_message_clear_state();
}

static InventoryItem *select_item_from_list(action, kind)
const char *action;
int kind;
{
  int ch;
  int index;
  int rows;
  int prompt_y;

  while (1) {
    epyx_message_clear_state();
    epyx_clear_window();
    epyx_move_cursor(0, 0);
    rows = draw_inventory_lines(kind);
    prompt_y = rogue_get8(OFF_SCREEN_HEIGHT) - 1;
    if (prompt_y < rows + 1) prompt_y = rows + 1;
    epyx_move_cursor(0, prompt_y);
    epyx_reverse_on();
    epyx_printf(rogue_string_at(OFF_SELECT_ITEM_ACTION), action);
    epyx_reverse_off();
    epyx_clear_to_eol();

    ch = read_key();
    if (ch == KEY_ESCAPE || ch == KEY_CTRL_E) {
      redraw_dungeon();
      return 0;
    }
    if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
    index = ch - 'a';
    if (index >= 0 && index < inventory_count &&
        (!kind || inventory[index].kind == kind)) {
      redraw_dungeon();
      return &inventory[index];
    }
  }
}

static InventoryItem *choose_item(action, kind)
const char *action;
int kind;
{
  int ch;
  int index;

  if (inventory_count == 0 || (kind && !has_inventory_kind(kind))) {
    no_appropriate();
    return 0;
  }

  while (1) {
    if (action_auto_lists(action)) return select_item_from_list(action, kind);

    show_item_prompt(action);
    ch = read_key();
    if (ch == KEY_ESCAPE || ch == KEY_CTRL_E) {
      epyx_message(0);
      return 0;
    }
    if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
    if (ch != '*' && ch != ':') {
      index = ch - 'a';
      if (index >= 0 && index < inventory_count &&
          (!kind || inventory[index].kind == kind))
        return &inventory[index];
      if (ch < 'a' || ch > 'z') return select_item_from_list(action, kind);
      epyx_message(rogue_string_at(OFF_BAD_PACK_LETTER),
                   'a' + inventory_count - 1);
      return 0;
    }
    return select_item_from_list(action, kind);
  }
}

static void eat_item()
{
  InventoryItem *item;

  item = choose_item(rogue_string_at(OFF_ACTION_EAT), OBJ_FOOD);
  if (!item) return;

  remove_item(item);
  epyx_message(rogue_string_at(OFF_EAT_GOOD_FOOD));
}

static void quaff_item()
{
  InventoryItem *item;
  int subtype;

  item = choose_item(rogue_string_at(OFF_ACTION_QUAFF), OBJ_POTION);
  if (!item) return;
  if (item->kind != OBJ_POTION) {
    epyx_message(rogue_string_at(OFF_CANT_QUAFF_THAT));
    return;
  }

  subtype = item->subtype;
  rogue_put8(OFF_POTION_KNOWN_FLAGS + subtype, 1);
  remove_item(item);
  if (subtype == POTION_HEALING) {
    player_hp += 4;
    if (player_hp > player_max_hp) player_hp = player_max_hp;
    draw_status();
    epyx_message(rogue_string_at(OFF_POTION_HEALING_MESSAGE));
  } else {
    epyx_message(rogue_string_at(OFF_ODD_TASTING_POTION));
  }
}

static void read_scroll()
{
  InventoryItem *item;
  int subtype;

  item = choose_item(rogue_string_at(OFF_ACTION_READ), OBJ_SCROLL);
  if (!item) return;
  if (item->kind != OBJ_SCROLL) {
    epyx_message(rogue_string_at(OFF_NOTHING_ON_IT_TO_READ));
    return;
  }

  subtype = item->subtype;
  rogue_put8(OFF_SCROLL_KNOWN_FLAGS + subtype, 1);
  remove_item(item);
  if (subtype == SCROLL_MAGIC_MAPPING) {
    redraw_dungeon();
    epyx_message(rogue_string_at(OFF_SCROLL_MAP_MESSAGE));
  } else {
    epyx_message(rogue_string_at(OFF_BLANK_SCROLL_MESSAGE));
  }
}

#if ROGUE_WITH_CALLS
static void call_item()
{
  InventoryItem *item;
  char *name;

  item = choose_item(rogue_string_at(OFF_ACTION_CALL), 0);
  if (!item) return;
  if (called_kind_index(item->kind) < 0) {
    epyx_message(rogue_string_at(OFF_CANT_CALL_OBJECT));
    return;
  }
  name = called_names[called_kind_index(item->kind)][item->subtype];
  if (read_prompt_text(rogue_string_at(OFF_CALL_IT_PROMPT), name,
                       CALLED_NAME_MAX))
    epyx_message(rogue_string_at(OFF_WAS_CALLED), name);
  else
    epyx_message("Cancelled.");
}
#endif

static void wield_item()
{
  InventoryItem *item;

  item = choose_item(rogue_string_at(OFF_ACTION_WIELD), OBJ_WEAPON);
  if (!item) return;
  if (item->kind == OBJ_ARMOR) {
    epyx_message(rogue_string_at(OFF_CANT_WIELD_ARMOR));
    return;
  }
  if (item->kind != OBJ_WEAPON) {
    no_appropriate();
    return;
  }
  wielded_weapon_index = (char) inventory_index(item);
  epyx_message(rogue_string_at(OFF_NOW_WIELDING_WEAPON),
               object_name(item->kind, item->subtype),
               'a' + (item - inventory));
}

static void wear_armor()
{
  InventoryItem *item;

  if (worn_armor_index != NO_ITEM_INDEX) {
    epyx_message(rogue_string_at(OFF_ALREADY_WEARING_ARMOR));
    return;
  }
  item = choose_item(rogue_string_at(OFF_ACTION_WEAR), OBJ_ARMOR);
  if (!item) return;
  if (item->kind != OBJ_ARMOR) {
    epyx_message(rogue_string_at(OFF_CANT_WEAR_THAT));
    return;
  }

  worn_armor_index = (char) inventory_index(item);
  epyx_message(rogue_string_at(OFF_NOW_WEARING_ARMOR),
               object_name(item->kind, item->subtype));
}

static void take_off_armor()
{
  InventoryItem *item;
  const char *name;
  int letter;

  if (worn_armor_index == NO_ITEM_INDEX) {
    epyx_message(rogue_string_at(OFF_NO_ARMOR_WORN));
    return;
  }

  item = &inventory[worn_armor_index];
  letter = '?';
  name = "armor";
  if (item) {
    letter = 'a' + (item - inventory);
    name = object_name(item->kind, item->subtype);
  }
  worn_armor_index = NO_ITEM_INDEX;
  epyx_message(rogue_string_at(OFF_TOOK_OFF_ARMOR),
               letter, name);
}

static void apply_ring_effect(item, wearing)
InventoryItem *item;
int wearing;
{
  if (!item) return;
  if (item->subtype == 1) {
    ring_strength_bonus += wearing ? 1 : -1;
    status_invalid = 1;
    draw_status();
  } else if (item->subtype == 4) {
    see_invisible = wearing ? 1 : 0;
  } else if (item->subtype == 6 && wearing) {
    reveal_room(room_at_position(hero_x, hero_y));
    redraw_dungeon();
  }
}

static int choose_ring_hand()
{
  int ch;

  while (1) {
    epyx_message(rogue_string_at(OFF_LEFT_RIGHT_HAND_PROMPT));
    ch = read_key();
    if (ch == KEY_ESCAPE || ch == KEY_CTRL_E) return -1;
    if (ch == 'l' || ch == 'L') return 0;
    if (ch == 'r' || ch == 'R') return 1;
    epyx_message(rogue_string_at(OFF_TYPE_L_OR_R));
  }
}

static void put_on_ring()
{
  InventoryItem *item;
  int index;
  int hand;

  item = choose_item(rogue_string_at(OFF_ACTION_PUT_ON), OBJ_RING);
  if (!item) return;
  if (item->kind != OBJ_RING) {
    epyx_message(rogue_string_at(OFF_CANT_PUT_ON_RING));
    return;
  }
  index = inventory_index(item);

  if (left_ring_index == index || right_ring_index == index) {
    epyx_message(rogue_string_at(OFF_NOW_WEARING_RING),
                 object_name(item->kind, item->subtype), 'a' + index);
    return;
  }

  if (left_ring_index == NO_ITEM_INDEX && right_ring_index == NO_ITEM_INDEX) {
    hand = choose_ring_hand();
    if (hand < 0) return;
  } else if (left_ring_index == NO_ITEM_INDEX) {
    hand = 0;
  } else if (right_ring_index == NO_ITEM_INDEX) {
    hand = 1;
  } else {
    epyx_message(rogue_string_at(OFF_RING_ON_EACH_HAND));
    return;
  }

  if (hand == 0) left_ring_index = (char) index;
  else right_ring_index = (char) index;
  apply_ring_effect(item, 1);
  epyx_message(rogue_string_at(OFF_NOW_WEARING_RING),
               object_name(item->kind, item->subtype), 'a' + index);
}

static void remove_ring()
{
  InventoryItem *item;
  int index;
  int hand;

  if (left_ring_index == NO_ITEM_INDEX && right_ring_index == NO_ITEM_INDEX) {
    epyx_message(rogue_string_at(OFF_NO_RINGS_WORN));
    return;
  }

  if (left_ring_index != NO_ITEM_INDEX && right_ring_index != NO_ITEM_INDEX) {
    hand = choose_ring_hand();
    if (hand < 0) return;
  } else if (left_ring_index != NO_ITEM_INDEX) {
    hand = 0;
  } else {
    hand = 1;
  }

  index = hand == 0 ? left_ring_index : right_ring_index;
  item = &inventory[index];
  if (hand == 0) left_ring_index = NO_ITEM_INDEX;
  else right_ring_index = NO_ITEM_INDEX;
  apply_ring_effect(item, 0);
  epyx_message(rogue_string_at(OFF_WAS_WEARING_RING),
               object_name(item->kind, item->subtype), 'a' + index);
}

static int direction_delta(dx, dy)
int *dx;
int *dy;
{
  int ch;
  int move;

  epyx_message(rogue_string_at(OFF_DIRECTION_PROMPT));
  ch = read_key();
  if (ch == KEY_ESCAPE || ch == KEY_CTRL_E) return 0;
  if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
  move = movement_delta(ch);
  if (move == 0) return 0;

  *dx = 0;
  *dy = 0;
  if (move == 1 || move == 5 || move == 7) *dx = -1;
  if (move == 2 || move == 6 || move == 8) *dx = 1;
  if (move == 3 || move == 5 || move == 6) *dy = -1;
  if (move == 4 || move == 7 || move == 8) *dy = 1;
  return 1;
}

static void throw_item()
{
  RogueObject *obj;
  InventoryItem *item;
  int kind;
  int glyph_ch;
  int subtype;
  int dx;
  int dy;
  int x;
  int y;
  int nx;
  int ny;
  RogueMonster *mon;

  if (!direction_delta(&dx, &dy)) return;
  item = choose_item(rogue_string_at(OFF_ACTION_THROW), 0);
  if (!item) return;

  kind = item->kind;
  glyph_ch = item->glyph;
  subtype = item->subtype;
  x = hero_x;
  y = hero_y;

  while (1) {
    nx = x + dx;
    ny = y + dy;
    mon = monster_at(nx, ny);
    if (mon) {
      mon->hp -= kind == OBJ_WEAPON ? 2 : 1;
      remove_item(item);
      turn_taken = 1;
      if (mon->hp <= 0) {
        defeat_monster(mon);
        message_defeated(mon);
      } else {
        message_you_hit();
      }
      return;
    }
    if (!is_walkable(nx, ny) || object_at(nx, ny)) break;
    x = nx;
    y = ny;
  }

  obj = free_floor_object();
  if (!obj) {
    no_appropriate();
    return;
  }

  remove_item(item);
  obj->kind = (char) kind;
  obj->glyph = (char) glyph_ch;
  obj->subtype = (char) subtype;
  obj->x = (char) x;
  obj->y = (char) y;
  obj->quantity = 1;
  turn_taken = 1;
  if (overlay_visible_position(x, y)) put_at(x, y, glyph_ch);
  draw_hero();
  epyx_message(rogue_string_at(OFF_DROPPED_OBJECT_MESSAGE),
               object_name(kind, subtype));
}

static int zap_ray(dx, dy, damage)
int dx;
int dy;
int damage;
{
  int x;
  int y;
  RogueMonster *mon;

  x = hero_x;
  y = hero_y;
  while (1) {
    x += dx;
    y += dy;
    if (!is_walkable(x, y)) return 0;
    mon = monster_at(x, y);
    if (mon) {
      mon->hp = (char) (mon->hp - damage);
      if (mon->hp <= 0) {
        defeat_monster(mon);
        message_defeated(mon);
      } else {
        message_you_hit();
      }
      return 1;
    }
  }
}

static void zap_item()
{
  InventoryItem *item;
  int subtype;
  int dx;
  int dy;

  if (!direction_delta(&dx, &dy)) return;
  item = choose_item(rogue_string_at(OFF_ACTION_ZAP), OBJ_WAND);
  if (!item) return;
  if (item->kind != OBJ_WAND) {
    epyx_message(rogue_string_at(OFF_CANT_ZAP_WITH_THAT));
    return;
  }

  subtype = item->subtype;
  if (subtype == WAND_LIGHT) {
    reveal_room(room_at_position(hero_x, hero_y));
    rogue_put8(OFF_WAND_KNOWN_FLAGS + subtype, 1);
    redraw_dungeon();
    epyx_message(rogue_string_at(OFF_ROOM_LIT_ARC_LIGHT));
  } else if (subtype == WAND_MAGIC_MISSILE) {
    rogue_put8(OFF_WAND_KNOWN_FLAGS + subtype, 1);
    if (!zap_ray(dx, dy, 4))
      epyx_message(rogue_string_at(OFF_MISSILE_VANISHES));
  } else if (subtype == WAND_STRIKING) {
    rogue_put8(OFF_WAND_KNOWN_FLAGS + subtype, 1);
    if (!zap_ray(dx, dy, 3))
      epyx_message(rogue_string_at(OFF_NOTHING_HAPPENS));
  } else {
    epyx_message("You find nothing.");
  }
  turn_taken = 1;
}

static RogueObject *adjacent_trap()
{
  RogueObject *obj;
  int dx;
  int dy;

  for (dy = -1; dy <= 1; dy++) {
    for (dx = -1; dx <= 1; dx++) {
      obj = object_at(hero_x + dx, hero_y + dy);
      if (obj && obj->kind == OBJ_TRAP) return obj;
    }
  }
  return 0;
}

static void search_here()
{
  RogueObject *obj;
  int dx;
  int dy;

  obj = adjacent_trap();
  turn_taken = 1;
  if (obj && !obj->known) {
    obj->known = 1;
    if (overlay_visible_position(obj->x, obj->y))
      put_at(obj->x, obj->y, obj->glyph);
    draw_hero();
    epyx_message(rogue_string_at(OFF_YOU_FOUND_FORMAT),
                 trap_name_with_article(obj->subtype));
  } else {
    for (dy = -1; dy <= 1; dy++)
      for (dx = -1; dx <= 1; dx++)
        if (map_in_bounds(hero_x + dx, hero_y + dy) &&
            map_tile(hero_x + dx, hero_y + dy) == TILE_SECRET_DOOR) {
          map_tile(hero_x + dx, hero_y + dy) = TILE_DOOR;
          map_seen(hero_x + dx, hero_y + dy) = 1;
          put_at(hero_x + dx, hero_y + dy, glyph(GLYPH_DOOR));
          draw_hero();
          epyx_message("You found a door.");
          return;
        }
    epyx_message("You find nothing.");
  }
}

static void identify_trap()
{
  RogueObject *obj;
  int dx;
  int dy;
  int x;
  int y;

  if (!direction_delta(&dx, &dy)) return;
  x = hero_x + dx;
  y = hero_y + dy;
  obj = object_at(x, y);
  if (!obj || obj->kind != OBJ_TRAP) {
    epyx_message(rogue_string_at(OFF_NO_TRAP_THERE));
    return;
  }
  obj->known = 1;
  if (overlay_visible_position(x, y)) put_at(x, y, obj->glyph);
  epyx_message(rogue_string_at(OFF_YOU_FOUND_FORMAT),
               trap_name_with_article(obj->subtype));
}

static int movement_to_delta(move, dx, dy)
int move;
int *dx;
int *dy;
{
  if (move == 0) return 0;
  *dx = 0;
  *dy = 0;
  if (move == 1 || move == 5 || move == 7) *dx = -1;
  if (move == 2 || move == 6 || move == 8) *dx = 1;
  if (move == 3 || move == 5 || move == 6) *dy = -1;
  if (move == 4 || move == 7 || move == 8) *dy = 1;
  return 1;
}

static void fast_move(dx, dy)
int dx;
int dy;
{
  int steps;
  int nx;
  int ny;

  for (steps = 0; steps < 32; steps++) {
    nx = hero_x + dx;
    ny = hero_y + dy;
    if (!is_walkable(nx, ny)) break;
    try_move(dx, dy);
    if (adjacent_monster()) break;
    if (object_at(hero_x, hero_y)) break;
  }
}

#if ROGUE_WITH_MACROS
static void define_macro()
{
  if (read_prompt_text(rogue_string_at(OFF_MACRO_PROMPT),
                       rogue_string_at(OFF_MACRO_TEXT), MACRO_MAX))
    epyx_message("Macro defined.");
  else
    epyx_message("Cancelled.");
}

static int execute_macro()
{
  char *macro;
  int i;

  macro = rogue_string_at(OFF_MACRO_TEXT);
  if (!macro[0]) {
    epyx_message("No macro defined.");
    return 1;
  }
  for (i = 0; macro[i] && i < MACRO_MAX; i++) {
    if (!command(macro[i])) return 0;
    if (turn_taken) {
      turn_taken = 0;
      epyx_message_start_turn();
      monster_turn();
      maybe_spawn_monster();
    }
  }
  return 1;
}
#endif

static void drop_item()
{
  RogueObject *obj;
  InventoryItem *item;
  int kind;

  if (object_at(hero_x, hero_y)) {
    epyx_message(rogue_string_at(OFF_OBJECT_ALREADY_THERE));
    return;
  }

  item = choose_item(rogue_string_at(OFF_ACTION_DROP), 0);
  if (!item) return;
  kind = item->kind;

  obj = free_floor_object();
  if (!obj) {
    no_appropriate();
    return;
  }

  obj->kind = (char) kind;
  obj->glyph = item->glyph;
  obj->subtype = item->subtype;
  obj->x = (char) hero_x;
  obj->y = (char) hero_y;
  obj->quantity = 1;
  remove_item(item);
  turn_taken = 1;
  draw_hero();
  epyx_message(rogue_string_at(OFF_DROPPED_OBJECT_MESSAGE),
               object_name(kind, obj->subtype));
}

#if ROGUE_WITH_HELP || ROGUE_WITH_DISCOVERED
static int wait_for_space_or_escape_at(y)
int y;
{
  int ch;

  epyx_move_cursor(0, y);
  epyx_reverse_on();
  epyx_write_string(rogue_string_at(OFF_SPACE_CONTINUE_ESC_QUIT));
  epyx_reverse_off();
  epyx_clear_to_eol();
  while (1) {
    ch = read_key();
    if (ch == KEY_ESCAPE || ch == KEY_CTRL_E) return 0;
    if (ch == ' ') return 1;
  }
}

static int wait_for_space_or_escape_after(y)
int y;
{
  int prompt_y;
  int max_y;

  max_y = rogue_get8(OFF_SCREEN_HEIGHT) - 1;
  prompt_y = y + 1;
  if (prompt_y > max_y) prompt_y = max_y;
  if (prompt_y < 0) prompt_y = 0;
  return wait_for_space_or_escape_at(prompt_y);
}
#endif

static void wait_for_inventory_continue()
{
  int ch;
  int y;

  y = rogue_get8(OFF_SCREEN_HEIGHT) - 1;
  if (y < 0) y = 0;
  epyx_move_cursor(0, y);
  epyx_write_string(rogue_string_at(OFF_PRESS_SPACE_CONTINUE));
  epyx_clear_to_eol();
  while (1) {
    ch = read_key();
    if (ch == KEY_ESCAPE || ch == KEY_CTRL_E || ch == ' ') return;
  }
}

static int confirm_quit()
{
  int ch;

  epyx_move_cursor(0, 0);
  epyx_clear_to_eol();
  epyx_write_string(rogue_string_at(OFF_QUIT_CONFIRM));

  ch = read_key();
  if (ch == 'y' || ch == 'Y') {
    epyx_clear_window();
    epyx_move_cursor(0, 0);
    epyx_printf(rogue_string_at(OFF_QUIT_GOLD_FORMAT), player_gold);
    epyx_write_string("\r\n");
    return 0;
  }

  epyx_move_cursor(0, 0);
  epyx_clear_to_eol();
  draw_status();
  draw_hero();
  return 1;
}

#if ROGUE_WITH_HELP
static void show_help_text(text)
char *text;
{
  int y;
  int aborted;
  char line[33];
  int i;

  if (text[0] == 0) {
    epyx_message("Help file not loaded.");
    return;
  }

  epyx_clear_window();
  y = 0;
  aborted = 0;
  while (1) {
    i = 0;
    while (*text && *text != '\r' && i < sizeof(line) - 1) line[i++] = *text++;
    while (*text && *text != '\r') text++;
    if (*text == '\r') text++;
    line[i] = 0;

    if (line[0] == 0) break;
    if (line[0] == 'X') break;
    if (line[0] == 'N') {
      if (!wait_for_space_or_escape_after(y)) {
        aborted = 1;
        break;
      }
      epyx_clear_window();
      y = 0;
      continue;
    }

    epyx_move_cursor(0, y++);
    epyx_write_string(line);
    epyx_clear_to_eol();
    if (y >= rogue_get8(OFF_SCREEN_HEIGHT) - 2) {
      if (!wait_for_space_or_escape_after(y)) {
        aborted = 1;
        break;
      }
      epyx_clear_window();
      y = 0;
    }
  }

  if (!aborted) wait_for_space_or_escape_after(y);
  redraw_dungeon();
}
#endif

static int draw_inventory_lines(kind)
int kind;
{
  int i;
  int row;
  InventoryItem *item;

  row = 0;
  for (i = 0; i < inventory_count; i++) {
    item = &inventory[i];
    if (kind && item->kind != kind) continue;
    epyx_move_cursor(0, row++);
    epyx_write_char('a' + i);
    epyx_write_string(") ");
    epyx_write_string(inventory_object_name(item));
    if (i == wielded_weapon_index) epyx_write_string(
        rogue_string_at(OFF_WEAPON_IN_HAND));
    if (i == worn_armor_index) epyx_write_string(
        rogue_string_at(OFF_ARMOR_BEING_WORN));
    if (i == left_ring_index) epyx_write_string(
        rogue_string_at(OFF_RING_ON_LEFT_HAND));
    if (i == right_ring_index) epyx_write_string(
        rogue_string_at(OFF_RING_ON_RIGHT_HAND));
    epyx_clear_to_eol();
  }
  return row;
}

static void show_inventory()
{
  if (inventory_count == 0) {
    epyx_message(rogue_string_at(OFF_NOT_CARRYING_ANYTHING));
    return;
  }

  epyx_clear_window();
  draw_inventory_lines(0);
  wait_for_inventory_continue();
  redraw_dungeon();
}

static void repeat_message()
{
  epyx_repeat_message();
}

#if ROGUE_WITH_VERSION
static void show_version()
{
  epyx_message(rogue_string_at(OFF_VERSION_FORMAT),
               rogue_get8(0x20), rogue_get8(0x21));
}
#endif

#if ROGUE_WITH_DISCOVERED
static void show_known_objects()
{
  int i;
  int y;

  epyx_clear_window();
  y = 0;
  epyx_move_cursor(0, y++);
  epyx_write_string("Discovered items");

  for (i = 0; i < SCROLL_COUNT && y < rogue_get8(OFF_SCREEN_HEIGHT) - 2; i++)
    if (rogue_get8(OFF_SCROLL_KNOWN_FLAGS + i)) {
      epyx_move_cursor(0, y++);
      epyx_printf("scroll of %s", arena_table_string(OFF_SCROLL_TABLE, i, 4));
    }
  for (i = 0; i < POTION_COUNT && y < rogue_get8(OFF_SCREEN_HEIGHT) - 2; i++)
    if (rogue_get8(OFF_POTION_KNOWN_FLAGS + i)) {
      epyx_move_cursor(0, y++);
      epyx_printf("potion of %s", arena_table_string(OFF_POTION_TABLE, i, 4));
    }
  for (i = 0; i < RING_COUNT && y < rogue_get8(OFF_SCREEN_HEIGHT) - 2; i++)
    if (rogue_get8(OFF_RING_KNOWN_FLAGS + i)) {
      epyx_move_cursor(0, y++);
      epyx_printf("ring of %s", arena_table_string(OFF_RING_TABLE, i, 4));
    }
  for (i = 0; i < WAND_COUNT && y < rogue_get8(OFF_SCREEN_HEIGHT) - 2; i++)
    if (rogue_get8(OFF_WAND_KNOWN_FLAGS + i)) {
      epyx_move_cursor(0, y++);
      epyx_printf("wand of %s", arena_table_string(OFF_WAND_TABLE, i, 4));
    }

  if (y == 1) {
    epyx_move_cursor(0, y++);
    epyx_write_string(rogue_string_at(OFF_NOTHING_DISCOVERED));
  }
  wait_for_space_or_escape_at(y + 1);
  redraw_dungeon();
}
#endif

static void redraw_dungeon()
{
  epyx_clear_window();
  draw_known_area();
  status_invalid = 1;
  draw_status();
  draw_hero();
  reveal_draw_enabled = 1;
}

static void init_inventory()
{
  InventoryItem *item;

  item = add_item(OBJ_WEAPON, glyph(GLYPH_WEAPON), 0, 1);
  item->hit_bonus = 1;
  item->damage_bonus = 1;
  wielded_weapon_index = (char) inventory_index(item);

  item = add_item(OBJ_WEAPON, glyph(GLYPH_WEAPON), 2, 1);
  item->hit_bonus = 1;
  item->damage_bonus = 0;
  add_item(OBJ_WEAPON, glyph(GLYPH_WEAPON), 3, 25 + random_range(15));

  item = add_item(OBJ_ARMOR, glyph(GLYPH_ARMOR), 1, 1);
  item->hit_bonus = 1;
  item->armor_class = 5;
  worn_armor_index = (char) inventory_index(item);

  add_item(OBJ_FOOD, glyph(GLYPH_FOOD), 0, 1);
}

#if ROGUE_WITH_CALLS
static void clear_called_names()
{
  int kind;
  int item;
  int pos;

  for (kind = 0; kind < CALLED_KIND_MAX; kind++)
    for (item = 0; item < CALLED_ITEM_MAX; item++)
      for (pos = 0; pos < CALLED_NAME_MAX; pos++)
        called_names[kind][item][pos] = 0;
}
#endif

static void change_level(delta)
int delta;
{
  if (delta > 0 && (hero_x != stair_x || hero_y != stair_y)) {
    epyx_message(rogue_string_at(OFF_NO_WAY_DOWN));
    return;
  }

  if (delta < 0 && dungeon_level == 1) {
    epyx_message(rogue_string_at(OFF_NO_WAY_UP));
    return;
  }

  dungeon_level = (char) (dungeon_level + delta);
  rogue_put8(OFF_DUNGEON_LEVEL, dungeon_level);
  populate_level();
  show_corner_stars();
  redraw_dungeon();
}

static int movement_delta(ch)
int ch;
{
  if (ch == 'h' || ch == 8) return 1;
  if (ch == 'l' || ch == 9) return 2;
  if (ch == 'k' || ch == 12) return 3;
  if (ch == 'j' || ch == 10 || ch == 13) return 4;
  if (ch == 'y') return 5;
  if (ch == 'u') return 6;
  if (ch == 'b') return 7;
  if (ch == 'n') return 8;
  return 0;
}

static int command(ch)
int ch;
{
  int move;
  int lower;
  int dx;
  int dy;

  lower = ch;
  if (lower >= 'A' && lower <= 'Z') lower += 'a' - 'A';

  /* rogue.asm:L4400 movement command cluster. */
  move = movement_delta(lower);
  if (move) {
    movement_to_delta(move, &dx, &dy);
    if (ch != lower || next_fast_move || persistent_fast_mode) {
      fast_move(dx, dy);
      next_fast_move = 0;
    } else {
      try_move(dx, dy);
    }
  }

  /* rogue.asm:L43AD legal turn commands that are not movement. */
  else if (ch == '.') turn_taken = 1;

  /* rogue.asm:L448A-L44C6 inventory and item command cluster. */
  else if (ch == ',') pickup_here();
#if ROGUE_WITH_CALLS
  else if (ch == 'c') call_item();
#endif
  else if (ch == 'd') drop_item();
  else if (ch == 'e') eat_item();
  else if (ch == 'i') show_inventory();
  else if (ch == 'q') quaff_item();
  else if (ch == 'r') read_scroll();
  else if (ch == 't' || ch == '+') throw_item();
  else if (ch == 'z' || ch == '-') zap_item();
  else if (ch == 'f') next_fast_move = 1;
  else if (ch == 'F') {
    persistent_fast_mode = !persistent_fast_mode;
    rogue_put8(OFF_FAST_MODE_FLAG, persistent_fast_mode);
    next_fast_move = 1;
  }
  else if (ch == 'g') go_over_object = 1;
  else if (ch == 'w') wield_item();
  else if (ch == 'W') wear_armor();
  else if (ch == 'T') take_off_armor();
  else if (ch == 'P') put_on_ring();
  else if (ch == 'R') remove_ring();
  else if (ch == 's') search_here();
  else if (ch == 'I') identify_trap();
#if ROGUE_WITH_DISCOVERED
  else if (ch == 'D') show_known_objects();
#endif
#if ROGUE_WITH_VERSION
  else if (ch == 'v') show_version();
#endif
  else if (ch == KEY_CTRL_R) repeat_message();
#if ROGUE_WITH_MACROS
  else if (ch == 'M') define_macro();
  else if (ch == 'm') return execute_macro();
#endif
#if ROGUE_WITH_SAVE
  else if (ch == 'S') {
    if (save_game_state()) {
      epyx_message(rogue_string_at(OFF_GAME_SAVED_AS),
                   rogue_string_at(OFF_SAVE_FILE_NAME));
      return 0;
    }
    epyx_message("Save failed.");
  }
#endif

  /* rogue.asm:L4505-L451F stairs and symbol/help commands. */
  else if (ch == '>') change_level(1);
  else if (ch == '<') change_level(-1);
#if ROGUE_WITH_HELP
  else if (ch == '?') show_help_text(rogue_help_text);
  else if (ch == '/') show_help_text(rogue_chr_text);
#endif

  /* rogue.asm:L447D quit command. */
  else if (ch == 'Q') return confirm_quit();
  else if (ch == KEY_ESCAPE || ch == KEY_CTRL_E) {
    epyx_message("Cancelled.");
  } else {
    object_name_buf[0] = (char) ch;
    object_name_buf[1] = 0;
    epyx_message(rogue_string_at(OFF_ILLEGAL_COMMAND), object_name_buf);
  }
  return 1;
}

int rogue_game_run(restore_game)
int restore_game;
{
  int ch;
  char clear_greeting;

  dungeon_level = 1;
  player_gold = 0;
  player_hp = 12;
  player_max_hp = 12;
  player_strength = 16;
  wielded_weapon_index = NO_ITEM_INDEX;
  worn_armor_index = NO_ITEM_INDEX;
  left_ring_index = NO_ITEM_INDEX;
  right_ring_index = NO_ITEM_INDEX;
  ring_strength_bonus = 0;
  see_invisible = 0;
  turn_taken = 0;
  inventory_count = 0;
  status_invalid = 1;
  clear_message_next = 0;
  last_command = 0;
  reveal_draw_enabled = 0;
  next_fast_move = 0;
  persistent_fast_mode = rogue_get8(OFF_FAST_MODE_FLAG);
  go_over_object = 0;
  frozen_turns = 0;
  flytrap_hold = 0;
  death_cause = 'K';
  rogue_ignore_signals();
#if ROGUE_HEAP_MAPS
  if (!init_map_storage()) {
    epyx_clear_window();
    epyx_write_string("Cannot allocate map storage.\r\n");
    return 1;
  }
#endif
  epyx_screen_init();
  init_layout();
  terminal_game_mode();
  seed_random();
  init_object_names();
#if ROGUE_WITH_HELP
  preload_text_file("ROGUE/rogue.hlp", rogue_help_text, HELP_TEXT_MAX);
  preload_text_file("ROGUE/rogue.chr", rogue_chr_text, CHR_TEXT_MAX);
#endif
  if (restore_game) {
#if ROGUE_WITH_SAVE
    if (!load_game_state()) {
      epyx_clear_window();
      epyx_write_string("Cannot restore saved game.\r\n");
      terminal_restore();
      return 1;
    }
    persistent_fast_mode = rogue_get8(OFF_FAST_MODE_FLAG);
#else
    epyx_clear_window();
    epyx_write_string("Restore is not in this build.\r\n");
    terminal_restore();
    return 1;
#endif
  } else {
    show_title_screen();
    ask_player_name();
    show_corner_stars();
    centered_text(rogue_get8(OFF_SCREEN_MAX_Y) / 2 + 1, "Loading...");
    rogue_put8(OFF_DUNGEON_LEVEL, dungeon_level);
    rogue_put16(OFF_PLAYER_GOLD, player_gold);
#if ROGUE_WITH_CALLS
    clear_called_names();
#endif
    init_inventory();
    populate_level();
  }
  redraw_dungeon();
  if (restore_game) {
    epyx_message(rogue_string_at(OFF_RESTORE_GREETING),
                 rogue_string_at(OFF_DEFAULT_PLAYER_NAME));
  } else {
    epyx_message(rogue_string_at(OFF_NEW_GAME_GREETING),
                 rogue_string_at(OFF_DEFAULT_PLAYER_NAME));
  }
  clear_greeting = 1;

  while (1) {
    ch = read_key();
    if (ch < 0) break;
    epyx_message_start_turn();
    if (ch == 'a' && last_command) ch = last_command;
    if (clear_greeting || clear_message_next) {
      epyx_message(0);
      clear_greeting = 0;
      clear_message_next = 0;
    }
    if (frozen_turns) {
      frozen_turns--;
      turn_taken = 1;
    } else {
      if (!command(ch)) break;
      if (ch != 'a') last_command = (char) ch;
    }
    if (turn_taken) {
      turn_taken = 0;
      epyx_message_start_turn();
      monster_turn();
      maybe_spawn_monster();
    }
    if (player_hp <= 0) break;
  }

  if (player_hp <= 0) show_death_screen();
  terminal_finish();
  return 0;
}
