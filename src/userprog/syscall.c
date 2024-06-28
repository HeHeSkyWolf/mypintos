#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);

static void kernel_exit (int status);
static void syscall_halt (void);
static void syscall_exit (struct intr_frame *);
static void syscall_exec (struct intr_frame *);
static void syscall_wait (struct intr_frame *);
static void syscall_create (struct intr_frame *);

static void syscall_write (struct intr_frame *);



void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static bool
valid_uaddr (const void *uaddr)
{
  /* do i need to check all args address? */

  if (!is_user_vaddr(uaddr))
    return false;

  if (uaddr == NULL)
    return false;
  
  struct thread *t = thread_current ();
  if (pagedir_get_page (t->pagedir, uaddr) == NULL)
    return false;

  // printf("is valid addr\n");
  return true;
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

  if (!valid_uaddr(usrc)) {
    kernel_exit (-1);
  }

  for (; size > 0; size--, dst++, usrc++)
    *dst = get_user (usrc);
}

static void
kernel_exit (int status)
{
  
  struct thread *t = thread_current ();
  printf("%s: exit(%d)\n", t->name, status);
  t->return_status = status;

  if (!list_empty(&t->parent->child->sibling_list)) {
    list_remove (&t->sibling_elem);
  }
  sema_up (&t->wait_sema);
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
  
  struct thread *t = thread_current ();
  printf("%s: exit(%d)\n", t->name, status);
  t->return_status = status;
  f->eax = status;
  if (!list_empty(&t->parent->child->sibling_list)) {
    list_remove (&t->sibling_elem);
  }
  sema_up (&t->wait_sema);
  thread_exit ();
}

static void
syscall_exec (struct intr_frame *f UNUSED) {

}

static void
syscall_wait (struct intr_frame *f UNUSED) {
  int args[1];
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args);

  int status = process_wait ((tid_t) args[0]);
  f->eax = status;
}

static void
syscall_create (struct intr_frame *f UNUSED)
{
  int args[2];
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 2);

  // printf ("   ***fd: %p\n", (void *) args[0]);
  // printf ("   ***fd: %p\n", (char *) args[0]);

  if (!valid_uaddr ((void *) args[0])) {
    kernel_exit (-1);
  }
  
  const char *file = (const char *) args[0];
  off_t initial_size = (off_t) args[1];
  // printf ("   ***fd: %p\n", (void *) file);
  // printf ("   ***buffer address: %d\n", (off_t) args[1]);

  if (file == NULL || initial_size < 0) {
    kernel_exit (-1);
  }

  bool success = filesys_create (file, initial_size);
  f->eax = success;
}

static void
syscall_write (struct intr_frame *f UNUSED) {
  int args[3];
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 3);
  
  int fd = (int) args[0];
  if (fd == 1) {
    // execute the write on STDOUT_FILENO
    putbuf ((const char *)args[1], (size_t) args[2]);
    // set the returned value
    f->eax = args[2];
  }
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  unsigned int syscall_nr;
  int args[3];

  // extract syscall number
  copy_in (&syscall_nr, f->esp, sizeof syscall_nr);
  // printf ("   ***syscall number: %u (should be %u)\n", syscall_nr, SYS_WRITE);

  // extract 3 arguments, fd means file descripter
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * 3);
  // printf ("   ***fd: %d (should be %u)\n", args[0], STDOUT_FILENO);
  // printf ("   ***buffer address: %p\n", args[1]);
  // printf ("   ***size: %u\n", args[2]);

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
    case SYS_WRITE:
      syscall_write (f);
      break;
    default:
      /* kill process like this? */
      kernel_exit (-1);
      break;
  }

  // printf ("system call!\n");
  // thread_exit ();
}
