#include <debug.h>
#include <list.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "vm/frame.h"

struct frame_data *
frame_init (struct thread *t, void *vaddr)
{
  struct frame_data *data = malloc (sizeof (struct frame_data));
  data->vaddr = vaddr;
  data->owner = t;
  /* size of frame? pfn? */
  return data;
}