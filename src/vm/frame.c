#include <debug.h>
#include <list.h>
#include <stdio.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"

static struct list frame_table;

void
frame_table_init (void)
{
  lru_start = NULL;
  list_init (&frame_table);
}

void
add_frame_to_table (struct frame_data *frame)
{
  list_push_back (&frame_table, &frame->elem);
}

struct frame_data *
create_frame (uint8_t *vaddr, struct sup_data *sp_data)
{
  struct frame_data *data = malloc (sizeof (struct frame_data));
  data->owner = thread_current ();
  data->kaddr = vaddr;
  data->sup_entry = sp_data;
  data->is_owned = true;
  
  //???? for eviction
  data->is_pinned = false;
  
  /* size of frame? pfn? */
  return data;
}

void
remove_frame (struct frame_data *frame)
{
  list_remove (&frame->elem);
  palloc_free_page (frame->kaddr);
  free(frame);
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

struct frame_data *
select_victim_frame (void)
{
  if (lru_start == NULL) {
    return NULL;
  }

  struct list_elem *start = &lru_start->elem;
  struct frame_data *victim = NULL;

  if (!list_empty (&frame_table)) {
    while (victim == NULL) {
      bool is_missed = false;
      struct list_elem *e;
      for (e = start; e != list_end (&frame_table); e = list_next (e)) {
        struct frame_data *frame = list_entry (e, struct frame_data, elem);
        struct sup_data *data = frame->sup_entry;
        if (!pagedir_is_accessed (data->owner->pagedir, data->upage)) {
          pagedir_set_accessed (data->owner->pagedir, data->upage, true);
          if (is_missed) {
            struct list_elem *next = list_next(e);
            if (next == list_end (&frame_table)) {
              next = list_begin (&frame_table);
            }
            struct frame_data *next_frame = list_entry (e, struct frame_data, elem);
            lru_start = next_frame;
          }
          victim = frame;
        }
        else {
          pagedir_set_accessed (data->owner->pagedir, data->upage, false);
          if (!is_missed) {
            is_missed = true;
          }
        }
      }

      for (e = list_begin (&frame_table); e != start; e = list_next (e)) {
        struct frame_data *frame = list_entry (e, struct frame_data, elem);
        struct sup_data *data = frame->sup_entry;
        if (!pagedir_is_accessed (data->owner->pagedir, data->upage)) {
          pagedir_set_accessed (data->owner->pagedir, data->upage, true);

          if (is_missed) {
            struct list_elem *next = list_next(e);
            if (next == list_end (&frame_table)) {
              next = list_begin (&frame_table);
            }
            struct frame_data *next_frame = list_entry (e, struct frame_data, elem);
            lru_start = next_frame;
          }

          victim = frame;
        }
        else {
          pagedir_set_accessed (data->owner->pagedir, data->upage, false);
          if (!is_missed) {
            is_missed = true;
          }
        }
      }
    }
  }
  return victim;
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