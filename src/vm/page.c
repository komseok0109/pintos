#include "vm/page.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"

#define STACK_LIMIT (8 * 1024 * 1024) // 8MB 스택 제한

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
        return false; // 할당 실패
    }
    
    if (!pagedir_set_page(cur->pagedir, page, new_frame->kpage, true)) {
        free_frame(new_frame);
        return false;
    }
    
    spt_add_stack_entry(&cur->spt, page, new_frame);
    return true;
}

bool spt_add_stack_entry(struct supplemental_page_table *spt, void *vaddr, struct frame *frame) {
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
    }
    
    return success;
}

bool spt_add_swap_entry(struct hash *spt, void *vaddr, size_t swap_index) {
    struct page *page = malloc(sizeof(struct page));
    if (page == NULL) {
        return false; // 메모리 할당 실패
    }

    page->vaddr = vaddr;
    page->swap_index = swap_index;
    page->type = PAGE_SWAP;

    struct hash_elem *prev = hash_insert(spt, &page->hash_elem);
    if (prev != NULL) {
        free(page); // 입력 실패 시 free
        return false;
    }

    return true;
}

unsigned page_hash(const struct hash_elem *e, void *aux UNUSED) {
    const struct page *p = hash_entry(e, struct page, hash_elem);
    return hash_bytes(&p->vaddr, sizeof p->vaddr);
}

bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
    const struct page *pa = hash_entry(a, struct page, hash_elem);
    const struct page *pb = hash_entry(b, struct page, hash_elem);
    return pa->vaddr < pb->vaddr;
}