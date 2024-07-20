#include <debug.h>
#include <hash.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "threads/malloc.h"
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