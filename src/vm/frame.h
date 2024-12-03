#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include "threads/palloc.h"
#include "threads/synch.h"
#include "lib/kernel/list.h"

struct frame {
    void *frame_addr;
    struct spt_entry *page;
    bool is_allocated;
    struct list_elem elem;
};

extern struct list frame_table;
extern struct lock ft_lock;

void frame_init(void);
struct frame *allocate_frame(enum palloc_flags flags, void *upage);
void free_frame(struct frame *f);

#endif /* vm/frame.h */