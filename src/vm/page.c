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
  free (data);
}

bool
load_file (struct sup_data *data)
{
  bool is_lock_held = syscall_lock_held_by_current_thread ();
  if (!is_lock_held) {
    acquire_syscall_lock ();
  }
  // printf("page fault vaddr: %d, %p\n", data->owner->tid, data->upage);
  /* Get a page of memory. */
  uint8_t *kpage = palloc_get_page (PAL_USER);
  if (kpage == NULL) {
    release_syscall_lock ();
    return false;
  }

  // printf("page fault kvaddr: %p\n", kpage);
  /* Load this page. */
  if (file_read_at (data->file, kpage, data->page_read_bytes, data->offset) != 
      (int) data->page_read_bytes)
    {
      palloc_free_page (kpage);
      release_syscall_lock ();
      return false; 
    }
  memset (kpage + data->page_read_bytes, 0, data->page_zero_bytes);

  /* Add the page to the process's address space. */
  if (!install_page (data->upage, kpage, data->writable)) 
    {
      palloc_free_page (kpage);
      release_syscall_lock ();
      return false; 
    }

  struct thread *cur = data->owner;
  pagedir_set_accessed (cur->pagedir, kpage, false);
  /* is it suppose to be kpage or upage, and dirty should be set when is read and write */
  pagedir_set_dirty (cur->pagedir, kpage, false);
  if (!is_lock_held) {
    release_syscall_lock ();
  }
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
  data->is_elf = true;

  return data;
}