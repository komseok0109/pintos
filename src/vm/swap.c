#include "vm/swap.h"
#include "devices/block.h"
#include "threads/synch.h"
#include <bitmap.h>
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/page.h"

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

static struct block *swap_block;
static struct bitmap *swap_table;
static struct lock swap_lock;

void swap_init(void) 
{
    swap_block = block_get_role(BLOCK_SWAP);
    if (swap_block == NULL) {
        swap_table = bitmap_create(0);
    }
    else {
        swap_table = bitmap_create(block_size(swap_block) / SECTORS_PER_PAGE);
    }
    if (swap_table == NULL) {
        PANIC("Failed to create swap table");
    }
    lock_init(&swap_lock);
}

bool
swap_out(struct spte *spte) {
    lock_acquire(&swap_lock);

    size_t slot_index = bitmap_scan_and_flip(swap_table, 0, 1, false);
    
    if (slot_index == BITMAP_ERROR) {
        lock_release(&swap_lock);
        return false;
    }
    
    block_write(swap_block, slot_index * SECTORS_PER_PAGE, page->frame->frame_addr);

    spt_add_swap_entry(&thread_current()->spt, page->vaddr, slot_index);

    lock_release(&swap_lock);
    
    return slot_index;
}

void swap_in(struct spte *spte) {
    lock_acquire(&swap_lock);

    struct frame *frame = allocate_frame(spte);

    if (frame == NULL) {
        lock_release(&swap_lock);
        PANIC("Failed to allocate frame for swapping in");
    }

    block_read(swap_block, page->swap_index * SECTORS_PER_PAGE, frame->frame_addr);

    install_page(page->vaddr, frame->frame_addr, true);

    bitmap_reset(swap_table, page->swap_index);

    lock_release(&swap_lock);
}

void swap_free(size_t swap_slot_index) {
    lock_acquire(&swap_lock);
    bitmap_reset(swap_table, swap_slot_index);
    lock_release(&swap_lock);
}