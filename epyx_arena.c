#include "epyx_arena.h"

#if ROGUE_HEAP_ARENA
#include <stdlib.h>
#endif

int open();
int read();
int close();

#if ROGUE_HEAP_ARENA
unsigned char *rogue_arena;
#else
unsigned char rogue_arena[ROGUE_ARENA_SIZE];
#endif

int rogue_load_dat(path)
const char *path;
{
  int fd;
  int count;

#if ROGUE_HEAP_ARENA
  if (!rogue_arena) {
    rogue_arena = (unsigned char *) malloc(ROGUE_ARENA_SIZE);
    if (!rogue_arena) return -1;
  }
#endif
  fd = open(path, 1);
  if (fd < 0) return -1;
  count = read(fd, rogue_arena, ROGUE_DAT_EXPECTED_SIZE);
  close(fd);
  return count == ROGUE_DAT_EXPECTED_SIZE ? 0 : -1;
}
