#include <list.h>

struct frame_data {
  struct thread *owner;
  uint8_t *kaddr;
  struct sup_data* sup_entry;
  
  struct list_elem elem;
  
  bool is_pinned;
  bool is_owned;
};

struct frame_data *lru_start;

void frame_table_init (void);
void add_frame_to_table (struct frame_data *frame);
struct frame_data *create_frame (uint8_t *vaddr, struct sup_data *sp_data);
void clear_frame_table (void);
void remove_frame (struct frame_data *frame);
struct frame_data *select_victim_frame (void);