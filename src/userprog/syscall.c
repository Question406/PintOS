#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/stdio.h"

#include "userprog/process.h"


static void syscall_handler (struct intr_frame *);

typedef int pid_t;
// -------
static void sys_halt();
// static int sys_exit(int retVal);
static int sys_write(int fd, const void *buffer, unsigned size);
static pid_t sys_exec(const char *cmd_line);
static int sys_wait (pid_t pid);
static bool sys_create (const char *file, unsigned initial_size);
static bool sys_remove(const char* file);
static int sys_open(const char* file);
static int sys_filesize(int fd);
static int sys_read(int fd, void *buffer, unsigned size);
static void sys_seek(int fd, unsigned position);
static unsigned sys_tell(int fd);
static void sys_close(int fd);

static int mem_read(void *from, void* writeTo, int bytes);
static int32_t get_user (const uint8_t *uaddr);
static void check_valid_ptr(const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);

static struct file_descriptor* get_file_descriptor(struct thread* cur, int fd);

struct lock fileSys_lock;

static struct file_descriptor* get_file_descriptor(struct thread* cur, int fd){
  if (fd < 3) 
    return NULL; // should not happen
  struct list_elem *elem = NULL;
  if (! list_empty(&cur->opened_files)) {
    for ( elem = list_begin(&cur->opened_files); 
          elem != list_end(&cur->opened_files); 
          elem = list_next(elem)) 
    {
      struct file_descriptor *fileD = 
      list_entry(elem, struct file_descriptor, elem);
      if(fileD->fdID == fd) {
        return fileD;
      }
    } 
  }
  return NULL;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&fileSys_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int syscall_num;
  mem_read(f->esp, &syscall_num, sizeof(int));
  switch (syscall_num) {
    case SYS_HALT:
    {
      sys_halt();
      break;
    }

    case SYS_EXIT:
    {
      int retVal;
      mem_read(f->esp + 4, &retVal, sizeof(int)); // 1 arg
      sys_exit(retVal);
      break;
    }

    case SYS_EXEC:
    {
      char* cmd_line;
      mem_read(f->esp + 4, &cmd_line, sizeof(cmd_line)); // 1 arg

      int res = sys_exec((const char*) cmd_line);
      f->eax = res; // set ret value
      break;
    }
    
    case SYS_WAIT:
    {
      // pid_t pid;
      int pid;
      mem_read(f->esp + 4, &pid, sizeof(pid));

      int res = sys_wait(pid);
      f->eax = res;
      break;
    }
    case SYS_CREATE:
    {
      char* file;
      unsigned initial_size;
      bool res;

      mem_read(f->esp + 4, &file, sizeof(file));
      mem_read(f->esp + 8, &initial_size, sizeof(initial_size));

      res = sys_create(file, initial_size);
      f->eax = res;
      break;
    }

    case SYS_REMOVE:
    {
      char* file;
      bool res;
      mem_read(f->esp+4, &file, sizeof(file));
      res = sys_remove(file);
      f->eax = res;
      break;
    }

    case SYS_OPEN:
    {
      char* file;
      int res;
      mem_read(f->esp+4, &file, sizeof(file));
      res = sys_open(file);
      f->eax = res;
      break;
    }

    case SYS_FILESIZE:
    {
      int fd;
      int res;
      mem_read(f->esp + 4, &fd, sizeof(fd));
      res = sys_filesize(fd);
      f->eax = res;
      break;
    }

    case SYS_READ:
    {
      int fd;
      void *buffer;
      unsigned size;
      int res;
      mem_read(f->esp + 4, &fd, sizeof(fd));
      mem_read(f->esp + 8, &buffer, sizeof(buffer));
      mem_read(f->esp + 12, &size, sizeof(size));
      res = sys_read(fd, buffer, size);
      f->eax = res;
      break;
    }

    case SYS_WRITE:
    {
      int fd;
      void *buffer;
      unsigned size;
      int res;
      mem_read(f->esp + 4, &fd, sizeof(fd));
      mem_read(f->esp + 8, &buffer, sizeof(buffer));
      mem_read(f->esp + 12, &size, sizeof(size));
      res = sys_write(fd, buffer, size);
      f->eax = res;
      break;
    }

    case SYS_SEEK:
    {
      int fd;
      unsigned position;
      mem_read(f->esp + 4, &fd, sizeof(fd));
      mem_read(f->esp + 8, &position, sizeof(position));
      sys_seek(fd, position);
      break;
    }

    case SYS_TELL:
    {
      int fd;
      int res;
      mem_read(f->esp + 4, &fd, sizeof(fd));
      res = sys_tell(fd);
      f->eax = res;
      break;
    }

    case SYS_CLOSE:
    {
      int fd;
      mem_read(f->esp + 4, &fd, sizeof(fd));
      sys_close(fd);
      break;
    }
  }
}

static void
sys_halt() {
  shutdown_power_off();
}

void
sys_exit(int retVal) {
  struct thread * cur_thread = thread_current();
  printf("%s: exit(%d)\n", cur_thread->name, retVal);
  cur_thread->pcb->retVal = retVal;
  thread_exit ();
  return -1;
}

static pid_t
sys_exec(const char *cmd_line){
  // tid_t tid;
  check_valid_ptr((const uint8_t*) cmd_line);
  char* ptr = cmd_line;
  while (*ptr != '\0') { 
    ptr++;
    check_valid_ptr(ptr);
  }
  check_valid_ptr(ptr);
  // if (!is_user_vaddr(cmd_line))
  //   sys_exit(-1);
  pid_t pid = process_execute(cmd_line);
  return pid;
}

