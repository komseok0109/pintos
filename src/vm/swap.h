#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stddef.h>
#include <stdbool.h>
#include "vm/page.h"

void swap_init(void);
void swap_out(void* frame_addr, struct spt_entry* page);
bool swap_in(struct spt_entry* page);
void swap_free(size_t swap_slot_index);

#endif /* vm/swap.h */