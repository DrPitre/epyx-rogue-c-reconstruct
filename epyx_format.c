#include <os.h>
#include <stdarg.h>

#include "epyx_arena.h"
#include "epyx_format.h"
#include "epyx_screen.h"

int read();

static char message_busy;

static int is_digit(ch)
int ch;
{
  return ch >= '0' && ch <= '9';
}

static int str_len(s)
const char *s;
{
  int len;

  len = 0;
  while (s[len]) len++;
  return len;
}

static void out_char(dest, max, pos, ch)
char *dest;
int max;
int *pos;
int ch;
{
  if (*pos < max - 1) dest[*pos] = (char) ch;
  (*pos)++;
}

static void out_spaces(dest, max, pos, count)
char *dest;
int max;
int *pos;
int count;
{
  while (count-- > 0) out_char(dest, max, pos, ' ');
}

static int make_unsigned(value, work)
unsigned int value;
char *work;
{
  char reversed[6];
  int count;
  int i;

  count = 0;
  if (value == 0) {
    work[0] = '0';
    work[1] = 0;
    return 1;
  }

  while (value != 0 && count < 5) {
    reversed[count++] = (char) (value % 10 + '0');
    value = value / 10;
  }

  for (i = 0; i < count; i++) work[i] = reversed[count - i - 1];
  work[count] = 0;
  return count;
}

static void out_field(dest, max, pos, text, width, precision, left_adjust)
char *dest;
int max;
int *pos;
const char *text;
int width;
int precision;
int left_adjust;
{
  int len;
  int copy_len;

  len = str_len(text);
  copy_len = len;
  if (precision > 0 && copy_len > precision) copy_len = precision;

  if (!left_adjust && width > copy_len) {
    out_spaces(dest, max, pos, width - copy_len);
  }

  while (copy_len-- > 0) out_char(dest, max, pos, *text++);

  if (left_adjust && width > len) {
    out_spaces(dest, max, pos, width - len);
  }
}

char *epyx_format_buffer()
{
  return rogue_ptr(OFF_FORMAT_BUFFER);
}

int epyx_vformat(dest, max, fmt, ap)
char *dest;
int max;
const char *fmt;
va_list ap;
{
  int pos;
  int left_adjust;
  int width;
  int precision;
  int ch;
  char work[8];
  char one_char[2];
  char *text;

  pos = 0;

  while (*fmt) {
    ch = *fmt++;
    if (ch != '%') {
      out_char(dest, max, &pos, ch);
      continue;
    }

    left_adjust = 0;
    width = 0;
    precision = 0;

    if (*fmt == '-') {
      left_adjust = 1;
      fmt++;
    }

    while (is_digit(*fmt)) {
      width = width * 10 + *fmt++ - '0';
    }

    if (*fmt == '.') {
      fmt++;
      while (is_digit(*fmt)) {
        precision = precision * 10 + *fmt++ - '0';
      }
    }

    ch = *fmt++;
    if (ch == 's') {
      text = va_arg(ap, char *);
      out_field(dest, max, &pos, text, width, precision, left_adjust);
    } else if (ch == 'c') {
      one_char[0] = (char) va_arg(ap, int);
      one_char[1] = 0;
      out_field(dest, max, &pos, one_char, width, precision, left_adjust);
    } else if (ch == 'u') {
      make_unsigned(va_arg(ap, unsigned int), work);
      out_field(dest, max, &pos, work, width, precision, left_adjust);
    } else if (ch == 'd') {
      make_unsigned(va_arg(ap, int), work);
      out_field(dest, max, &pos, work, width, precision, left_adjust);
    }
  }

  if (pos < max) dest[pos] = 0;
  else dest[max - 1] = 0;
  return pos;
}

int epyx_format(char *dest, int max, const char *fmt, ...)
{
  va_list ap;
  int len;

  va_start(ap, fmt);
  len = epyx_vformat(dest, max, fmt, ap);
  va_end(ap);
  return len;
}

void epyx_printf(const char *fmt, ...)
{
  va_list ap;
  char *buffer;

  buffer = epyx_format_buffer();
  va_start(ap, fmt);
  epyx_vformat(buffer, FORMAT_BUFFER_SIZE, fmt, ap);
  va_end(ap);
  epyx_write_string(buffer);
}

static void wait_for_enter()
{
  char ch;

  do {
    read(0, &ch, 1);
  } while (ch != 13 && ch != 10);
}

static void write_slice(text, count)
char *text;
int count;
{
  char saved;

  saved = text[count];
  text[count] = 0;
  epyx_write_string(text);
  text[count] = saved;
}

static void scroll_delay()
{
  int ticks;

  ticks = 6;
  _os9_sleep(&ticks);
}

static void scroll_message(buffer, from, to, count)
char *buffer;
int from;
int to;
int count;
{
  while (++from <= to) {
    epyx_move_cursor(0, 0);
    write_slice(buffer + from, count);
    epyx_clear_to_eol();
    scroll_delay();
  }
}

static void show_cont()
{
  int width;

  width = rogue_get8(OFF_SCREEN_WIDTH);
  epyx_move_cursor(width - 4, 0);
  epyx_reverse_on();
  epyx_write_string("Cont");
  epyx_reverse_off();
}

static void display_message_buffer(buffer)
char *buffer;
{
  int len;
  int width;
  int shown;
  int chunk;
  int final_start;

  len = str_len(buffer);
  width = rogue_get8(OFF_SCREEN_WIDTH);
  if (width < 10 || len <= width) {
    epyx_write_string(buffer);
    epyx_clear_to_eol();
    rogue_put8(OFF_MESSAGE_LENGTH, len);
    message_busy = 1;
    return;
  }

  chunk = width - 5;
  shown = 0;
  write_slice(buffer, chunk);
  epyx_clear_to_eol();
  show_cont();
  wait_for_enter();
  final_start = len - width;
  if (final_start < 0) final_start = 0;
  scroll_message(buffer, shown, final_start, width);
  epyx_move_cursor(0, 0);
  write_slice(buffer + final_start, width);
  epyx_clear_to_eol();
  rogue_put8(OFF_MESSAGE_LENGTH, len);
  message_busy = 1;
}

void epyx_message(const char *fmt, ...)
{
  va_list ap;
  char *buffer;

  epyx_move_cursor(0, 0);
  if (fmt == 0) {
    epyx_clear_to_eol();
    rogue_put8(OFF_MESSAGE_LENGTH, 0);
    message_busy = 0;
    return;
  }

  if (message_busy && rogue_get8(OFF_MESSAGE_LENGTH)) {
    show_cont();
    wait_for_enter();
    epyx_move_cursor(0, 0);
  }

  buffer = epyx_format_buffer();
  va_start(ap, fmt);
  epyx_vformat(buffer, FORMAT_BUFFER_SIZE, fmt, ap);
  va_end(ap);

  display_message_buffer(buffer);
}

void epyx_repeat_message()
{
  epyx_move_cursor(0, 0);
  display_message_buffer(epyx_format_buffer());
}

void epyx_message_start_turn()
{
  message_busy = 0;
}
