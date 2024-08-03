#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include <string.h>
#include "devices/shutdown.h"
#include "devices/input.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

#include "vm/frame.h"
#include "vm/page.h"

static struct lock syscall_lock;

static void syscall_handler (struct intr_frame *);

static void valid_uaddr (const void *uaddr);
void kernel_exit (int status);
bool holding_syscall_lock (void);
void acquire_syscall_lock (void);
void release_syscall_lock (void);
static struct file_open *find_file_by_fd (struct process *, int fd);
static int get_user (const uint8_t *uaddr);
static void syscall_halt (void);
static void syscall_exit (struct intr_frame *);
static void syscall_exec (struct intr_frame *);
static void syscall_wait (struct intr_frame *);
static void syscall_create (struct intr_frame *);
static void syscall_remove (struct intr_frame *);
static void syscall_open (struct intr_frame *);
static void syscall_filesize (struct intr_frame *);
static void syscall_read (struct intr_frame *);
static void syscall_write (struct intr_frame *);
static void syscall_seek (struct intr_frame *);
static void syscall_tell (struct intr_frame *);
static void syscall_close (struct intr_frame *);
unsigned mmap_hash (const struct hash_elem *e, void *aux UNUSED);
bool mmap_less (const struct hash_elem *a_, const struct hash_elem *b_, 
                       void *aux UNUSED);
struct mmaped_file *mmaped_file_lookup (const mapid_t id, 
                                        struct hash mmap_table);
struct mmaped_file *create_mmap_file (void);
static void unmap_map_file (struct hash_elem *hash_e, void *aux UNUSED);
static void syscall_mmap (struct intr_frame *);
static void syscall_munmap (struct intr_frame *);


void
syscall_init (void) 
{
  lock_init (&syscall_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
valid_uaddr (const void *uaddr)
{
  if (!is_user_vaddr(uaddr))
    kernel_exit (-1);

  if (uaddr == NULL)
    kernel_exit (-1);
}

bool
holding_syscall_lock (void)
{
  return lock_held_by_current_thread (&syscall_lock);
}

void 
acquire_syscall_lock (void)
{
  lock_acquire (&syscall_lock);
}

void
release_syscall_lock (void)
{
  if (holding_syscall_lock ()) {
    lock_release (&syscall_lock);
  }
}

static struct file_open *
find_file_by_fd (struct process *p, int fd)
{
  if (!list_empty (&p->file_opened_list)) {
    struct list_elem *e;
    for (e = list_begin (&p->file_opened_list); e != list_end (&p->file_opened_list); e = list_next (e)) {
      struct file_open *f_opened = list_entry (e, struct file_open, file_elem);
      if (f_opened->fd == fd) {
        return f_opened;
      }
    }
  }
  return NULL;
}

void
close_all_opened_file (struct process *p)
{
  if (!list_empty (&p->file_opened_list)) {
    while (!list_empty (&p->file_opened_list)) {
      struct list_elem *e = list_pop_front (&p->file_opened_list);
      struct file_open *f_opened = list_entry (e, struct file_open, file_elem);
      file_close (f_opened->file);
      free (f_opened);
    }
  }
}
	
/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}
 
/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

static void
copy_in  (void *dst_, const void *usrc_, size_t size)
{
  uint8_t *dst = dst_;
  const uint8_t *usrc = usrc_;

  valid_uaddr(usrc);

  for (; size > 0; size--, dst++, usrc++) {
    valid_uaddr(usrc);
    *dst = get_user (usrc);
  }
}

void
kernel_exit (int status)
{
  struct thread *cur = thread_current ();

  printf ("%s: exit(%d)\n", cur->name, status);

  // printf("kernel exit\n");

  if (holding_syscall_lock ()) {
    release_syscall_lock ();
  }
  
  cur->process->return_status = status;
  
  thread_exit ();
}

static void
syscall_halt (void) {
  shutdown_power_off ();
}

static void
syscall_exit (struct intr_frame *f UNUSED)
{
  int args[1];
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args);
  int status = (int) args[0];

  acquire_syscall_lock ();
  struct thread *cur = thread_current ();

  printf ("%s: exit(%d)\n", cur->name, status);
  
  // printf("normal exit\n");

  cur->process->return_status = status;
  f->eax = status;
  release_syscall_lock ();

  thread_exit ();
}

static void
syscall_exec (struct intr_frame *f UNUSED) {
  int args[1];
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args);
  valid_uaddr ((char *) args[0]);

  const char *cmd_line = (const char *) args[0];

  // printf("%d syscall exec\n", thread_current()->tid);

  // acquire_syscall_lock ();
  tid_t tid = process_execute (cmd_line);
  f->eax = tid;
  // release_syscall_lock ();
}

