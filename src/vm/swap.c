#include "vm/swap.h"
#include "devices/block.h"
#include "threads/synch.h"
#include <bitmap.h>
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/page.h"
#include <stdbool.h>

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
swap_out(struct spt_entry *spte) {
    lock_acquire(&swap_lock);

    size_t slot_index = bitmap_scan_and_flip(swap_table, 0, 1, false);
    
    if (slot_index == BITMAP_ERROR) {
        lock_release(&swap_lock);
        return false;
    }
    
    block_write(swap_block, slot_index * SECTORS_PER_PAGE, spte->f->frame_addr);

    spt_add_swap_entry(spte->vaddr, slot_index, true);

    lock_release(&swap_lock);
    
    return slot_index;
}

bool
swap_in(struct spt_entry *spte) {
    lock_acquire(&swap_lock);

    struct frame *frame = allocate_frame(spte);

    if (frame == NULL) {
        lock_release(&swap_lock);
        return false;
    }

    block_read(swap_block, spte->swap_index * SECTORS_PER_PAGE, frame->frame_addr);

    install_page_(spte->vaddr, frame->frame_addr, true);

    bitmap_reset(swap_table, spte->swap_index);

    lock_release(&swap_lock);
    return true;
}

void swap_free(size_t swap_slot_index) {
    lock_acquire(&swap_lock);
    bitmap_reset(swap_table, swap_slot_index);
    lock_release(&swap_lock);
}