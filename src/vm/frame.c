#include <debug.h>
#include <list.h>
#include <stdio.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/swap.h"

static struct list frame_table;
static struct list_elem *lru_start;

static size_t max;
static int cnt = 0;
static void count_evict (void);

void
frame_table_init (void)
{
  lru_start = NULL;
  list_init (&frame_table);
  max = 0;
}

static void
max_frame (void)
{
  if (max < list_size (&frame_table)) {
    max = list_size (&frame_table);
  }
  printf("Max frame table size: %d\n", max);
}

void
add_frame_to_table (struct frame_data *frame)
{
  list_push_back (&frame_table, &frame->elem);
  // max_frame ();
}

static void
count_evict (void)
{
  cnt++;
  printf("cnt: %d\n", cnt);
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
  data->is_pinned = true;
  
  /* size of frame? pfn? */
  return data;
}

void
remove_frame (struct frame_data *frame)
{
  // printf("freeing frame: %p\n", frame->kaddr);
  if (frame->sup_entry->is_swapped) {
    free_sector (frame->sup_entry->sector_idx);
  }
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
        remove_frame (frame);
      }
    }
  }
}

struct frame_data *
select_victim_frame (void)
{
  struct frame_data *victim = NULL;

  if (!list_empty (&frame_table)) {
    if (lru_start == NULL) {
      lru_start = list_begin (&frame_table);
    }

    struct frame_data *frame = list_entry (lru_start, struct frame_data, elem);
    struct sup_data *data = frame->sup_entry;
    
    while (victim == NULL) {
      // printf("current frame: %p\n", frame->kaddr);
      if (!frame->is_pinned) {
        if (!pagedir_is_accessed (data->owner->pagedir, data->upage)) {
          struct list_elem *next = list_next(lru_start);
          if (next == list_end (&frame_table)) {
            next = list_begin (&frame_table);
          }
          
          lru_start = next;
          victim = frame;
          count_evict ();
        }
        else {
          pagedir_set_accessed (data->owner->pagedir, data->upage, false);
        }
      }

      lru_start = list_next (lru_start);
      if (lru_start == list_end (&frame_table)) {
        lru_start = list_begin (&frame_table);
      }
      frame = list_entry (lru_start, struct frame_data, elem);
      data = frame->sup_entry;
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