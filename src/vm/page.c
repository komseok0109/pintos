#include "vm/page.h"
#include <stdbool.h>
#include <hash.h>
#include "thread/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "threads/malloc.h"

#define STACK_LIMIT (8 * 1024 * 1024)

unsigned
hash_value (const struct hash_elem *e, void *aux UNUSED)
{
  const struct spt_entry *p = hash_entry (e, struct spt_entry, hash_elem);
  return ((uintptr_t) p->addr) >> PGBITS;
}

bool
hash_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct spt_entry *a = hash_entry (a_, struct spt_entry, hash_elem);
  const struct spt_entry *b = hash_entry (b_, struct spt_entry, hash_elem);

  return a->addr < b->addr;
}

bool 
add_spt_entry(struct spt_entry *p)
{
  return hash_insert(&thread_current()->s_page_table, p->elem) == NULL;
}

struct spt_entry *
find_spt_entry(void* addr)
{
  struct spt_entry temp_entry;
  temp_entry.vaddr = addr;
  struct hash_elem *e = hash_find(&thread_current()->s_page_table, &temp_entry.elem);
  if (e == NULL) {
    return NULL; 
  }
  return hash_entry(e, struct spt_entry, elem);  
}

void 
delete_spt_entry(void* addr)
{
  struct spt_entry temp_entry;
  temp_entry.vaddr = addr;
  struct hash_elem *e = hash_find(&thread_current()->s_page_table, &temp_entry.elem);
  if (e != NULL) {
  struct spt_entry *entry = hash_entry(e, struct spt_entry, hash_elem);
    hash_delete(&thread_current()->s_page_table, &entry->elem);
    free(entry);  
  }   
}

bool 
spt_add_file_entry(void* vaddr, struct file* file, off_t offset, size_t read_bytes, size_t zero_bytes, bool writable)
{
  struct spt_entry *spte = malloc(sizeof(struct spt_entry));
  if (spte == NULL) return false;
  spte->vaddr = vaddr;
  spte->owner = thread_current();
  spte->file = file;
  spte->offset = offset;
  spte->read_bytes = read_bytes;
  spte->zero_bytes = zero_bytes;
  spte->writable = writable;
  spte->type = FILE;

  if (!add_spt_entry(spte)) {
    free(spte);
    return false;
  }

  return true;
}

bool spt_add_mmap_entry(void* vaddr, struct file* file, off_t offset, size_t read_bytes, size_t zero_bytes, bool writable)
{
  struct spt_entry *spte = malloc(sizeof(struct spt_entry));
  if (spte == NULL) return false;
  spte->vaddr = vaddr;
  spte->owner = thread_current();
  spte->file = file;
  spte->offset = offset;
  spte->read_bytes = read_bytes;
  spte->zero_bytes = zero_bytes;
  spte->writable = writable;
  spte->type = MMAP;

  if (!add_spt_entry(spte)) {
    free(spte);
    return false;
  }

  return true;
}

bool spt_add_swap_entry(void *vaddr, size_t swap_index, bool writable){
  struct spt_entry *spte = malloc(sizeof(struct spt_entry));
  if (spte == NULL) return false;

  spte->vaddr = vaddr;
  spte->type = SWAP;
  spte->swap_index = swap_index; 
  spte->writable = writable;

  if (!add_spt_entry(spte)) {
    free(spte);
    return false;
  }

  return true;
}

bool spt_add_stack_entry(void* vaddr, bool writeable){
  struct spt_entry *spte = malloc(sizeof(struct spt_entry));
  if (spte == NULL) return false;
  
  spte->vaddr = vaddr;
  spte->type = STACK;
  spte->writable = writable;
  
  if (!add_spt_entry(spte)) {
    free(spte);
    return false;
  }

  return true;
}

bool spt_add_stack_entry(void *vaddr, struct frame *frame) {
    struct spt_entry *spte = malloc(sizeof(struct spte));
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

bool load_page_mmap (struct spt_entry *spte){
  void *frame = allocate_frame(spte);

  if (frame == NULL) 
    return false;

  if (spte->read_bytes > 0) {
    file_seek(spte->file, spte->file_offset);
    if (file_read(spte->file, frame, spte->read_bytes) != (int)spte->read_bytes) {
        free_free(frame);
        return false;
    }
  }
  memset(frame + spte->read_bytes, 0, spte->zero_bytes);

  if (!pagedir_set_page(spte->owner->pagedir, spte->vaddr, frame, spte->writable)) {
    frame_free(frame);
    return false;
  }
  return true;
}

bool load_page_lazy (struct spt_entry *spte){
  uint8_t* kpage = allocate_frame(spte);    
  if (kpage == NULL)
    return false;
  
  if (file_read (spte->file, kpage, spte->page_read_bytes) != (int) spte->page_read_bytes)
  {
    free_frame(kpage);
    return false; 
  }
  memset (kpage + spte->page_read_bytes, 0, spte->page_zero_bytes)

  if (!install_page (spte->vaddr, kpage, spte->writable)) 
  {
    free_frame (kpage);
    return false; 
  }
  
  return true
}

bool is_stack_access(void *fault_addr, void *esp) {
    return (fault_addr < PHYS_BASE && 
            fault_addr >= esp - 32 && 
            fault_addr >= PHYS_BASE - STACK_LIMIT);
}

bool grow_stack(void *fault_addr) {
    struct thread *cur = thread_current();
    void *page = pg_round_down(fault_addr);

    void* new_frame = allocate_frame(find_spt_entry(page));
    
    if (new_frame == NULL) {
        return false;
    }

    if (!pagedir_set_page(cur->pagedir, page, new_frame->frame_addr, true)) {
        free_frame(new_frame);
        return false;
    }
    
    return spt_add_stack_entry(page, new_frame);
}