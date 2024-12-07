#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdbool.h>
#include <list.h>

typedef int pid_t;

void syscall_init (void);
void halt (void);
void exit (int status);
pid_t exec (const char *cmd_line);
int sys_wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

void check_pointer_validity (const void* ptr);
void check_buffer_validity (const void* buffer, unsigned size);

typedef int mapid_t;

struct file_mapping 
{
    mapid_t mapid;
    struct file *file;
    void* start_addr;
    size_t page_count;
    struct list_elem elem;
};

mapid_t mmap(int fd, void* addr);
void munmap(mapid_t mapping);

#endif /* userprog/syscall.h */
