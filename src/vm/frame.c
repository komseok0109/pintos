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

struct frame *vm_get_frame(void) {
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
  f->page = NULL; // 아직 페이지와 연결되지 않음
  list_push_back(&frame_table, &f->elem);

  lock_release(&frame_table_lock);
  return f;
}

void *allocate_frame(struct spt_entry *spte) {
  struct frame *f = vm_get_frame();
  if (f == NULL) {
    return NULL;
  }

  f->page = spte;
  spte->f = f;

  return f->frame_addr;
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

void *evict_frame(void) {
  lock_acquire(&frame_table_lock);

  struct frame *victim = choose_victim_clock(); // Clock 알고리즘으로 victim 선택

  if (victim == NULL) {
    lock_release(&frame_table_lock);
    return NULL; 
  }

  if (pagedir_is_dirty(victim->page->owner->pagedir, victim->page->vaddr)) {
    if (!swap_out(victim->page)) { // Dirty 상태라면 스왑 아웃
      lock_release(&frame_table_lock);
      return NULL; // 스왑 실패 시 NULL 반환
    }
  }

  pagedir_clear_page(victim->page->owner->pagedir, victim->page->vaddr); // 페이지 테이블에서 제거
  void *frame_addr = victim->frame_addr;

  list_remove(&victim->elem); // 프레임 테이블에서 제거
  free(victim); // 프레임 구조체 해제

  lock_release(&frame_table_lock);
  return frame_addr; // 교체된 프레임 주소 반환
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






