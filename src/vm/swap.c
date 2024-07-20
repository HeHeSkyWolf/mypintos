
#include <debug.h>
#include "devices/block.h"
#include "vm/swap.h"

void
swap_in (void)
{
  struct block *swap_disk = block_get_role (BLOCK_SWAP);
  if (swap_disk == NULL)
    PANIC ("couldn't open swap device");
  
}

void
swap_out (void)
{
  int x= 1;
}