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
#include "threads/malloc.h"
#include <round.h>
#include "vm/page.h"

static void syscall_handler (struct intr_frame *);

static struct lock fs_lock;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&fs_lock);
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
      f->eax = exec((const char*) args[1]);
      break;
    case SYS_WAIT:
      f->eax = sys_wait((pid_t) args[1]);
      break;
    case SYS_CREATE:
      f->eax = create((const char*) args[1], (unsigned) args[2]);
      break;
    case SYS_REMOVE:
      f->eax = remove((const char*) args[1]);
      break;
    case SYS_OPEN:
      f->eax = open((const char*) args[1]);
      break;
    case SYS_FILESIZE:
      f->eax = filesize(args[1]);
      break;
    case SYS_READ:
      check_pointer_validity(args + 1);
      check_pointer_validity(args + 2);
      check_pointer_validity(args + 3);
      f->eax = read(args[1], (void*) args[2], (unsigned) args[3]);
      break;
    case SYS_WRITE:
      f->eax = write(args[1], (const void*) args[2], (unsigned) args[3]);
      break;
    case SYS_SEEK:
      seek(args[1], (unsigned) args[2]);
      break;
    case SYS_TELL:
      f->eax = tell(args[1]);
      break;
    case SYS_CLOSE:
      close(args[1]);
      break;
    case SYS_MMAP:
      f->eax = mmap(args[1], (void*) args[2]);
      break;
    case SYS_MUNMAP:
      munmap(args[1]);
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
sys_wait (pid_t pid){
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
  struct file *f = filesys_open (file);
  if (f == NULL)
    success = false;
  else
  {
      file_close (f);
      success = filesys_remove (file);
  }
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
    if (fd < 0) {
        return -1;
    } 
    // if (buffer < 0x08084000 )
    //   exit(-1);
    if (buffer == NULL || !is_user_vaddr(buffer)) {
        exit(-1);
    }
    if (buffer + size == NULL || !is_user_vaddr(buffer+size)){
      exit(-1);
    }
    void* buffer_ = pg_round_down(buffer);
    for (unsigned i = 0; i + buffer_ <= buffer + size; i += PGSIZE) {
        if (find_spt_entry(buffer_+i) == NULL && buffer_ + i < thread_current()->stack_end){
          exit(-1);
        }
        void *page = pagedir_get_page(thread_current()->pagedir, buffer_ + i);
        if (page == NULL) {
            if (!grow_stack(buffer_ + i)) {
                exit(-1); 
            }
        }
    }
    if (fd == 0) {
        unsigned bytes_read = 0;
        while (bytes_read < size) {
            char c = input_getc(); 
            ((char *)buffer)[bytes_read++] = c;
            if (c == '\n') break;
        }
        return bytes_read;
    }
    struct file *f = thread_get_file(fd);
    if (f == NULL) {
        return -1; 
    }
    lock_acquire(&fs_lock); 
    int bytes_read = file_read(f, buffer, size);
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
    struct spt_entry temp_entry;
    temp_entry.file = f;
    struct hash_elem *e = hash_find(thread_current()->s_page_table, &temp_entry.elem);
    if (e != NULL) 
      load_page_mmap(hash_entry(e, struct spt_entry, elem));  
    thread_remove_file_from_fd_table(fd);
    file_close(f);
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

mapid_t mmap(int fd, void *addr) {
  if (addr == NULL || pg_ofs(addr) != 0 || fd <= 1 || !is_user_vaddr(addr)) return -1;

  struct thread *cur = thread_current();

  lock_acquire(&fs_lock);
  struct file *file = file_reopen(thread_get_file(fd));
  if (file == NULL || file_length(file) == 0) {
    lock_release(&fs_lock);
    return -1;
  }
  lock_release(&fs_lock);
  size_t file_size = file_length(file);
  size_t page_count = DIV_ROUND_UP(file_size, PGSIZE);
  for (size_t i = 0; i < page_count; i++) {
    void *page_addr = addr + i * PGSIZE;
    if (find_spt_entry(page_addr) != NULL)
      return -1;
  }

  struct file_mapping *mapping = malloc(sizeof(struct file_mapping));
  if (mapping == NULL) return -1;

  mapping->mapid = cur->next_mapid++;
  mapping->file = file;
  mapping->start_addr = addr;
  mapping->page_count = page_count;
  list_push_back(&cur->file_mapping_table, &mapping->elem);

  for (size_t i = 0; i < page_count; i++) {
    size_t offset = i * PGSIZE;
    size_t read_bytes = (file_size > offset + PGSIZE) ? PGSIZE : file_size - offset;
    size_t zero_bytes = PGSIZE - read_bytes;
    if (!spt_add_mmap_entry(addr + offset, file, offset, read_bytes, zero_bytes, true)) {
      munmap(mapping->mapid); 
      return -1;
      }
  }
  return mapping->mapid;
}

void munmap(mapid_t mapping) {
  struct thread *cur = thread_current();
  struct list_elem *e;

  for (e = list_begin(&cur->file_mapping_table); e != list_end(&cur->file_mapping_table); e = list_next(e)) {
    struct file_mapping *m = list_entry(e, struct file_mapping, elem);

    if (m->mapid == mapping) {
      for (size_t i = 0; i < m->page_count; i++) {
        void *page_addr = m->start_addr + i * PGSIZE;
        struct spt_entry *spte = find_spt_entry(page_addr);

        if (spte != NULL && pagedir_is_dirty(cur->pagedir, spte->page)) {
          file_write_at(m->file, spte->page, spte->read_bytes, spte->offset);
        }

        free_frame(pagedir_get_page(cur->pagedir, spte->page));
        pagedir_clear_page(cur->pagedir, spte->page);
        delete_spt_entry(spte);
      }

      file_close(m->file);
      list_remove(&m->elem);
      free(m);
      return;
    }
  }
}