#include <debug.h>
#include <list.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "vm/frame.h"

static struct list frame_table;

void
frame_table_init (void)
{
  list_init (&frame_table);
}

void
add_frame_to_table (struct frame_data *frame)
{
  list_push_back (&frame_table, &frame->elem);
}

struct frame_data *
frame_init (struct thread *t, void *vaddr)
{
  struct frame_data *data = malloc (sizeof (struct frame_data));
  data->vaddr = vaddr;
  data->owner = t;
  /* size of frame? pfn? */
  return data;
}