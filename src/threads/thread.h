#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include <hash.h>
#include "userprog/syscall.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

#define NICE_DEFAULT 0
#define RECENT_CPU_DEFAULT 0
#define LOAD_AVG_DEFAULT 0

#define FD_TABLE_SIZE 128

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */
    int64_t wakeup_tick; 
    int original_priority;
    struct list donations_list;
    struct lock *waiting_lock;
    struct list_elem donator;
    int nice;
    int recent_cpu;

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */
    
     
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */

    struct thread *parent;
    int child_exit_status;

    bool is_child_loaded;
    struct list children;
    struct list_elem child;
    bool has_parent_waited;

    struct semaphore load_sema;
    struct semaphore wait_sema;
    struct semaphore synch_sema;

    struct file *exec_file;
    struct file *fd_table[FD_TABLE_SIZE];

    struct hash *s_page_table;
    struct list file_mapping_table;
    mapid_t next_mapid;

    void* stack_end;

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

void thread_sleep (int64_t wakeup_tick);
void thread_awake (int64_t current_tick);
int get_next_tick_to_awake (void);
bool compare_wakeup_ticks(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

bool compare_thread_prority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
void thread_preemption(void);
void nested_donation(struct lock *lock, struct thread* cur);
void update_priority (void);
bool compare_thread_donator_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

void recalculate_priority_foreach(struct thread *t);
void recalculate_priority(void);
void increment_recent_cpu(void);
void recalculate_recent_cpu_foreach(struct thread *t);
void recalculate_recent_cpu(void);
void recalculate_load_avg(void);

int thread_add_file_to_fd_table (struct file *file);
struct file *thread_get_file (int fd);
void thread_remove_file_from_fd_table (int fd);
#endif /* threads/thread.h */
