#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <list.h>
#include <stdbool.h>
#include "vm/page.h"
#include "threads/palloc.h"

struct frame 
{
    void* frame_addr;
    void* page;
    struct thread* owner;
    struct list_elem elem;
};

void frame_init(void);
void *allocate_frame(enum palloc_flags flags, void* page);
void free_frame(void *frame_addr);
void evict_frame(void);
void *find_frame(void* page);
struct frame *choose_victim_clock(void);

#endif