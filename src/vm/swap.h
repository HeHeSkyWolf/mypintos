#include <stdbool.h>
#include "vm/frame.h"

bool is_swap_init;

void swap_init (void);
bool swap_in (uint8_t *kpage, struct sup_data *data);
bool swap_out (struct frame_data *frame);