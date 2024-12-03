#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <hash.h>

enum page_type {
    PAGE_STACK,  // 스택 페이지
    PAGE_SWAP    // 스왑된 페이지
};

struct page {
    void *vaddr;                // 가상 주소
    size_t swap_index;          // 스왑 인덱스
    enum page_type type;        // 페이지 유형
    struct hash_elem hash_elem; // 해시 요소
};

bool is_stack_access(void *fault_addr, void *esp); // 스택 접근 확인
bool grow_stack(void *fault_addr);                 // 스택 확장
bool spt_add_stack_entry(struct hash *spt, void *vaddr, struct frame *frame); // 스택 엔트리 추가
bool spt_add_swap_entry(struct hash *spt, void *vaddr, size_t swap_index);    // 스왑 엔트리 추가

/* 해시 함수 관련 */
unsigned page_hash(const struct hash_elem *e, void *aux);  // 해시 계산
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux); // 비교 함수

#endif /* vm/page.h */