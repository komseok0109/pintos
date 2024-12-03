#include "vm/page.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "threads/malloc.h"

#define STACK_LIMIT (8 * 1024 * 1024)

bool is_stack_access(void *fault_addr, void *esp) {
    return (fault_addr < PHYS_BASE && 
            fault_addr >= esp - 32 && 
            fault_addr >= PHYS_BASE - STACK_LIMIT);
}

bool grow_stack(void *fault_addr) {
    struct thread *cur = thread_current();
    void *page = pg_round_down(fault_addr);

    struct frame *new_frame = allocate_frame(PAL_USER, page);
    
    if (new_frame == NULL) {
        return false;
    }

    if (!pagedir_set_page(cur->pagedir, page, new_frame->frame_addr, true)) {
        free_frame(new_frame);
        return false;
    }
    
    return spt_add_stack_entry(&cur->spt, page, new_frame);
}

bool spt_add_stack_entry(struct hash *spt, void *vaddr, struct frame *frame) {
    struct page *page = malloc(sizeof(struct page));
    if (page == NULL) {
        return false;
    }

    page->vaddr = vaddr;
    page->frame = frame;
    page->type = PAGE_STACK;

    bool success = hash_insert(spt, &page->hash_elem) == NULL;
    if (!success) {
        free(page);
    } else {
        frame->page = page;
    }
    
    return success;
}

bool spt_add_swap_entry(struct hash *spt, void *vaddr, size_t swap_index) {
    struct page *page = malloc(sizeof(struct page));
    if (page == NULL) {
        return false;
    }

    page->vaddr = vaddr;
    page->swap_index = swap_index;
    page->type = PAGE_SWAP;
    
    struct hash_elem *prev = hash_insert(spt, &page->hash_elem);
    if (prev != NULL) {
        free(page);
        return false;
    }

    return true;
}

struct page* spt_find_page(struct hash *spt, void *va) {
    struct page temp_page;
    temp_page.vaddr = pg_round_down(va);
    
    struct hash_elem* e = hash_find(spt, &temp_page.hash_elem);
    
    return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

bool spt_insert_page(struct hash *spt, struct page *page) {
    return hash_insert(spt, &page->hash_elem) == NULL;
}

bool vm_do_claim_page(struct page *page) {
    struct frame* frame = allocate_frame(PAL_USER, page->vaddr);
    
    if (frame == NULL) {
        return false;
    }
    
    frame->page = page;
    page->frame = frame;

    if (!pagedir_set_page(thread_current()->pagedir, page->vaddr, frame->frame_addr, true)) {
        free_frame(frame);
        return false;
    }

    return true;
}

unsigned page_hash(const struct hash_elem* e, void* aux UNUSED) {
    const struct page* p = hash_entry(e, struct page, hash_elem);
    return hash_bytes(&p->vaddr, sizeof p->vaddr);
}

bool page_less(const struct hash_elem* a, const struct hash_elem* b, void* aux UNUSED) {
    const struct page* pa = hash_entry(a, struct page, hash_elem);
    const struct page* pb = hash_entry(b, struct page, hash_elem);
    return pa->vaddr < pb->vaddr;
}