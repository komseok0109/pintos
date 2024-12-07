#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <hash.h>
#include "devices/block.h"
#include "filesys/off_t.h"
#include "vm/frame.h"
#include "threads/thread.h"
#include <stdbool.h>

enum page_type {
  STACK,
  MMAP,
  FILE
};

struct spt_entry 
{
    struct thread* owner; 
    void* page; 
    enum page_type type; 
    struct file* file;
    off_t offset; 
    size_t read_bytes; 
    size_t zero_bytes; 
    size_t swap_index; 
    bool writable; 
    bool pinning; 
    struct hash_elem elem; 
    bool is_swapped;
};

unsigned hash_value (const struct hash_elem *e, void *aux UNUSED);
bool hash_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
bool add_spt_entry(struct spt_entry *p);
struct spt_entry *find_spt_entry(void* addr);
void delete_spt_entry(void* addr);
bool spt_add_file_entry(void* vaddr, struct file* file, off_t offset, size_t read_bytes, size_t zero_bytes, bool writable);
bool spt_add_mmap_entry(void* vaddr, struct file* file, off_t offset, size_t read_bytes, size_t zero_bytes, bool writable); 
bool spt_add_stack_entry(void* vaddr); 
bool load_page_mmap (struct spt_entry *spte);
bool load_page_lazy (struct spt_entry *spte);
bool is_stack_access(void *fault_addr, void *esp);
bool grow_stack(void *fault_addr);
bool install_page_ (void *upage, void *kpage, bool writable);
void free_page(struct hash_elem *h, void* aux UNUSED);

#endif