#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stddef.h>
#include <stdbool.h>
#include "vm/page.h"

void swap_init(void);
bool swap_out(struct spt_entry *spte);
bool swap_in(struct spt_entry *spte);
void swap_free(size_t swap_slot_index);

#endif /* vm/swap.h */