#include <debug.h>
#include <hash.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include "vm/swap.h"

/* Returns the page containing the given virtual address,
   or a null pointer if no such page exists. */
struct sup_data *
sup_page_lookup (const void *address, struct hash sup_table)
{
  struct sup_data p;
  struct hash_elem *e;

  p.upage = (void *) address;
  e = hash_find (&sup_table, &p.hash_elem);
  if (e == NULL) {
    return NULL;
  }
  
  struct sup_data *data = hash_entry (e, struct sup_data, hash_elem);
  return data;
}

void
sup_page_free (struct hash_elem *e, void *aux UNUSED)
{
  struct sup_data *data = hash_entry (e, struct sup_data, hash_elem);
  
  if (data->is_swapped) {
    free_swapped_sup (data->sector_idx);
  }

  free (data);
}

bool
load_file (uint8_t *kpage, struct sup_data *data)
{
  acquire_syscall_lock ();
  acquire_frame_lock ();
  // printf("page fault vaddr: %d, %p\n", data->owner->tid, data->upage);

  // printf("page fault kvaddr: %p\n", kpage);
  /* Load this page. */
  if (file_read_at (data->file, kpage, data->page_read_bytes, data->offset) != 
      (int) data->page_read_bytes)
    {
      palloc_free_page (kpage);
      release_frame_lock ();
      release_syscall_lock ();
      return false; 
    }
  memset (kpage + data->page_read_bytes, 0, data->page_zero_bytes);

  /* Add the page to the process's address space. */
  if (!install_page (data->upage, kpage, data->writable)) 
    {
      palloc_free_page (kpage);
      release_frame_lock ();
      release_syscall_lock ();
      return false; 
    }

  release_frame_lock ();
  release_syscall_lock ();
  return true;
}

struct sup_data *
create_sup_page (uint8_t *upage, struct file *file, bool writable, off_t ofs,
                 size_t page_read_bytes, size_t page_zero_bytes)
{
  struct sup_data *data = malloc (sizeof (struct sup_data));
  data->owner = thread_current ();
  data->upage = upage;
  data->file = file;
  data->page_read_bytes = page_read_bytes;
  data->page_zero_bytes = page_zero_bytes;
  data->offset = ofs;
  data->writable = writable;
  data->sector_idx = 0;

  return data;
}

bool
grow_stack (uint8_t *kpage, void *rounded_addr)
{
  struct sup_data *data = create_sup_page (rounded_addr, NULL, true, 0, 0, 0);
  data->type = VM_ELF;

  /* Add the page to the process's address space. */
  if (!install_page (data->upage, kpage, data->writable)) {
    free(data);
    palloc_free_page (kpage);
    release_syscall_lock ();
    return false; 
  }

  struct frame_data *frame = create_frame (kpage, data);
  add_frame_to_table (frame);
  if (lru_start == NULL) {
    lru_start = frame;
  }

  hash_insert (&thread_current ()->sup_page_table, &data->hash_elem);
  frame->is_pinned = false;
  return true;
}