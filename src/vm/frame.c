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
create_frame (void *vaddr, struct sup_data *sp_data)
{
  struct frame_data *data = malloc (sizeof (struct frame_data));
  data->owner = thread_current ();
  data->vaddr = vaddr;
  data->sup_entry = sp_data;
  data->is_owned = true;
  
  //???? for eviction
  data->is_pinned = false;
  
  /* size of frame? pfn? */
  return data;
}

void
clear_frame_table (void) {
  // show I free all the page that's owned by the current thread when exiting
  // or kernel auto do that and I just need to free the frames that owned by
  // current thread ?
  if (!list_empty (&frame_table)) {
    struct list_elem *e;
    for (e = list_begin (&frame_table); e != list_end (&frame_table); e = list_next (e)) {
      struct frame_data *frame = list_entry (e, struct frame_data, elem);
      if (frame->owner->tid == thread_current ()->tid) {
        frame->is_owned = false;
      }
    }
  }
}

// struct frame_data *
// select_victim_frame (void)
// {
//   struct frame_data *frame = NULL;

//   if (!list_empty (&frame_table)) {
//     struct list_elem *e;
//     for (e = list_begin (&frame_table); e != list_end (&frame_table); e = list_next (e)) {
      
//     }
//   }
//   return frame;
// }