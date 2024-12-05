#include "vm/frame.h"
#include <list.h>
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "vm/page.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"

struct list frame_table;
struct lock frame_table_lock;

void 
frame_init(void) 
{
  lock_init(&frame_table_lock); 
  list_init(&frame_table);
}

void *
allocate_frame(struct spt_entry *spte) 
{
  lock_acquire(&frame_table_lock);
  struct frame *f = malloc(sizeof(struct frame));
  if (f == NULL) {
    lock_release(&frame_table_lock);
    return NULL;
  }

  void *frame_addr = palloc_get_page(PAL_USER);
  if (frame_addr == NULL) {
    frame_addr = evict_frame();
    if (frame_addr == NULL) { 
      free(f);
      lock_release(&frame_table_lock);
      return NULL;
    }
  }

  f->frame_addr = frame_addr;
  f->page = spte;
  spte->f = f;
  list_push_back(&frame_table, &f->elem);
  lock_release(&frame_table_lock);
  return frame_addr;
}

void 
free_frame(void *addr) 
{
  lock_acquire(&frame_table_lock);
  struct list_elem *e;
  for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)) {
    struct frame *f = list_entry(e, struct frame, elem);
    if (f->frame_addr == addr) {
        palloc_free_page(f->frame_addr);
        list_remove(e);
        free(f);
        lock_release(&frame_table_lock);
        return;
    }
  }
  lock_release(&frame_table_lock);
}

void *
evict_frame(void) 
{
  struct frame *victim = choose_victim_clock();  

  if (victim == NULL) {
    return NULL; 
  }

  if (pagedir_is_dirty(victim->page->owner->pagedir, victim->page->vaddr)) {
    swap_out(victim->page); 
  }

  pagedir_clear_page(victim->page->owner->pagedir, victim->page->vaddr);
  void *frame_addr = victim->frame_addr;

  list_remove(&victim->elem);
  free(victim);

  return frame_addr;
}

struct frame *
choose_victim_clock(void) 
{
  static struct list_elem *clock_hand = NULL;
  
  if (clock_hand == NULL || clock_hand == list_end(&frame_table))
    clock_hand = list_begin(&frame_table);

  while (true) {
    struct frame *candidate = list_entry(clock_hand, struct frame, elem);
    if (!pagedir_is_accessed(candidate->page->owner->pagedir, candidate->page->vaddr)) {
        return candidate;
    }
    pagedir_set_accessed(candidate->page->owner->pagedir, candidate->page->vaddr, false);
    clock_hand = list_next(clock_hand);
    if (clock_hand == list_end(&frame_table))
     clock_hand = list_begin(&frame_table);
  }
}






