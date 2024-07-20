#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>

void syscall_init (void);
void kernel_exit (int status);
void acquire_syscall_lock (void);
void release_syscall_lock (void);
bool syscall_lock_held_by_current_thread (void);

#endif /* userprog/syscall.h */
