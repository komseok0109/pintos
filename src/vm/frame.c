#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/synch.h"

struct lock frame_lock;

void frame_init(void) {
    lock_init(&frame_lock);
}

struct frame *allocate_frame(enum palloc_flags flags, void *upage) {
    lock_acquire(&frame_lock);
    void *kpage = palloc_get_page(flags);
    
    if (kpage == NULL) {
        lock_release(&frame_lock);
        return NULL;
    }
    
    struct frame *f = malloc(sizeof(struct frame));
    f->kpage = kpage;
    f->upage = upage;
    
    lock_release(&frame_lock);
    return f;
}

void free_frame(struct frame *f) {
    lock_acquire(&frame_lock);
    palloc_free_page(f->kpage);
    free(f);
    lock_release(&frame_lock);
}