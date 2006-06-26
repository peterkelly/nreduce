#include "nreduce.h"

int verify_noneedclear()
{
#ifdef VERIFICATION
  block *bl;
  int i;
  for (bl = blocks; bl; bl = bl->next)
    for (i = 0; i < BLOCK_SIZE; i++)
      if (bl->cells[i].tag & FLAG_NEEDCLEAR)
        return 0;
#endif
  return 1;
}
