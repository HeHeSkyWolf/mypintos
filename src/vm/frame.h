#include <list.h>

struct frame_data {
  struct thread *owner;
  void *vaddr;
  struct list_elem elem;
  struct sup_data* sup_entry;
  bool is_pinned;
  bool is_owned;
};

void frame_table_init (void);
void add_frame_to_table (struct frame_data *frame);
struct frame_data *create_frame (void *vaddr, struct sup_data *sp_data);
void clear_frame_table (void);