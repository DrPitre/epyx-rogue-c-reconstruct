#include <os.h>
#include "epyx_arena.h"
#include "epyx_screen.h"

int write();

static int string_length(text)
const char *text;
{
  int len;

  len = 0;
  while (text[len]) len++;
  return len;
}

int epyx_screen_init()
{
  int width;
  int height;

  if (_os_gs_scsiz(1, &width, &height) != 0) {
    width = 80;
    height = 24;
  }
  if (width >= 40 && height < 24) height = 24;

  rogue_put8(OFF_SCREEN_WIDTH, width);
  rogue_put8(OFF_SCREEN_HEIGHT, height);
  rogue_put8(OFF_SCREEN_MAX_X, width - 1);
  rogue_put8(OFF_SCREEN_MAX_Y, height - 1);
  epyx_clear_window();
  epyx_cursor_off();
  epyx_move_cursor(0, 0);
  return 0;
}

void epyx_write_char(ch)
int ch;
{
  rogue_arena[OFF_CHAR_BUFFER] = (unsigned char) ch;
  write(1, rogue_arena + OFF_CHAR_BUFFER, 1);
}

void epyx_write_string(text)
const char *text;
{
  write(1, text, string_length(text));
}

void epyx_move_cursor(x, y)
int x;
int y;
{
  rogue_arena[OFF_CURSOR_XY_BUFFER] = 2;
  rogue_arena[OFF_CURSOR_XY_BUFFER + 1] = (unsigned char) (x + 32);
  rogue_arena[OFF_CURSOR_XY_BUFFER + 2] = (unsigned char) (y + 32);
  write(1, rogue_arena + OFF_CURSOR_XY_BUFFER, 3);
}

void epyx_put_at(x, y, ch)
int x;
int y;
int ch;
{
  rogue_arena[OFF_CURSOR_XY_BUFFER] = 2;
  rogue_arena[OFF_CURSOR_XY_BUFFER + 1] = (unsigned char) (x + 32);
  rogue_arena[OFF_CURSOR_XY_BUFFER + 2] = (unsigned char) (y + 32);
  rogue_arena[OFF_CURSOR_XY_BUFFER + 3] = (unsigned char) ch;
  write(1, rogue_arena + OFF_CURSOR_XY_BUFFER, 4);
}

void epyx_clear_window()
{
  epyx_write_char(12);
}

void epyx_clear_to_eol()
{
  epyx_write_char(4);
}

void epyx_cursor_on()
{
  write(1, rogue_arena + OFF_CURSOR_ON, 2);
}

void epyx_cursor_off()
{
  write(1, rogue_arena + OFF_CURSOR_OFF, 2);
}

void epyx_reverse_on()
{
  write(1, rogue_arena + OFF_REVERSE_ON, 2);
}

void epyx_reverse_off()
{
  write(1, rogue_arena + OFF_REVERSE_OFF, 2);
}
