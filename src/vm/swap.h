#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stddef.h>

void swap_init(void);
size_t swap_out(struct spt_entry *page);
void swap_in(struct spt_entry *page);
void swap_free(size_t swap_slot_index);

#endif /* vm/swap.h */