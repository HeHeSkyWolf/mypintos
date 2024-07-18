#include <list.h>

struct frame_data {
  struct thread *owner;
  void *vaddr;
  struct list_elem elem;
  size_t pfn; /* Physical Frame Number */
};

struct frame_data *frame_init (struct thread *t, void *vaddr);