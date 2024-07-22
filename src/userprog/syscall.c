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

#include "vm/page.h"

static struct lock syscall_lock;

static void syscall_handler (struct intr_frame *);

static void valid_uaddr (const void *uaddr);
void kernel_exit (int status);
void acquire_syscall_lock (void);
void release_syscall_lock (void);
bool syscall_lock_held_by_current_thread (void);
static struct file_open *find_file_by_fd (struct process *, int fd);
static void close_all_opened_file (struct process *);
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

void 
acquire_syscall_lock (void)
{
  lock_acquire (&syscall_lock);
}

void
release_syscall_lock (void)
{
  lock_release (&syscall_lock);
}

bool
syscall_lock_held_by_current_thread (void)
{
  return lock_held_by_current_thread (&syscall_lock);
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

static void
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

  printf("%s: exit(%d)\n", cur->name, status);
  
  cur->process->return_status = status;
  close_all_opened_file (cur->process);
  file_close (cur->running_file);
  
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
  
  struct thread *cur = thread_current ();

  printf("%s: exit(%d)\n", cur->name, status);
  cur->process->return_status = status;
  f->eax = status;
  close_all_opened_file (cur->process);
  file_close (cur->running_file);
  
  thread_exit ();
}

static void
syscall_exec (struct intr_frame *f UNUSED) {
  int args[1];
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args);
  valid_uaddr ((char *) args[0]);

  const char *cmd_line = (const char *) args[0];

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

  struct file *file = filesys_open (file_name);

  acquire_syscall_lock ();
  if (file == NULL) {
    f->eax = -1;
  } else {
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

  acquire_syscall_lock ();
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

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  unsigned int syscall_nr;
  copy_in (&syscall_nr, f->esp, sizeof syscall_nr);

  struct thread *cur = thread_current ();
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
    default:
      /* kill process like this? sc-boundary-3 */
      kernel_exit (-1);
      break;
  }
}
