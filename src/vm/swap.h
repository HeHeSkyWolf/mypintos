#include <stdbool.h>
#include "vm/frame.h"

#define SECTOR_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

bool is_swap_init;

void swap_init (void);
bool swap_in (uint8_t *kpage, struct sup_data *data);
void swap_out (struct frame_data *frame);
void free_swapped_sup (size_t sector_idx);