static void
syscall_wait (struct intr_frame *f UNUSED) {
  int args[1];
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args);
  tid_t tid = (tid_t) args[0];

  // printf("%d syscall wait\n", thread_current()->tid);

  int status = process_wait (tid);
  f->eax = status;
}

static void
syscall_create (struct intr_frame *f UNUSED)
{
  int args[2];
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 2);
  valid_uaddr ((void *) args[0]);
  
  const char *file = (const char *) args[0];
  off_t initial_size = (off_t) args[1];

  // printf("%d syscall create\n", thread_current()->tid);

  acquire_syscall_lock ();
  bool success = filesys_create (file, initial_size);
  f->eax = success;
  release_syscall_lock ();
}

static void
syscall_remove (struct intr_frame *f UNUSED)
{
  int args[1];
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args);
  valid_uaddr ((void *) args[0]);

  const char *file = (const char *) args[0];

  // printf("%d syscall remove\n", thread_current()->tid);

  acquire_syscall_lock ();
  bool success = filesys_remove (file);
  f->eax = success;
  release_syscall_lock ();
}

static void
syscall_open (struct intr_frame *f UNUSED)
{
  int args[1];
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args);
  valid_uaddr ((void *) args[0]);

  const char *file_name = (const char *) args[0];
  
  acquire_syscall_lock ();
  struct file *file = filesys_open (file_name);

  // printf("%d syscall open\n", thread_current()->tid);

  if (file == NULL) {
    f->eax = -1;
  } 
  else {
    struct thread *cur = thread_current ();
    cur->next_fd += 1;
    struct file_open *f_open = malloc (sizeof (struct file_open));
    f_open->fd = cur->next_fd;
    f_open->file = file;

    list_push_back (&cur->process->file_opened_list, &f_open->file_elem);
    f->eax = f_open->fd;
  }
  release_syscall_lock ();
}

static void
syscall_filesize (struct intr_frame *f UNUSED)
{
  int args[1];
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args);
  int fd = (int) args[0];

  // printf("%d syscall filesize\n", thread_current()->tid);
  
  acquire_syscall_lock ();
  struct file_open *f_opened = find_file_by_fd (thread_current()->process, fd);
  if (f_opened == NULL) {
    f->eax = 0;
  }
  else {
    f->eax = file_length (f_opened->file);
  }
  release_syscall_lock ();
}

static void
syscall_read (struct intr_frame *f UNUSED)
{
  int args[3];
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 3);

  int fd = (int) args[0];
  void *buffer = (void *) args[1];
  unsigned size = (unsigned) args[2];

  valid_uaddr (buffer);

  // printf("%d syscall read\n", thread_current()->tid);

  acquire_syscall_lock ();
  if (fd == STDIN_FILENO) {
    char *keyboard_input = "";
    for (int i = 0; i < args[2]; i++) {
      char key = input_getc ();
      strlcat (keyboard_input, &key, size + 1);
    }
    memcpy (buffer, keyboard_input, size);
    f->eax = size;
  }
  else {
    struct file_open *f_opened = find_file_by_fd (thread_current()->process, fd);
    if (f_opened == NULL) {
      release_syscall_lock ();
      kernel_exit (-1);
    }
    f->eax = file_read (f_opened->file, buffer, size);
  }
  release_syscall_lock ();
}

static void
syscall_write (struct intr_frame *f UNUSED)
{
  int args[3];
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 3);
  
  int fd = (int) args[0];
  void *buffer = (void *) args[1];
  unsigned size = (unsigned) args[2];

  valid_uaddr (buffer);

  // printf("%d syscall write\n", thread_current()->tid);

  if (!holding_syscall_lock ()) {
    acquire_syscall_lock ();
  }
  if (fd == STDOUT_FILENO) {
    putbuf (buffer, size);
    f->eax = size;
  }
  else {
    struct file_open *f_opened = find_file_by_fd (thread_current()->process, fd);
    if (f_opened == NULL) {
      release_syscall_lock ();
      kernel_exit (-1);
    }
    f->eax = file_write (f_opened->file, buffer, size);
  }
  release_syscall_lock ();
}

static void
syscall_seek (struct intr_frame *f UNUSED)
{
  int args[2];
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 2);

  int fd = (int) args[0];
  unsigned position = (unsigned) args[1];

  // printf("%d syscall seek\n", thread_current()->tid);

  acquire_syscall_lock ();
  struct file_open *f_opened = find_file_by_fd (thread_current()->process, fd);
  if (f_opened != NULL) {
    file_seek (f_opened->file, position);
  }
  release_syscall_lock ();
}

