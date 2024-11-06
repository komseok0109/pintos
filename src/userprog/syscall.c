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

static void syscall_handler (struct intr_frame *);
static void check_user_ptr (const void *ptr);
static void check_user_buffer (const void *buffer, unsigned size);

static struct lock filesys_lock;

void
syscall_init (void) 
{
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t* args = ((uint32_t*) f->esp);
  
  check_user_ptr (args);

  switch (args[0]) {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      check_user_ptr (args + 1);
      exit(args[1]);
      break;
    case SYS_EXEC:
      check_user_ptr (args + 1);
      f->eax = exec((const char*) args[1]);
      break;
    case SYS_WAIT:
      check_user_ptr (args + 1);
      f->eax = wait((pid_t) args[1]);
      break;
    case SYS_CREATE:
      check_user_ptr (args + 1);
      check_user_ptr (args + 2);
      f->eax = create((const char*) args[1], (unsigned) args[2]);
      break;
    case SYS_REMOVE:
      check_user_ptr (args + 1);
      f->eax = remove((const char*) args[1]);
      break;
    case SYS_OPEN:
      check_user_ptr (args + 1);
      f->eax = open((const char*) args[1]);
      break;
    case SYS_FILESIZE:
      check_user_ptr (args + 1);
      f->eax = filesize(args[1]);
      break;
    case SYS_READ:
      check_user_ptr (args + 1);
      check_user_ptr (args + 2);
      check_user_ptr (args + 3);
      f->eax = read(args[1], (void*) args[2], (unsigned) args[3]);
      break;
    case SYS_WRITE:
      check_user_ptr (args + 1);
      check_user_ptr (args + 2);
      check_user_ptr (args + 3);
      f->eax = write(args[1], (const void*) args[2], (unsigned) args[3]);
      break;
    case SYS_SEEK:
      check_user_ptr (args + 1);
      check_user_ptr (args + 2);
      seek(args[1], (unsigned) args[2]);
      break;
    case SYS_TELL:
      check_user_ptr (args + 1);
      f->eax = tell(args[1]);
      break;
    case SYS_CLOSE:
      check_user_ptr (args + 1);
      close(args[1]);
      break;
    default:
      exit(-1);
  }
}

void halt (void) {
  shutdown_power_off();
}

void exit (int status) {
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_current()->exit_status = status;
  thread_exit ();
}

pid_t exec (const char *cmd_line) {
  check_user_ptr(cmd_line);
  return process_execute(cmd_line);
}

int wait (pid_t pid) {
  return process_wait(pid);
}

bool create (const char *file, unsigned initial_size) {
  check_user_ptr(file);
  return filesys_create(file, initial_size);
}

bool remove (const char *file) {
  check_user_ptr(file);
  return filesys_remove(file);
}

int open (const char *file) {
  check_user_ptr(file);
  lock_acquire(&filesys_lock);
  struct file *f = filesys_open(file);
  lock_release(&filesys_lock);
  if (f == NULL)
    return -1;
  int fd = thread_add_file(f);
  if (fd == -1)
    file_close(f);
  return fd;
}

int filesize (int fd) {
  int length;
  struct file *f = thread_get_file(fd);
  if (f == NULL)
    return -1;
  lock_acquire(&filesys_lock);
  length = file_length(f);
  lock_release(&filesys_lock);

  return length;
}

int read (int fd, void *buffer, unsigned size) {
  if (fd < 0 || buffer == NULL)
    return -1;
  
  int bytes_read;
  check_user_buffer(buffer);
  if (fd == 0) {
    unsigned i;
    for (i = 0; i < size; i++) {
      if (((char *)buffer)[i] = input_getc() == '\0')
        break;
    }
    return i;
  }

  struct file *f = thread_get_file(fd);

  if (f == NULL)
    return -1;

  lock_acquire(&filesys_lock);
  bytes_read = file_read(f, buffer, size);
  lock_release(&filesys_lock);
  
  return bytes_read;
}

int write (int fd, const void *buffer, unsigned size) {
  if (fd < 0 || buffer == NULL)
    return -1;

  check_user_buffer(buffer);
  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  }
  struct file *f = thread_get_file(fd);
  if (f == NULL)
    return -1;
  return file_write(f, buffer, size);
}

void seek (int fd, unsigned position) {
  struct file *f = thread_get_file(fd);
  if (f != NULL) {
    lock_acquire(&filesys_lock);
    file_seek(f, position);
    lock_release(&filesys_lock);
  }
}

unsigned tell (int fd) {
  struct file *f = thread_get_file(fd);
  if (f == NULL)
    return -1;

  lock_acquire(&filesys_lock);
  unsigned position = file_tell(f);
  lock_release(&filesys_lock);

  return position;
}

void close (int fd) {
  struct file *f = thread_get_file(fd);
  if (f != NULL) {
    lock_acquire(&filesys_lock);
    file_close(f);
    thread_remove_file(fd);
    lock_release(&filesys_lock);
  }
}

static void
check_user_ptr (const void *ptr)
{
  if (!is_user_vaddr(ptr) || pagedir_get_page(thread_current()->pagedir, ptr) == NULL)
    exit(-1);
}

static void check_user_buffer (const void *buffer, unsigned size) {
  char *ptr = (char *) buffer;
  for (unsigned i = 0; i < size; i++) {
    check_user_ptr(ptr + i);
  }
}