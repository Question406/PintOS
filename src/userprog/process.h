#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H
#include "threads/thread.h"

tid_t process_execute (const char *file_name);

int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct process_control_block {
    tid_t tid;
    char* args;
    int retVal;
    bool child_fail_load;
    bool waitingBy;
    bool exited;
    bool orphan;
    struct thread* related_thread;
    struct semaphore sema_waiting;
    struct semaphore sema_syncPaSon;
    struct list_elem child_elem;
};

#endif /* userprog/process.h */