static void
syscall_tell (struct intr_frame *f UNUSED)
{
  int args[1];
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args);

  int fd = (int) args[0];

  // printf("%d syscall tell\n", thread_current()->tid);

  acquire_syscall_lock ();
  struct file_open *f_opened = find_file_by_fd (thread_current()->process, fd);
  if (f_opened == NULL) {
    f->eax = -1;
  }
  else {
    f->eax = file_tell (f_opened->file);
  }
  release_syscall_lock ();
}

static void
syscall_close (struct intr_frame *f UNUSED)
{
  int args[1];
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args);
  int fd = (int) args[0];

  // printf("%d syscall close\n", thread_current()->tid);

  acquire_syscall_lock ();
  struct thread *cur = thread_current ();
  struct file_open *f_opened = find_file_by_fd (cur->process, fd);
  if (f_opened != NULL) {
    list_remove (&f_opened->file_elem);
    file_close (f_opened->file);
    free (f_opened);
  }
  release_syscall_lock ();
}

unsigned
mmap_hash (const struct hash_elem *e, void *aux UNUSED)
{
    const struct mmaped_file *data = hash_entry (e, struct mmaped_file, hash_elem);
    return hash_int (data->mapid);
}

bool
mmap_less (const struct hash_elem *a_, const struct hash_elem *b_, 
           void *aux UNUSED)
{
  const struct mmaped_file *a = hash_entry (a_, struct mmaped_file, hash_elem);
  const struct mmaped_file *b = hash_entry (b_, struct mmaped_file, hash_elem);

  return a->mapid < b->mapid;
}

void
mmap_free (struct hash_elem *e, void *aux UNUSED)
{
  struct mmaped_file *data = hash_entry (e, struct mmaped_file, hash_elem);
  
  struct list_elem *list_e;
  struct thread *cur = thread_current ();
  if (!list_empty (&data->sp_list)) {
    for (list_e = list_begin (&data->sp_list); list_e != list_end (&data->sp_list);
         list_e = list_begin (&data->sp_list)) {
      struct mmap_sup *mmap_data = list_entry (list_e, struct mmap_sup, elem);
      list_remove (list_e);
      if (pagedir_is_dirty (cur->pagedir, mmap_data->data->upage)) {
        file_write_at (mmap_data->data->file, mmap_data->data->upage,
                      mmap_data->data->page_read_bytes, 
                      mmap_data->data->offset);
      }
      free (mmap_data);
    }
  }

  file_close (data->file);
  free (data);
}

static void
unmap_map_file (struct hash_elem *e, void *aux UNUSED)
{
  struct mmaped_file *data = hash_entry (e, struct mmaped_file, hash_elem);
  
  struct list_elem *list_e;
  struct thread *cur = thread_current ();
  if (!list_empty (&data->sp_list)) {
    for (list_e = list_begin (&data->sp_list); list_e != list_end (&data->sp_list);
         list_e = list_begin (&data->sp_list)) {
      struct mmap_sup *mmap_data = list_entry (list_e, struct mmap_sup, elem);
      list_remove (list_e);
      struct frame_data *frame = find_frame (cur, mmap_data->data->upage);
      if (pagedir_is_dirty (cur->pagedir, mmap_data->data->upage)) {
        file_write_at (mmap_data->data->file, frame->kaddr,
                      mmap_data->data->page_read_bytes, 
                      mmap_data->data->offset);
      }
      remove_frame (frame);
      pagedir_clear_page (mmap_data->data->owner->pagedir, mmap_data->data->upage);
      hash_delete (&cur->sup_page_table, &mmap_data->data->hash_elem);
      sup_page_free (&mmap_data->data->hash_elem, NULL);
      free (mmap_data);
    }
  }

  hash_delete (&cur->mmap_table, e);
  file_close (data->file);
  free (data);
}

struct mmaped_file *
mmaped_file_lookup (const mapid_t id, struct hash mmap_table)
{
  struct mmaped_file mm;
  struct hash_elem *e;

  mm.mapid = (mapid_t) id;
  e = hash_find (&mmap_table, &mm.hash_elem);
  if (e == NULL) {
    return NULL;
  }
  
  struct mmaped_file *data = hash_entry (e, struct mmaped_file, hash_elem);
  return data;
}

struct mmaped_file *
create_mmap_file (void)
{
  struct mmaped_file *mm = malloc (sizeof (struct mmaped_file));
  if (mm == NULL) {
    return NULL;
  }

  list_init (&mm->sp_list);
  mm->file = NULL;
  
  struct thread *cur = thread_current ();
  mm->mapid = cur->next_mapid;
  cur->next_mapid++;

  hash_insert (&cur->mmap_table, &mm->hash_elem);
  return mm;
}

