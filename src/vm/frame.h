#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include "threads/palloc.h"

struct frame {
  void *kpage; // 커널 페이지 주소
  void *upage; // 사용자 페이지 주소
};

void frame_init(void);
struct frame *allocate_frame(enum palloc_flags flags, void *upage);
void free_frame(struct frame *f);

#endif /* vm/frame.h */