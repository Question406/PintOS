#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

#ifdef USERPROG
#include "userprog/syscall.h"
#include "threads/thread.h"
#endif

// #define DEBUG
#ifdef DEBUG
#define _DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#define _DEBUG_PRINTF(...)
#endif

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
static void push_args(void** esp, const int argc, const char *argv[]);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
// int
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;

  struct process_control_block *pcb = NULL;
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);
  pcb = palloc_get_page(0);
  if (pcb == NULL) {
    if (fn_copy)
      palloc_free_page(fn_copy);
    return TID_ERROR;
  }

  pcb->tid = TID_ERROR;
  pcb->child_fail_load = false;
  pcb->waitingBy = false;
  pcb->exited = false;
  pcb->orphan = false;
  sema_init(&pcb->sema_waiting, 0);
  sema_init(&pcb->sema_syncPaSon, 0);
  // highlight: advised by pintos manual to call strtok_r,
  //            split 'echo x' into 'echo' ' x'
  //                  'echo x y' into 'echo' ' x y'
  char *executing_name, *args;
  executing_name = strtok_r(fn_copy, " ", &args);
  pcb->args = args;
  /* Create a new thread to execute FILE_NAME. */
  // tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
  tid = thread_create (executing_name, PRI_DEFAULT, start_process, pcb);
  // printf("[DEBUG] %s hi\n", thread_current()->name);
  // traverseChild(thread_current());
  // if (tid == TID_ERROR || pcb->child_fail_load) {          // if failed, free the page
  if (tid == TID_ERROR){
    if (fn_copy)
      palloc_free_page(fn_copy);
    if (pcb)
      palloc_free_page(pcb);
    return TID_ERROR;
  }

  sema_down(&pcb->sema_syncPaSon);
  if (pcb->tid != TID_ERROR)
    list_push_back(&thread_current()->child_threads, &pcb->child_elem);
  palloc_free_page(fn_copy);
  _DEBUG_PRINTF("want to execute: %s at %d\n", file_name, tid);
  return pcb->tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *_pcb)
{
  struct process_control_block* pcb = _pcb;
  char *file_name = pcb->args;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);
  /* If load failed, quit. */
  // split args, still don't know why it fails to assign tokens in setup_stac,k
  char *args = file_name;
  char *tokens[32];
  int argc = 0;
  tokens[argc++] = thread_current()->name;
  char *token, *save_ptr;
  // arg tokens
  for ( token = strtok_r(args, " ", &save_ptr); 
        token != NULL;
        token = strtok_r(NULL, " ", &save_ptr))
  {
    tokens[argc++] = token;
  }

  if (argc > 32)
    success = false;
  if (success) 
    push_args(&if_.esp, argc, tokens);
  thread_current()->pcb = pcb;
  pcb->tid = (success) ? thread_current()->tid : TID_ERROR;
  pcb->related_thread = thread_current();

  sema_up(&(pcb->sema_syncPaSon));
  if (!success)  {
    _DEBUG_PRINTF("%d call exit\n", pcb->tid);
    sys_exit(-1);
  }

  _DEBUG_PRINTF("%s %d user process start! cmd: why?\n", thread_current()->name, pcb->tid);
  #ifdef DEBUG
  // for (int i = 0; i < argc; i++)
  //   _DEBUG_PRINTF("%s ", tokens[i]);
  // _DEBUG_PRINTF("\n");
  #endif

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid /*UNUSED*/) 
{
  struct thread *cur_thread = thread_current ();
  struct list *child_threads = &(cur_thread->child_threads);
  struct process_control_block *child_thread = get_child_thread(cur_thread, child_tid);
  if (child_thread == NULL || child_thread->waitingBy) {  // already waiting, wait twice
    return -1;
  }
  else
    child_thread->waitingBy = true;

  // wait(block) until child terminates
  if (! child_thread->exited){
    _DEBUG_PRINTF("[DEBUG] %s %d waiting, %d\n", cur_thread->name, cur_thread->tid, child_thread->tid);
    sema_down(& (child_thread->sema_waiting));
  }
  ASSERT (child_thread->exited);
  // return the exit code of the child process
  int retcode = child_thread->retVal;

  list_remove(&child_thread->child_elem); // delte child
  palloc_free_page(child_thread);
  // printf("[DEBUG] %s wait end\n", cur_thread->name);
  return retcode;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  // free resources, file structrue
  struct list *opend_files = &cur->opened_files;
  while (!list_empty(opend_files)) {
    struct list_elem *elem = list_pop_front(opend_files);
    struct file_descriptor *fileD = list_entry(elem, struct file_descriptor, elem);
    file_close(fileD->file);
    palloc_free_page(fileD);
  }

  // free child_thread list
  struct list *child_threads = &cur->child_threads;
  struct process_control_block *child_thread = NULL;
  while (!list_empty(child_threads)) {
    struct list_elem *elem = list_pop_front (child_threads);
    child_thread = list_entry(elem, struct process_control_block, child_elem);
    if (child_thread->exited)
      palloc_free_page(child_thread);
    else {
      child_thread->related_thread->parentThread = NULL;
      child_thread->orphan = true;
    }
  }

  // free executing_file
  if (cur->executing_file) {
    file_allow_write(cur->executing_file);
    file_close(cur->executing_file);
  }

  cur->pcb->exited = true;
  _DEBUG_PRINTF("process %d exiting\n", cur->tid);
  _DEBUG_PRINTF("[DEBUG] %s, %d sema_waiting up\n", cur->name, cur->tid);
  bool temp_orphan = cur->pcb->orphan; // father may 
  sema_up (&cur->pcb->sema_waiting);

  if (temp_orphan)
    palloc_free_page(&cur->pcb);

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) // Highlight: means user processe
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      // printf("%s: exit(%d)\n", cur_thread->name, retVal);
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
  // printf("[DEBUG] %s exit end\n", cur->name);
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
// load (const char *file_name, void (**eip) (void), void **esp) 
load (const char *args, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  // file = filesys_open (file_name); 
  _DEBUG_PRINTF("open %s:\n", t->name);
  file = filesys_open (t->name);   // we put the executable name at thread->name
  if (file == NULL) 
    {
      // printf("at %s\n",t->name);
      printf ("load: %s: open failed\n", t->name);
      success = false;
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", t->name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  // executing file
  file_deny_write(file);
  thread_current()->executing_file = file;
  success = true;

done:
  /* We arrive here whether the load is successful or not. */
  // if (!success)
  //   file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

static 
void push_args(void** esp, const int argc, const char *argv[]) {
  // highlight: Do the argument setup described in 4.5.1, 
  //            push arg string, split arg , push arg pointers, fake return address pointer
  //      note: the stack should be pushed from top to bottom because of the layout
  // push tokens from back and get pointers
  uint32_t *argv_ptrs[64];
  int i = 0, strlength = 0;
  for (i = argc - 1; i >= 0; i--) {
    strlength = strlen(argv[i]) + 1;
    *esp -= strlength;
    memcpy(*esp, argv[i], strlength);
    argv_ptrs[i] = *esp;
  }
  // align
  *esp -= 4 - (strlength % 4);
  // push pointers from back
  // push NULL pointer for argv[argc]
  *esp -= 4;
  *(uint32_t *) *esp = (uint32_t) NULL;
  // push argvs
  for (i = argc - 1; i >= 0; i--) {
    *esp -= 4;
    *(uint32_t *) *esp = (uint32_t *) argv_ptrs[i];
  }
  // push argv
  // * (uint32_t *) (*esp - 4) = *(uint32_t *) esp;
  // *esp -= 4;
  *esp -= 4;
  *((void**) *esp) = (*esp + 4);
  
  // push argc
  *esp -= 4;
  * (int *) *esp = argc;
  // push return addr
  *esp -= 4;
  *((uint32_t*) *esp) = (uint32_t) NULL;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);  // get the page for stack
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true); // map the page
      if (success) {
        *esp = PHYS_BASE;     // stack top
      }
      else
        palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
