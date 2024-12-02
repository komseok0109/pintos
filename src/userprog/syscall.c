#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/shutdown.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "devices/input.h"

static void syscall_handler (struct intr_frame *);

static struct lock fs_lock;

void
syscall_init (void) 
{
  lock_init(&fs_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t* args = ((uint32_t*) f->esp);
  
  check_pointer_validity (args);
  switch (args[0]) {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      check_pointer_validity (args + 1);
      exit(args[1]);
      break;
    case SYS_EXEC:
      check_pointer_validity (args + 1);
      f->eax = exec((const char*) args[1]);
      break;
    case SYS_WAIT:
      check_pointer_validity (args + 1);
      f->eax = wait((pid_t) args[1]);
      break;
    case SYS_CREATE:
      check_pointer_validity (args + 1);
      check_pointer_validity (args + 2);
      f->eax = create((const char*) args[1], (unsigned) args[2]);
      break;
    case SYS_REMOVE:
      check_pointer_validity (args + 1);
      f->eax = remove((const char*) args[1]);
      break;
    case SYS_OPEN:
      check_pointer_validity (args + 1);
      f->eax = open((const char*) args[1]);
      break;
    case SYS_FILESIZE:
      check_pointer_validity (args + 1);
      f->eax = filesize(args[1]);
      break;
    case SYS_READ:
      check_pointer_validity (args + 1);
      check_pointer_validity (args + 2);
      check_pointer_validity (args + 3);
      f->eax = read(args[1], (void*) args[2], (unsigned) args[3]);
      break;
    case SYS_WRITE:
      check_pointer_validity (args + 1);
      check_pointer_validity (args + 2);
      check_pointer_validity (args + 3);
      f->eax = write(args[1], (const void*) args[2], (unsigned) args[3]);
      break;
    case SYS_SEEK:
      check_pointer_validity (args + 1);
      check_pointer_validity (args + 2);
      seek(args[1], (unsigned) args[2]);
      break;
    case SYS_TELL:
      check_pointer_validity (args + 1);
      f->eax = tell(args[1]);
      break;
    case SYS_CLOSE:
      check_pointer_validity (args + 1);
      close(args[1]);
      break;
    default:
      exit(-1);
  }
}

void 
halt (void){
  shutdown_power_off();
}

void 
exit (int status){
  struct thread *cur = thread_current();

  if (cur->parent != NULL)
    cur->parent->child_exit_status = status;

  for (int i = 0; i < FD_TABLE_SIZE; i++) {
    if (cur->fd_table[i] != NULL)
      close(i);
  }

  printf ("%s: exit(%d)\n", thread_name(), status);

  thread_exit();
}

pid_t 
exec (const char *cmd_line){
  check_pointer_validity(cmd_line);
  
  pid_t pid;
  lock_acquire(&fs_lock);
  pid = process_execute(cmd_line);
  lock_release(&fs_lock);

  return pid;
}

int 
wait (pid_t pid){
  return process_wait(pid);
}

bool 
create (const char *file, unsigned initial_size) {
  check_pointer_validity(file);
  bool success;
  lock_acquire(&fs_lock);
  success = filesys_create(file, initial_size); 
  lock_release(&fs_lock);
  return success;
}

bool 
remove (const char *file) {
  check_pointer_validity(file);
  bool success;
  lock_acquire(&fs_lock);
  success = filesys_remove(file); 
  lock_release(&fs_lock);
  return success;
}

int 
open (const char *file) {
  check_pointer_validity(file);
  int fd;
  lock_acquire(&fs_lock);
  struct file *f = filesys_open(file);
  fd = f != NULL ? thread_add_file_to_fd_table(f) : -1;
  if (f != NULL && fd == -1)
    file_close(f);
  lock_release(&fs_lock);
  return fd;
}

int 
filesize (int fd) {
  int length;
  lock_acquire(&fs_lock);
  struct file *f = thread_get_file(fd);
  length = f != NULL ? file_length(f) : -1;
  lock_release(&fs_lock);
  return length;
}

int 
read (int fd, void *buffer, unsigned size) {
  if (fd < 0 || buffer == NULL)
    return -1;
  
  int bytes_read;
  check_buffer_validity(buffer, size);
  
  lock_acquire(&fs_lock);
  if (fd == STDIN_FILENO) {
    unsigned i;
    for (i = 0; i < size; i++) {
      if ((((char *)buffer)[i] = input_getc()) == '\0')
        break;
    }
    bytes_read = i;
  }
  else {
    struct file *f = thread_get_file(fd);
    bytes_read = f != NULL ? file_read(f, buffer, size) : -1;
  }
  lock_release(&fs_lock);
  
  return bytes_read;
}

int 
write (int fd, const void *buffer, unsigned size) {
  if (fd < 0 || buffer == NULL)
    return -1;
  check_buffer_validity(buffer, size);
  int bytes_written;

  lock_acquire(&fs_lock);
  if (fd == STDOUT_FILENO) {
    putbuf(buffer, size);
    bytes_written = size;
  }
  else {
    struct file *f = thread_get_file(fd);
    bytes_written = f != NULL ? file_write(f, buffer, size) : -1;
  }
  lock_release(&fs_lock);

  return bytes_written;
}

void 
seek (int fd, unsigned position) {
  lock_acquire(&fs_lock);
  struct file *f = thread_get_file(fd);
  if (f != NULL) 
    file_seek(f, position);
  lock_release(&fs_lock);
}

unsigned 
tell (int fd) {
  int position;
  lock_acquire(&fs_lock);
  struct file *f = thread_get_file(fd);
  position = f != NULL ? file_tell(f) : -1;
  lock_release(&fs_lock);
  return position;
}

void 
close (int fd) {
  lock_acquire(&fs_lock);
  struct file *f = thread_get_file(fd);
  if (f != NULL) {
    file_close(f);
    thread_remove_file_from_fd_table(fd);
  }
  lock_release(&fs_lock);
}

void 
check_pointer_validity (const void* ptr){
  if (ptr == NULL || !is_user_vaddr(ptr) || pagedir_get_page(thread_current()->pagedir, ptr) == NULL)
    exit(-1);
}

void 
check_buffer_validity (const void *buffer, unsigned size) {
  char *ptr = (char *) buffer;
  for (unsigned i = 0; i < size; i++) {
    check_pointer_validity(ptr + i);
  }
}