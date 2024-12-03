#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "lib/kernel/list.h"

struct list frame_table;
struct lock ft_lock;

void frame_init(void) {
    list_init(&frame_table);
    lock_init(&ft_lock);
}

struct frame *allocate_frame(enum palloc_flags flags, void *upage) {
    lock_acquire(&ft_lock);

    void *frame_addr = palloc_get_page(flags);
    if (frame_addr == NULL) {
        lock_release(&ft_lock);
        return NULL;
    }
    
    struct frame *f = malloc(sizeof(struct frame));
    if (f == NULL) {
        palloc_free_page(frame_addr); 
        lock_release(&ft_lock);
        return NULL;
    }
    
    f->frame_addr = frame_addr;
    f->page = NULL; 
    f->is_allocated = true;
    list_push_back(&frame_table, &f->elem);

    lock_release(&ft_lock);
    return f;
}

void free_frame(struct frame *f) {
    lock_acquire(&ft_lock);

    if (f != NULL && f->is_allocated) {
        palloc_free_page(f->frame_addr);
        list_remove(&f->elem);
        free(f);
    }

    lock_release(&ft_lock);
}