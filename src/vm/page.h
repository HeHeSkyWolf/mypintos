#include <list.h>
#include <hash.h>
#include "filesys/file.h"

/* Supplemental Page Data */
struct sup_data 
{
  struct thread *owner;
  struct hash_elem hash_elem;
  
  size_t pfn;

  uint8_t *upage;
  struct file *file;
  bool writable;
  uint32_t page_read_bytes;
  uint32_t page_zero_bytes;
  off_t offset;

  bool is_elf;
};

struct sup_data *sup_page_lookup (const void *address, struct hash sup_table);
void sup_page_free (struct hash_elem *e, void *aux UNUSED);