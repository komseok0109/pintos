#include "vm/page.h"
#include <stdbool.h>
#include <hash.h>
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include <string.h>
#include "userprog/process.h"
#include "vm/swap.h"

#define STACK_LIMIT (8 * 1024 * 1024)

unsigned
hash_value (const struct hash_elem *e, void *aux UNUSED)
{
  return ((uintptr_t) hash_entry (e, struct spt_entry, elem)->page) >> PGBITS;
}

bool
hash_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  return hash_entry (a_, struct spt_entry, elem)->page < hash_entry (b_, struct spt_entry, elem)->page;
}

bool 
add_spt_entry(struct spt_entry *p)
{
  return hash_insert(thread_current()->s_page_table, &p->elem) == NULL;
}

struct spt_entry *
find_spt_entry(void* addr)
{
  struct spt_entry temp_entry;
  temp_entry.page = pg_round_down(addr);
  struct hash_elem *e = hash_find(thread_current()->s_page_table, &temp_entry.elem);
  if (e == NULL) {
    return NULL; 
  }
  return hash_entry(e, struct spt_entry, elem);  
}

void 
delete_spt_entry(void* addr)
{
  struct spt_entry temp_entry;
  temp_entry.page = addr;
  struct hash_elem *e = hash_find(thread_current()->s_page_table, &temp_entry.elem);
  if (e != NULL) {
    struct spt_entry *entry = hash_entry(e, struct spt_entry, elem);
    hash_delete(thread_current()->s_page_table, &entry->elem);
    free(entry);  
  }   
}

bool 
spt_add_file_entry(void* vaddr, struct file* file, off_t offset, size_t read_bytes, size_t zero_bytes, bool writable)
{
  struct spt_entry *spte = malloc(sizeof(struct spt_entry));
  if (spte == NULL) return false;
  spte->page = vaddr;
  spte->owner = thread_current();
  spte->file = file;
  spte->offset = offset;
  spte->read_bytes = read_bytes;
  spte->zero_bytes = zero_bytes;
  spte->writable = writable;
  spte->is_swapped = false;
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
  spte->page = vaddr;
  spte->owner = thread_current();
  spte->file = file;
  spte->offset = offset;
  spte->read_bytes = read_bytes;
  spte->zero_bytes = zero_bytes;
  spte->writable = writable;
  spte->type = MMAP;
  spte->is_swapped = false;

  if (!add_spt_entry(spte)) {
    free(spte);
    return false;
  }
  return true;
}


bool spt_add_stack_entry(void *vaddr) {
  struct spt_entry *spte = malloc(sizeof(struct spt_entry));
  if (spte == NULL) {
    return false;
  }  

  spte->page = vaddr;
  spte->writable = true;
  spte->owner = thread_current();
  spte->is_swapped = false;
  spte->type = STACK;

  if (!add_spt_entry(spte)) {
    free(spte);
    return false;
  }

  return true;
}

bool load_page_mmap (struct spt_entry *spte){
  void *frame = allocate_frame(PAL_USER|PAL_ZERO, spte->page);

  if (frame == NULL) 
    return false;

  if (spte->read_bytes > 0) {
    off_t read_bytes = file_read_at (spte->file, frame, spte->read_bytes, spte->offset);
    if (read_bytes != (int) spte->read_bytes) {
      free_frame(frame);
      return false;
    }
    memset (frame + spte->read_bytes, 0, spte->zero_bytes);
  }
  if (!install_page_ (spte->page, frame, spte->writable)) 
  {
    free_frame(frame);
    return false; 
  }   
  spte->is_swapped = false;
  return true;
}

bool load_page_lazy (struct spt_entry *spte){
  enum palloc_flags flags = spte->zero_bytes == PGSIZE ? PAL_USER | PAL_ZERO : PAL_USER;
  uint8_t* frame = allocate_frame(flags, spte->page);
  if (frame == NULL) return false;
  if (spte->read_bytes > 0) {
    off_t read_bytes = file_read_at (spte->file, frame, spte->read_bytes, spte->offset);
    if (read_bytes != (int) spte->read_bytes) {
      free_frame(frame);
    }
    memset (frame + spte->read_bytes, 0, spte->zero_bytes);
  }
  if (!install_page_ (spte->page, frame, spte->writable)) 
  {
    free_frame(frame);
    return false; 
  }   
  spte->is_swapped = false;
  return true;
}

bool is_stack_access(void *fault_addr, void *esp) {
    return (fault_addr >= PHYS_BASE - STACK_LIMIT && esp - 32 <= fault_addr);
}

bool grow_stack(void *addr) {
    void *page = pg_round_down(addr);

    if (!spt_add_stack_entry(page))
      return false;

    void* new_frame = allocate_frame(PAL_USER ,page);
    
    if (new_frame == NULL) {
      delete_spt_entry(page);
      return false;
    }

    if (!install_page_ (page, new_frame, true)) 
    {
      delete_spt_entry(page);
      free_frame(new_frame);
      return false; 
    }  
    
    return true;
}

bool
install_page_ (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

void free_page(struct hash_elem *h, void* aux UNUSED) {
  struct spt_entry* spte = hash_entry(h, struct spt_entry, elem);
  void * f = find_frame(spte->page); 
  if (f != NULL)
    free_frame(f);
  swap_free(spte->swap_index);
  free(spte);
}