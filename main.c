#include "epyx_arena.h"
#include "rogue_game.h"

void exit();
int write();

int main(argc, argv)
int argc;
char **argv;
{
  if (rogue_load_dat("ROGUE/rogue.dat") != 0) {
    write(1, "Cannot load rogue.dat\n", 22);
    exit(1);
  }

  rogue_put8(OFF_COMMAND_LINE, 0);
  return rogue_game_run(argc > 1);
}
