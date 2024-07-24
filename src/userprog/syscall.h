#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>
#include <debug.h>
#include <list.h>
#include <hash.h>
#include "threads/thread.h"

/* Map region identifier. */
typedef int mapid_t;
#define MAP_FAILED ((mapid_t) -1)

struct mmap_sup {
  struct sup_data *data;
  struct list_elem elem;
};

struct mmaped_file {
  mapid_t mapid;
  struct file *file;
  struct list sp_list;
  struct hash_elem hash_elem;
};

struct lock_with_ctr {
  struct lock lock;
  int ctr;
};

void syscall_init (void);
void kernel_exit (int status);
void acquire_syscall_lock (void);
void release_syscall_lock (void);

unsigned mmap_hash (const struct hash_elem *e, void *aux UNUSED);
bool mmap_less (const struct hash_elem *a_, const struct hash_elem *b_, 
                       void *aux UNUSED);
void mmap_free (struct hash_elem *hash_e, void *aux UNUSED);
struct mmaped_file *create_mmap_file (void);

void close_all_opened_file (struct process *p);

#endif /* userprog/syscall.h */
