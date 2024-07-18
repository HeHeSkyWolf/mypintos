#include <list.h>
#include <hash.h>

/* Supplemental Page Data */
struct sup_data {
  struct thread *owner;
  void *vaddr;
  struct hash_elem hash_elem;
  size_t pfn;
  bool write_only;
  bool is_zero;
  bool is_user;
  uint32_t size;
};

struct sup_data *sup_page_lookup (const void *address, struct hash sup_table);
void sup_page_free (struct hash_elem *e, void *aux UNUSED);