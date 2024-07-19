#include <list.h>

struct frame_data {
  struct thread *owner;
  void *vaddr;
  struct list_elem elem;
  size_t pfn; /* Physical Frame Number */
};

void frame_table_init (void);
void add_frame_to_table (struct frame_data *frame);
struct frame_data *frame_init (struct thread *t, void *vaddr);