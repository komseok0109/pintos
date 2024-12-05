#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <list.h>
#include <stdbool.h>
#include "vm/page.h"

struct frame 
{
    void *frame_addr;
    struct spt_entry *page;
    struct list_elem elem;
}

void frame_init(void);
void *allocate_frame(struct spt_entry *page);
void free_frame(void *frame_addr);
void *evict_frame(void);
struct frame *choose_victim_clock(void);

#endif