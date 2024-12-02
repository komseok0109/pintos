#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/fixed-point.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

static struct list sleep_list;
static int64_t next_tick_to_awake;
int load_avg;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);
  list_init (&sleep_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->nice = NICE_DEFAULT;
  initial_thread->recent_cpu = RECENT_CPU_DEFAULT;

  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  load_avg = LOAD_AVG_DEFAULT;

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  struct thread *parent = thread_current();
  if (thread_mlfqs) {
    t->nice = parent->nice;
    t->recent_cpu = parent->recent_cpu;
  }

  t->parent = parent;
  list_push_back(&parent->children, &t->child);


  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  thread_unblock (t);

  if (thread_mlfqs){
    recalculate_priority_foreach(t);
  }

  thread_preemption(); 

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED); 
  list_insert_ordered(&ready_list, &t->elem, compare_thread_prority, NULL);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread)  
      list_insert_ordered(&ready_list, &cur->elem, compare_thread_prority, NULL);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  if (thread_mlfqs) 
    return;
  thread_current()->original_priority = new_priority;
  update_priority();
  thread_preemption(); 
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice) {
  enum intr_level old_level = intr_disable();
  struct thread *cur = thread_current();
  cur->nice = nice;
  recalculate_priority_foreach(cur);
  thread_preemption(); 
  intr_set_level(old_level);
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  enum intr_level old_level = intr_disable();
  int nice_value = thread_current()->nice;
  intr_set_level(old_level);
  return nice_value;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  enum intr_level old_level = intr_disable ();
  int scaled_load_avg = convert_fixed_to_int_nearest(fixed_multiply_int(load_avg, 100));
  intr_set_level (old_level);
  return scaled_load_avg;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  enum intr_level old_level = intr_disable ();
  int scaled_recent_cpu = convert_fixed_to_int_nearest(fixed_multiply_int(thread_current()->recent_cpu, 100));
  intr_set_level (old_level);
  return scaled_recent_cpu;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  if (!thread_mlfqs)
    t->priority = priority;
  t->magic = THREAD_MAGIC;
  t->original_priority = priority;
  list_init(&t->donations_list);
  t->waiting_lock = NULL;

  list_init(&t->children);
  sema_init(&t->load_sema, 0);
  sema_init(&t->wait_sema, 0);
  sema_init(&t->synch_sema, 0);
  t->is_child_loaded = false;
  t->has_parent_waited = false;
  for (int i = 0; i < FD_TABLE_SIZE; i++)
    t->fd_table[i] = NULL;

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);

void
thread_sleep(int64_t wakeup_tick)
{
  struct thread *current_thread = thread_current();
  enum intr_level old_level = intr_disable();
  current_thread->wakeup_tick = wakeup_tick;
  list_insert_ordered(&sleep_list, &current_thread->elem, compare_wakeup_ticks, NULL);
  if (wakeup_tick < next_tick_to_awake) {
      next_tick_to_awake = wakeup_tick;
  }
  thread_block();
  intr_set_level(old_level);
}


void 
thread_awake(int64_t current_ticks) 
{
    struct list_elem *e = list_begin(&sleep_list);
    
    while (e != list_end(&sleep_list)) {
        struct thread *t = list_entry(e, struct thread, elem);
        if (t->wakeup_tick <= current_ticks) {
            e = list_remove(e); 
            thread_unblock(t);   
        } 
        else {
            next_tick_to_awake = t->wakeup_tick;
            break;
        }
    }
}

int
get_next_tick_to_awake (void) 
{
  return next_tick_to_awake;
}

bool 
compare_wakeup_ticks(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
  const struct thread *thread_a = list_entry(a, struct thread, elem);
  const struct thread *thread_b = list_entry(b, struct thread, elem);

  return thread_a->wakeup_tick < thread_b->wakeup_tick;
}

bool 
compare_thread_prority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
  const struct thread *thread_a = list_entry(a, struct thread, elem);
  const struct thread *thread_b = list_entry(b, struct thread, elem);
  
  return thread_a->priority > thread_b->priority;
}

