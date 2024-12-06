#include "vm/swap.h"
#include "devices/block.h"
#include "threads/synch.h"
#include <bitmap.h>
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/page.h"
#include <stdbool.h>
#include "threads/palloc.h"

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

void
swap_out(void* frame_addr, struct spt_entry* page) {
    lock_acquire(&swap_lock);
    size_t slot_index = bitmap_scan_and_flip(swap_table, 0, 1, false);
    if (slot_index == BITMAP_ERROR) {
        lock_release(&swap_lock);
    }
    size_t i;
    for (i = 0; i < SECTORS_PER_PAGE; i++){
        block_write(swap_block, slot_index * SECTORS_PER_PAGE + i, (uint8_t *) frame_addr + i * BLOCK_SECTOR_SIZE );
    }
    lock_release(&swap_lock); 
    page->swap_index = slot_index;
    page->is_swapped = true;
}

bool
swap_in(struct spt_entry *spte) {
    uint8_t * frame = allocate_frame(PAL_USER, spte->page);
    if (frame == NULL) {
        return false;
    }
    if (!install_page_(spte->page, frame, true)) {
        free_frame(frame);
        return false;
    }
    lock_acquire(&swap_lock);
    spte->is_swapped = false;
    size_t i;
    for (i = 0; i < SECTORS_PER_PAGE; i++)
        block_read (swap_block, spte->swap_index * SECTORS_PER_PAGE + i, frame + i * BLOCK_SECTOR_SIZE);
    bitmap_reset (swap_table, spte->swap_index);
    lock_release(&swap_lock);
    return true;
}

void swap_free(size_t swap_slot_index) {
    lock_acquire(&swap_lock);
    bitmap_reset(swap_table, swap_slot_index);
    lock_release(&swap_lock);
}