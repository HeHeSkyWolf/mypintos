#include "vm/swap.h"
#include <bitmap.h>
#include <debug.h>
#include <round.h>
#include <stdio.h>
#include <string.h>
#include "devices/block.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "vm/page.h"

static struct block *swap_device;
static struct bitmap *swap_map;
static struct lock swap_lock;

static void swap_out_disk (struct frame_data *frame, struct sup_data *data);

void
free_sector (size_t sector_idx)
{
  bitmap_set_multiple (swap_map, sector_idx, 1, false);
}

void
swap_init (void)
{
  lock_init (&swap_lock);
  swap_device = block_get_role (BLOCK_SWAP);
  if (swap_device == NULL)
    PANIC ("couldn't open swap device");
  size_t swap_map_size = block_size (swap_device) / SECTOR_PER_PAGE;
  swap_map = bitmap_create (swap_map_size);
  if (swap_map == NULL)
    PANIC ("bitmap creation failed--swap device is too large");
}

bool
swap_in (struct sup_data *data)
{
  bool is_lock_held = syscall_lock_held_by_current_thread ();
  if (!is_lock_held) {
    acquire_syscall_lock ();
  }

  uint8_t *kpage = palloc_get_page (PAL_USER);
  struct frame_data *frame = NULL;
    
  /* Get a page of memory. */
  if (kpage == NULL) {
    struct frame_data *victim = select_victim_frame ();
    if (victim == NULL) {
      return false;
    }
    frame = swap_out (victim);
    frame->sup_entry = data;
    kpage = frame->kaddr;
  }
  else {
    frame = create_frame (kpage, data);
    add_frame_to_table (frame);
  }

  struct thread* cur = thread_current ();
  /* Add the page to the process's address space. */
  if (!install_page_by_thread (cur, data->upage, kpage, data->writable) || frame == NULL)
  {
    palloc_free_page (kpage);
    return false; 
  }

  for (size_t i = 0; i < SECTOR_PER_PAGE; i++) {
    block_read (swap_device, data->sector_idx * SECTOR_PER_PAGE + i, kpage + BLOCK_SECTOR_SIZE * i);
  }
  
  free_sector (data->sector_idx);

  frame->is_pinned = false;
  data->is_swapped = false;
  frame->sup_entry = data;

  if (!is_lock_held) {
    release_syscall_lock ();
  }

  return true;
}

struct frame_data *
swap_out (struct frame_data *frame)
{
  frame->is_pinned = true;

  if (!is_swap_init) {
    swap_init ();
    // printf("swap init\n");
    is_swap_init = true;
  }
  
  // printf("swap out\n");

  struct sup_data *data = frame->sup_entry;
  switch (data->type) {
    case VM_ELF:
      /*if dirty?*/
      swap_out_disk (frame, data);
      data->type = VM_ANON;
      /* return NULL if ELF needs to be keep, while loop on getting a free*/
      break;
    case VM_FILE:
      if (pagedir_is_dirty (data->owner->pagedir, data->upage)) {
        file_write (data->file, frame->kaddr, data->page_read_bytes);
      }
      break;
    case VM_ANON:
      swap_out_disk (frame, data);
      break;
  }

  data->is_swapped = true;

  return frame;
}

static void
swap_out_disk (struct frame_data *frame, struct sup_data *data)
{
  // printf("%d\n", data->page_read_bytes);
  lock_acquire (&swap_lock);
  size_t sector_idx = bitmap_scan_and_flip (swap_map, 0, 1, false);
  lock_release (&swap_lock);

  // printf("sector idx: %d\n", sector_idx);

  if (sector_idx == BITMAP_ERROR) {
    PANIC ("bitmap error");
  }

  for (size_t i = 0; i < SECTOR_PER_PAGE; i++) {
    block_write (swap_device, sector_idx * SECTOR_PER_PAGE + i, frame->kaddr + BLOCK_SECTOR_SIZE * i);
  }
  
  data->sector_idx = sector_idx;
}