static void
syscall_mmap (struct intr_frame *f)
{
  int args[2];
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 2);
  int fd = (int) args[0];
  void *addr = (void *) args[1];

  // valid_uaddr (addr);
  mapid_t mapid = -1;
  
  if (fd < 2 || addr == NULL || (uintptr_t) addr % (uintptr_t) PGSIZE != 0) {
    // printf("mmap fail #1\n");
    f->eax = mapid;
    return;
  }

  struct thread *cur = thread_current ();

  struct file_open *f_opened = find_file_by_fd (cur->process, fd);
  if (f_opened == NULL || file_length (f_opened->file) == 0) {
    // printf("mmap fail #2\n");
    f->eax = mapid;
    return;
  }
  struct file *f_reopened = file_reopen (f_opened->file);
  if (f_reopened == NULL) {
    f->eax = mapid;
    return;
  }

  size_t read_bytes;
  off_t length = file_length (f_reopened);
  off_t ofs = 0;

  struct mmaped_file *map_file = create_mmap_file ();
  if (map_file == NULL) {
    f->eax = mapid;
    return;
  }

  map_file->file = f_reopened;
  while (length > 0) {
    read_bytes = length < PGSIZE ? length : PGSIZE;
    if (addr >= PHYS_BASE - MAX_STACK_SIZE) {
      // printf("mmap fail #3\n");
      f->eax = mapid;
      return;
    }

    /* am i suppose to check the preexisting pages, not the current addr is it in pagedir or not 
       will, i guess i can check the pagedir but what about overlaping the preexisting pages */
    if (sup_page_lookup (addr, cur->sup_page_table) == NULL) {
      struct sup_data *data = create_sup_page (addr, f_reopened, true, ofs, 
                                               read_bytes, PGSIZE - read_bytes);
      if (data == NULL) {
        // printf("mmap fail #4\n");
        f->eax = mapid;
        return;
      }
      data->type = VM_FILE;
      hash_insert (&cur->sup_page_table, &data->hash_elem);

      struct mmap_sup *mmap_data = malloc (sizeof (struct mmap_sup));
      mmap_data->data = data;
      list_push_back (&map_file->sp_list, &mmap_data->elem);
      addr += PGSIZE;
      ofs += PGSIZE;
      length -= PGSIZE;
    }
    else {
      // printf("mmap fail #5\n");
      f->eax = mapid;
      return;
    }
  }

  mapid = map_file->mapid;
  f->eax = mapid;
}

static void
syscall_munmap (struct intr_frame *f)
{
  int args[1];
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args);
  mapid_t mapid = (mapid_t) args[0];

  // printf("mapid %d\n", mapid);

  if (mapid < 0) {
    // printf("munmap fail\n");
    kernel_exit (-1);
  }

  struct mmaped_file *map_file = mmaped_file_lookup (mapid, thread_current ()->mmap_table);
  if (map_file == NULL) {
    kernel_exit (-1);
  }
  unmap_map_file (&map_file->hash_elem, NULL);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  unsigned int syscall_nr;
  copy_in (&syscall_nr, f->esp, sizeof syscall_nr);

  thread_current ()->interrupt_esp = f->esp;

  // struct thread *cur = thread_current ();
  // printf ("%d   ***syscall number: %u\n", cur->tid, syscall_nr);

  switch (syscall_nr) {
    case SYS_HALT:
      syscall_halt ();
      break;
    case SYS_EXIT:
      syscall_exit (f);
      break;
    case SYS_EXEC:
      syscall_exec (f);
      break;
    case SYS_WAIT:
      syscall_wait (f);
      break;
    case SYS_CREATE:
      syscall_create (f);
      break;
    case SYS_REMOVE:
      syscall_remove (f);
      break;
    case SYS_OPEN:
      syscall_open (f);
      break;
    case SYS_FILESIZE:
      syscall_filesize (f);
      break;
    case SYS_READ:
      syscall_read (f);
      break;
    case SYS_WRITE:
      syscall_write (f);
      break;
    case SYS_SEEK:
      syscall_seek (f);
      break;
    case SYS_TELL:
      syscall_tell (f);
      break;
    case SYS_CLOSE:
      syscall_close (f);
      break;
    case SYS_MMAP:
      syscall_mmap (f);
      break;
    case SYS_MUNMAP:
      syscall_munmap (f);
      break;
    default:
      /* kill process like this? sc-boundary-3 */
      kernel_exit (-1);
      break;
  }
}