void 
thread_preemption(void) {
  if (!list_empty(&ready_list)){
    struct thread *cur = thread_current ();
    struct thread *most_priority_thread = list_entry(list_front(&ready_list), struct thread, elem);
    if (cur->priority < most_priority_thread->priority){
      if (intr_context())
        intr_yield_on_return();
      else
        thread_yield();
    }
  }
}

void
update_priority(void) {
  struct thread *cur = thread_current();
  cur->priority = cur->original_priority;
  if (!list_empty(&cur->donations_list)) {
    list_sort(&cur->donations_list, compare_thread_donator_priority, NULL);
    struct thread *max_priority_donator = list_entry(list_front(&cur->donations_list), struct thread, donator);
    if (max_priority_donator->priority > cur->priority)
      cur->priority = max_priority_donator->priority;
  }
}

void 
nested_donation(struct lock *lock, struct thread* cur){
  int level = 0, max_priority = cur->priority;
  struct lock *waiting_lock = lock;
  struct thread *lock_holder;
  while (level < 8){
    if (!waiting_lock) break;
    lock_holder = waiting_lock->holder;
    if (lock_holder->priority >= max_priority)
      max_priority = lock_holder->priority;
    else{ 
      lock_holder->priority = max_priority;
    }
    waiting_lock = lock_holder->waiting_lock;
    level++;
  }
}

bool
compare_thread_donator_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
	return list_entry(a, struct thread, donator)->priority > list_entry(b, struct thread, donator)->priority;
}

void
recalculate_priority_foreach(struct thread *t){
  if (t != idle_thread)
    t->priority = convert_fixed_to_int_zero(fixed_add_int(fixed_divide_int(t->recent_cpu, -4), PRI_MAX - t->nice * 2));
}

void
recalculate_priority(void){
  struct list_elem *e;
  for (e = list_begin (&all_list); e != list_end (&all_list); e = list_next (e))
    recalculate_priority_foreach(list_entry(e, struct thread, allelem));
  if (!list_empty(&ready_list))
    list_sort(&ready_list, compare_thread_prority, NULL);
}

void
increment_recent_cpu(void){
  struct thread *cur = thread_current();
  if (cur != idle_thread)
    cur->recent_cpu = fixed_add_int(cur->recent_cpu, 1);  
}

void
recalculate_recent_cpu_foreach(struct thread *t){
  if (t != idle_thread)
    t->recent_cpu = fixed_add_int(fixed_multiply(fixed_divide(fixed_multiply_int(load_avg, 2), fixed_add_int(fixed_multiply_int(load_avg, 2),1)), t->recent_cpu), t->nice);
}

void 
recalculate_recent_cpu(void){
  struct list_elem *e;
  for (e = list_begin (&all_list); e != list_end (&all_list); e = list_next (e))
    recalculate_recent_cpu_foreach(list_entry(e, struct thread, allelem));
}

void
recalculate_load_avg(void) {
  int ready_threads = thread_current() != idle_thread ? list_size(&ready_list) + 1 : list_size(&ready_list);
  load_avg = fixed_add(fixed_multiply(fixed_divide_int(convert_int_to_fixed(59),60), load_avg), fixed_divide_int(convert_int_to_fixed(ready_threads), 60));
}

int 
thread_add_file_to_fd_table (struct file *file) {
  struct file **fd_table = thread_current()->fd_table;
  for (int i = 2; i < FD_TABLE_SIZE; i++) { 
    if (fd_table[i] == NULL) {
      fd_table[i] = file;
      return i;
    }
  }
  return -1;
}

struct 
file *thread_get_file (int fd) {
  struct file **fd_table = thread_current()->fd_table;
  if (fd < 0 || fd >= FD_TABLE_SIZE)
    return NULL;
  return fd_table[fd];
}

void 
thread_remove_file_from_fd_table (int fd) {
  struct file **fd_table = thread_current()->fd_table;
  if (fd < 2 || fd >= FD_TABLE_SIZE || fd_table[fd] == NULL)
    return;
  fd_table[fd] = NULL;
}