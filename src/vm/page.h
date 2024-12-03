#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <hash.h>
#include "vm/frame.h"

enum page_type {
    PAGE_STACK,
    PAGE_SWAP
};

struct page {
    void *vaddr;
    size_t swap_index;
    enum page_type type;
    struct hash_elem hash_elem;
    struct frame *frame;
};

bool is_stack_access(void *fault_addr, void *esp);
bool grow_stack(void *fault_addr);
bool spt_add_stack_entry(struct hash *spt, void *vaddr, struct frame *frame);
bool spt_add_swap_entry(struct hash *spt, void *vaddr, size_t swap_index);
struct page *spt_find_page(struct hash *spt, void *va);
bool vm_do_claim_page(struct page *page);

/* Supplemental page table management */
void spt_destroy(struct hash *spt);

unsigned page_hash(const struct hash_elem *e, void *aux);
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);

#endif /* vm/page.h */