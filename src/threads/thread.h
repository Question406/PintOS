#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#define USERPROG

#include <debug.h>
#include <list.h>
#include <stdint.h>

#include "threads/synch.h"

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

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

    struct list_elem child_elem;

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
    int retVal;
    struct file *executing_file;
    struct lock child_thread_lock;
    struct list child_threads;
    struct list opened_files;
    bool child_fail_load;
    bool waitingBy;
    bool exited;
    struct semaphore sema_waiting;
    struct semaphore sema_syncPaSon;
    struct thread* parentThread;
#endif

   /*Owned by timer.c */
    int64_t wake_tick;                  /* tick to wake this thread */
    struct list_elem sleepelem;         /* used to keep the sleep queue */

   struct list holding_locks;       /* locks this thread holding */
   int old_priority;
   struct lock* lock_waiting;       /* lock this thread wants to acquire, but have to wait */
   
   /* Owend by mlfqs algorithm */
   int nice_val;                       /* nice value for mlfqs */
   int recent_cpu;                     /* recent_cpu for mlfqs */

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

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);


bool sleep_thread_cmp (const struct list_elem* t1, const struct list_elem* t2, void* aux);
bool thread_priority_cmp (const struct list_elem* t1, const struct list_elem* t2, void* aux);
void thread_donate_priority(const struct thread* donate_to);
void thread_hold_lock (struct lock* lock);
void thread_update_priority(struct thread* to_update);
void thread_remove_lock(struct lock* to_rm);
void thread_update_recent_cpu_and_load_avg();
void thread_update_recent_cpu(struct thread *t, void *aux);
void thread_update_recent_cpu_one();
void thread_update_priority_mlfqs(struct thread* to_update);

struct thread* get_child_thread(struct thread* cur, tid_t child_id);

void traverseChild(struct thread* cur);

/*********************************************************************/
struct file_descriptor {
   int fdID;
   struct file *file;
   struct list_elem elem;
};

#endif /* threads/thread.h */
