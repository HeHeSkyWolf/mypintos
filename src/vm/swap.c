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
#include "vm/page.h"

static struct block *swap_device;
static struct bitmap *swap_map;
static struct lock swap_lock;

static void swap_out_disk (struct frame_data *frame, struct sup_data *data);

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
swap_in (uint8_t *kpage, struct sup_data *data)
{
  for (size_t i = 0; i < SECTOR_PER_PAGE; i++) {
    block_read (swap_device, data->sector_idx * SECTOR_PER_PAGE + i, kpage + BLOCK_SECTOR_SIZE * i);
  }
  
  bitmap_set_multiple (swap_map, data->sector_idx, 1, false);
  
  /* Add the page to the process's address space. */
  if (!install_page (data->upage, kpage, data->writable)) {
    palloc_free_page (kpage);
    return false; 
  }

  return true;
}

bool
swap_out (struct frame_data *frame)
{  
  bool success = false;
  struct sup_data *data = frame->sup_entry;

  // printf("swap out\n");

  switch (data->type) {
    case VM_ELF:
      if (pagedir_is_dirty (data->owner->pagedir, data->upage)) {
        swap_out_disk (frame, data);
        remove_frame (frame);
        data->type = VM_ANON;
        pagedir_clear_page (data->owner->pagedir, data->upage);
        success = true;
      }
      break;
    case VM_FILE:
      if (pagedir_is_dirty (data->owner->pagedir, data->upage)) {
        file_write (data->file, frame->kaddr, data->page_read_bytes);
      }
      remove_frame (frame);
      pagedir_clear_page (data->owner->pagedir, data->upage);
      success = true;
      break;
    case VM_ANON:
      swap_out_disk (frame, data);
      remove_frame (frame);
      pagedir_clear_page (data->owner->pagedir, data->upage);
      success = true;
      break;
  }

  return success;
}

static void
swap_out_disk (struct frame_data *frame, struct sup_data *data)
{
  // printf("%d\n", data->page_read_bytes);
  lock_acquire (&swap_lock);
  size_t sector_idx = bitmap_scan_and_flip (swap_map, 0, 1, false);
  lock_release (&swap_lock);
  if (sector_idx == BITMAP_ERROR) {
    PANIC ("bitmap error");
  }
  data->sector_idx = sector_idx;
  for (size_t i = 0; i < SECTOR_PER_PAGE; i++) {
    block_write (swap_device, sector_idx * SECTOR_PER_PAGE + i, frame->kaddr + BLOCK_SECTOR_SIZE * i);
  }
}