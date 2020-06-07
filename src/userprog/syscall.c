#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/stdio.h"

static void syscall_handler (struct intr_frame *);

typedef int pid_t;
// -------
static void sys_halt();
static int sys_exit(int retVal);
static int sys_write(int fd, const void *buffer, unsigned size);
static pid_t sys_exec(const char *cmd_line);
static int sys_wait (pid_t pid);
static bool sys_create (const char *file, unsigned initial_size);
static bool sys_remove(const char* file);
static int sys_open(const char* file);
static int sys_filesize(int fd);
static int sys_read(int fd, void *buffer, unsigned size);
static int sys_write(int fd, const void *buffer, unsigned size);
static void sys_seek(int fd, unsigned position);
static unsigned sys_tell(int fd);
static void sys_close(int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

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
      res = sys_close(fd);
      break;
    }
  }
}

static void
sys_halt() {
  shutdown_power_off();
}

static int 
sys_exit(int retVal) {
  struct thread * cur_thread = thread_current();
  cur_thread->retVal = retVal;
  thread_exit ();
  return -1;
}

static pid_t
sys_exec(const char *cmd_line){
  tid_t tid;
  if (!is_user_vaddr(cmd_line))
    sys_exit(-1);
  struct thread * cur_thread = thread_current(); 
}

static int 
sys_write(int fd, const void *buffer, unsigned size) {
  int res = -1;
  if (fd == STDOUT_FILENO) {
    putbuf(buffer, size);
  }
  return res;
}

static bool
sys_create (const char *file, unsigned initial_size)
{
  if (file != NULL)
    return sys_exit (-1);
  return filesys_create (file, initial_size);
}


static int 
sys_wait (pid_t pid) {

}

static bool 
sys_create (const char *file, unsigned initial_size) {

}

static bool 
sys_remove(const char* file) {

}

static int 
sys_open(const char* file) {

}

static int 
sys_filesize(int fd) {

}

static int 
sys_read(int fd, void *buffer, unsigned size) {

}

static int 
sys_write(int fd, const void *buffer, unsigned size) {

}

static void 
sys_seek(int fd, unsigned position) {

}

static unsigned 
sys_tell(int fd) {

}

static void 
sys_close(int fd) {

}

/*****************************************************************/
static void
check_valid_user(const uint8_t *uaddr) {
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