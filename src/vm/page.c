#include <debug.h>
#include <hash.h>
#include "threads/malloc.h"
#include "vm/page.h"
	
/* Returns the page containing the given virtual address,
   or a null pointer if no such page exists. */
struct sup_data *
sup_page_lookup (const void *address, struct hash sup_table)
{
  struct sup_data p;
  struct hash_elem *e;

  p.vaddr = (void *) address;
  e = hash_find (&sup_table, &p.hash_elem);
  struct sup_data *data = hash_entry (e, struct sup_data, hash_elem);
  if (e == NULL) {
    return NULL;
  }
  return data;
}

void
sup_page_free (struct hash_elem *e, void *aux UNUSED)
{
  struct sup_data *data = hash_entry (e, struct sup_data, hash_elem);
  free (data);
}