static int 
sys_write(int fd, const void *buffer, unsigned size) {
  check_valid_ptr((const uint8_t*) buffer);
  check_valid_ptr((const uint8_t*) buffer + size - 1);

  lock_acquire (&fileSys_lock);
  int res = -1;
  if(fd == 1) { // stdout
    putbuf(buffer, size);
    res = size;
  }
  else { // file
    struct file_descriptor* file = get_file_descriptor(thread_current(), fd);
    if (file && file->file) //file should be opened by cur thread
      res = file_write(file->file, buffer, size);
  }
  lock_release (&fileSys_lock);
  return res;
}

static bool
sys_create (const char *file, unsigned initial_size)
{
  check_valid_ptr(file);
  lock_acquire(&fileSys_lock);
  bool res = filesys_create(file, initial_size);
  lock_release(&fileSys_lock);
  return res;
}


static int 
sys_wait (pid_t pid) {
  process_wait(pid);
}

static bool 
sys_remove(const char* file) {
  check_valid_ptr(file);
  lock_acquire(&fileSys_lock);
  bool res = filesys_remove(file);
  lock_release(&fileSys_lock);
  return res;
}

static int 
sys_open(const char* file) {
  check_valid_ptr(file);
  int res = -1;
  struct file_descriptor* file_desc = palloc_get_page(0);
  
  if (file_desc == NULL) // not enough space
    return -1;

  lock_acquire (&fileSys_lock);
  struct file* File;
  File = filesys_open(file);
  if (!File) { // open failure
    palloc_free_page (file_desc);
    res = -1;
    goto done;
  }
  // set file_descriptor
  file_desc->file = File;
  struct list* opened_files = &thread_current()->opened_files;
  if (list_empty(opened_files)) // first opened file
    file_desc->fdID = 3; // 0, 1, 2 reserved
  else {
    struct list_elem* last_opened_elem = list_back(opened_files);
    struct file_descriptor* last_opend = list_entry(last_opened_elem, struct file_descriptor, elem);
    file_desc->fdID = last_opend->fdID + 1;
  }    
  res = file_desc->fdID;
  // push to opend files
  list_push_back(opened_files, &(file_desc->elem));
done:
  lock_release (&fileSys_lock);
  return res;
}

static int 
sys_filesize(int fd) { // fd should be opend by cur thread
  lock_acquire (&fileSys_lock);
  struct file_descriptor* file = get_file_descriptor(thread_current(), fd);
  int res = -1;
  if (file != NULL) 
    res = file_length(file->file);
  lock_release (&fileSys_lock);
  return res;
}

static int 
sys_read(int fd, void *buffer, unsigned size) {
  // check valid
  check_valid_ptr(buffer);
  check_valid_ptr(buffer + size - 1);
  lock_acquire(&fileSys_lock);
  
  int res = -1;
  if (fd == 0) { // stdin
    for (int i = 0; i < size; ++i) {
      if(! put_user(buffer + i, input_getc())){
        lock_release (&fileSys_lock);
        sys_exit(-1); // segfault
      }
    }
    res = size;
  } else {
    // fd should be opened
    struct file_descriptor* fileD = get_file_descriptor(thread_current(), fd); 
    if (fileD == NULL || fileD->file == NULL)
      res = -1;
    else {
      res = file_read(fileD->file, buffer, size);
    }
  }
  lock_release(&fileSys_lock);
  return res;
}

static void 
sys_seek(int fd, unsigned position) {
  lock_acquire(&fileSys_lock);
  struct file_descriptor* fileD = get_file_descriptor(thread_current(), fd); 
  if (fileD && fileD->file) {
    file_seek(fileD->file, position);
  } 
  else  {
    lock_release(&fileSys_lock);
    sys_exit(-1);
  }
  lock_release(&fileSys_lock);
}

static unsigned 
sys_tell(int fd) {
  lock_acquire(&fileSys_lock);
  struct file_descriptor* fileD = get_file_descriptor(thread_current(), fd); 
  unsigned res = -1;
  if (fileD && fileD->file) 
    res = file_tell(fileD->file);
  lock_release(&fileSys_lock);
  return res;
}

static void 
sys_close(int fd) {
  struct file_descriptor* file = get_file_descriptor(thread_current(), fd);
  lock_acquire (&fileSys_lock);
  if (file) {
    file_close(file->file);
    list_remove(&(file->elem));
    palloc_free_page(file);
  }
  lock_release (&fileSys_lock);
}

/*****************************************************************/
static void
check_valid_ptr(const uint8_t *uaddr) {
  if (uaddr == NULL)
    sys_exit(-1);
  int res = get_user(uaddr);
  if (res == -1) // page fault || seg fault
    sys_exit (-1);
}

// read "bytes" data from *from* and put it to writeTo
static int
mem_read(void *from, void* writeTo, int bytes) {
  int32_t res;
  int i;
  for(i = 0; i < bytes; i++) {
    res = get_user(from + i);
    if (res == -1) // segfault or invalid memory access
      sys_exit(-1);
    *(char*)(writeTo + i) = res & 0xff;
  }
  return (int) bytes;
}

// Highlight: mem read && write in pintos book
/**
 * Reads a single 'byte' at user memory admemory at 'uaddr'.
 * 'uaddr' must be below PHYS_BASE.
 * Returns the byte value if successful (extract the least significant byte),
 * or -1 in case of error (a segfault occurred or invalid uaddr)
 */
static int32_t
get_user (const uint8_t *uaddr) {
  // check valid 
  if (!is_user_vaddr(uaddr)) {
    return -1;
  }

  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Writes a single byte (content is 'byte') to user address 'udst'.
 * 'udst' must be below PHYS_BASE.
 * Returns true if successful, false if a segfault occurred.
 */
static bool
put_user (uint8_t *udst, uint8_t byte) {
  // check valid
  if (!is_user_vaddr(udst)) {
    return -1;
  }
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
      : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}