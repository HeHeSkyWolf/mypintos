#include <stdbool.h>
#include "vm/frame.h"

#define SECTOR_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

bool is_swap_init;

void swap_init (void);
bool swap_in (struct sup_data *data);
struct frame_data *swap_out (struct frame_data *frame);
void free_sector (size_t sector_idx);