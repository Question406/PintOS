#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "lib/stdio.h"

#include "userprog/process.h"

#ifdef VM
#include "vm/page.h"
#endif

#ifdef FILESYS
bool sys_chdir(const char *name);
bool sys_mkdir(const char *name);
bool sys_readdir(int fd, char *name);
bool sys_isdir(int fd);
int sys_inumber(int fd);
#endif

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

#ifdef VM
bool sys_munmap(mmapid_t);
mmapid_t sys_mmap(int fd, void *);

void preload_pin_pages(const void *, size_t);
void preload_unpin_pages(const void *, size_t);

static struct mmap_desc* find_mmap_desc(struct thread*, mmapid_t fd);
#endif

static int mem_read(void *from, void* writeTo, int bytes);
static int32_t get_user (const uint8_t *uaddr);
static void check_valid_ptr(const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);

static struct file_descriptor* get_file_descriptor(struct thread* cur, int fd, int type);
//type = 0 for all  1 for file 2 for directory

struct lock fileSys_lock;

static struct file_descriptor* get_file_descriptor(struct thread* cur, int fd, int type){
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
        if (fileD->dir != NULL && type != 1) 
          return fileD;
        else if (fileD->dir == NULL && type != 2)
          return fileD;
        return NULL;
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
  thread_current()->cur_esp = f->esp;
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

#ifdef VM
    case SYS_MMAP:
    {
      int fd;
      void *addr;
      mem_read(f->esp + 4, &fd, sizeof(fd));
      mem_read(f->esp + 8, &addr, sizeof(addr));
      mmapid_t res = sys_mmap (fd, addr);
      f->eax = res;
      break;
    }

      case SYS_MUNMAP:
      {
        mmapid_t mid;
        mem_read(f->esp + 4, &mid, sizeof(mid));
        sys_munmap(mid);
        break;
      }
#endif
#ifdef FILESYS
    case SYS_CHDIR:
    {
      const char *name;
      mem_read(f->esp + 4, &name, sizeof(name));
      f->eax = sys_chdir(name);
      break;
    }
    case SYS_MKDIR:
    {
      const char *name;
      mem_read(f->esp + 4, &name, sizeof(name));
     // printf("---SYS_MKDIR---%s\n", name);
      f->eax = sys_mkdir(name);
      break;
    }
    case SYS_READDIR:
    {
      int fd;
      char *name;
      mem_read(f->esp + 4, &fd, sizeof(fd));
      mem_read(f->esp + 8, &name, sizeof(name));
      f->eax = sys_readdir(fd, name);
      break;
    }
    case SYS_ISDIR:
    {
      int fd;
      mem_read(f->esp + 4, &fd, sizeof(fd));
      f->eax = sys_isdir(fd);
      break;
    }
    case SYS_INUMBER:
    {
      int fd;
      mem_read(f->esp + 4, &fd, sizeof(fd));
      f->eax = sys_inumber(fd);
      break;
    }
#endif
    default:
      printf("[ERROR], forget add something!\n");
      sys_exit(-1);
      break;
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
  lock_acquire(&fileSys_lock);
  pid_t pid = process_execute(cmd_line);
  lock_release(&fileSys_lock);
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
    struct file_descriptor* file = get_file_descriptor(thread_current(), fd, 1);
    if (file && file->file) //file should be opened by cur thread
    {
#ifdef VM
        preload_pin_pages(buffer, size);
#endif
        res = file_write(file->file, buffer, size);
#ifdef VM
        preload_unpin_pages(buffer, size);
#endif
    }
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
  return process_wait(pid);
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

  //judge whether a directory
  struct inode *inode = file_get_inode(file_desc->file);
  if (inode != NULL && inode_dir(inode)) 
    file_desc -> dir = dir_open(inode_reopen(inode));
  else 
    file_desc -> dir = NULL;

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
  struct file_descriptor* file = get_file_descriptor(thread_current(), fd, 0);
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
    struct file_descriptor* fileD = get_file_descriptor(thread_current(), fd, 0); 
    if (fileD == NULL || fileD->file == NULL)
      res = -1;
    else {
#ifdef VM
        preload_pin_pages(buffer, size);
#endif
      res = file_read(fileD->file, buffer, size);
#ifdef VM
      preload_unpin_pages(buffer, size);
#endif
    }
  }
  lock_release(&fileSys_lock);
  return res;
}

static void 
sys_seek(int fd, unsigned position) {
  lock_acquire(&fileSys_lock);
  struct file_descriptor* fileD = get_file_descriptor(thread_current(), fd, 0); 
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
  struct file_descriptor* fileD = get_file_descriptor(thread_current(), fd, 0); 
  unsigned res = -1;
  if (fileD && fileD->file) 
    res = file_tell(fileD->file);
  lock_release(&fileSys_lock);
  return res;
}

static void 
sys_close(int fd) {
  struct file_descriptor* file = get_file_descriptor(thread_current(), fd, 0);
  lock_acquire (&fileSys_lock);
  if (file) {
    file_close(file->file);
    if (file->dir) dir_close(file->dir);
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

#ifdef VM
bool sys_munmap(mmapid_t mid){
    struct thread * cur_thread = thread_current ();
    struct mmap_desc *mmap_d = find_mmap_desc(cur_thread, mid);
    if(mmap_d == NULL)
        return false;
    lock_acquire(&fileSys_lock);
    {size_t file_size = mmap_d->size;
    for(size_t offset=0; offset<file_size; offset+=PGSIZE){
        void *addr = mmap_d->addr + offset;
        size_t bytes = (offset + PGSIZE < file_size ? PGSIZE : file_size-offset);
        vm_supt_mm_unmap(cur_thread->supt, cur_thread->pagedir, addr, mmap_d->file, offset, bytes);
    }
    list_remove(&mmap_d->elem);
    file_close(mmap_d->file);
    free(mmap_d);}
    lock_release(&fileSys_lock);
    return true;
}

mmapid_t sys_mmap(int fd, void *usr_page){
    if(usr_page == NULL || pg_ofs(usr_page) != 0)
        return -1;
    if(fd <= 1)
        return -1;
    struct thread * cur_thread = thread_current ();
    lock_acquire(&fileSys_lock);

    struct file* file = NULL;
    struct file_descriptor *fileD = get_file_descriptor(thread_current(), fd, 0);
    if(fileD && fileD->file)
        file = file_reopen(fileD->file);
    if(file == NULL){
        lock_release (&fileSys_lock);
        return -1;
    }
    size_t file_size = file_length (file);
    if(file_size == 0){
        lock_release (&fileSys_lock);
        return -1;
    }
    for(size_t offset=0; offset < file_size; offset+=PGSIZE){
        void *addr = usr_page + offset;
        if(vm_supt_has_entry(cur_thread->supt, addr)){
            lock_release (&fileSys_lock);
            return -1;
        }
    }
    for(size_t offset=0; offset < file_size; offset+=PGSIZE){
        void *addr = usr_page + offset;
        size_t read_bytes = (offset + PGSIZE < file_size ? PGSIZE : file_size - offset);
        size_t zero_bytes = PGSIZE - read_bytes;
        vm_supt_install_filesys(cur_thread->supt, addr, file, offset, read_bytes, zero_bytes, true);
    }

    mmapid_t mid;
    if(!list_empty(&cur_thread->mmap_list))
        mid = list_entry(list_back(&cur_thread->mmap_list), struct mmap_desc, elem)->id + 1;
    else mid = 1;
    struct mmap_desc *mmap_d = (struct mmap_desc*)malloc(sizeof(struct mmap_desc));
    mmap_d->id = mid;
    mmap_d->addr = usr_page;
    mmap_d->file = file;
    mmap_d->size = file_size;
    list_push_back(&cur_thread->mmap_list, &mmap_d->elem);
    lock_release (&fileSys_lock);
    return mid;
}

void preload_pin_pages(const void *buffer, size_t size){
    struct supplemental_page_table *supt = thread_current()->supt;
    uint32_t *pagedir = thread_current ()->pagedir;
    for(void *usr_page = pg_round_down(buffer); usr_page < buffer + size; usr_page+=PGSIZE){
        vm_load_page(supt, pagedir, usr_page);
        vm_pin_page(supt, usr_page);
    }
}

void preload_unpin_pages(const void *buffer, size_t size){
    struct supplemental_page_table *supt = thread_current()->supt;
    for(void *usr_page = pg_round_down(buffer); usr_page < buffer + size; usr_page+=PGSIZE){
        vm_unpin_page(supt, usr_page);
    }
}

static struct mmap_desc* find_mmap_desc(struct thread* t, mmapid_t fd){
    ASSERT (t!= NULL);
    if(! list_empty(&t->mmap_list)){
        for(struct list_elem *l_elem = list_begin(&t->mmap_list); l_elem != list_end(&t->mmap_list); l_elem = list_next(l_elem)){
            struct mmap_desc *mmap_d = list_entry(l_elem, struct mmap_desc, elem);
            if(mmap_d->id == fd)
                return mmap_d;
        }
    }
    return NULL;
}
#endif

#ifdef FILESYS
bool sys_chdir(const char *name) 
{
   // printf("---change directory to %s---\n",name);
    lock_acquire(&fileSys_lock);
    struct dir *dir = dir_open_path(name);
    if (dir == NULL) 
    {
      lock_release(&fileSys_lock);
      return false;
    }
    struct thread *t = thread_current();
    dir_close(t->cwd);
    t->cwd = dir;
    lock_release(&fileSys_lock);
    return true;
}

bool sys_mkdir(const char *name) 
{
  //printf("makedir!\n");
  lock_acquire(&fileSys_lock);
  int len = strlen(name);
  if (len == 0) 
  {
    lock_release(&fileSys_lock);
    return false;
  }
  char directory[len], filename[len];
  parse_path_name(name, directory, filename);
  struct dir *dir = dir_open_path(directory);
  block_sector_t inode_sector = 0;
  bool t2 = free_map_allocate (1, &inode_sector);
  bool t3 = inode_create (inode_sector, 0, 1);
  bool t4 = dir_add (dir, filename, inode_sector, 1);
 // printf("---name:   %s\n", name);
 // printf("---directory:   %s\n", directory);
 // printf("---filename:   %s\n", filename);
  //printf("-----%d %d %d-----\n", t2 ,t3, t4);
  bool success = (dir != NULL
                  && t2
                  && t3
                  && t4);
  if (!success && inode_sector != 0)
    free_map_release(inode_sector, 1);
  dir_close(dir);
  lock_release(&fileSys_lock);
  return success;
}

bool sys_readdir(int fd, char *name)
{
  lock_acquire(&fileSys_lock);
  struct file_descriptor* fdr = get_file_descriptor(thread_current(), fd, 2);
  if (fdr == NULL) 
  {
    lock_release(&fileSys_lock);
    return false;
  }
  struct inode *inode = file_get_inode(fdr->file);
  if (inode == NULL) 
  {
    lock_release(&fileSys_lock);
    return false;
  }
  if (!inode_dir(inode)) 
  {
    lock_release(&fileSys_lock);
    return false;
  }
  struct dir *dir = fdr -> dir;
  ASSERT(dir != NULL);
  bool tmp = dir_readdir(dir, name);
  lock_release(&fileSys_lock);
  return tmp;
}

bool sys_isdir(int fd) 
{
  lock_acquire(&fileSys_lock);
  struct file_descriptor* fdr = get_file_descriptor(thread_current(), fd , 0);
  if (fdr == NULL) 
  {
    lock_release(&fileSys_lock);
    return false;
  }
  struct inode *inode = file_get_inode(fdr->file);
  if (inode == NULL) 
  {
    lock_release(&fileSys_lock);
    return false;
  }
  lock_release(&fileSys_lock);
  return inode_dir(inode);
}

int sys_inumber(int fd) 
{
  lock_acquire(&fileSys_lock);
  struct file_descriptor* fdr = get_file_descriptor(thread_current(), fd, 0);
  struct inode * inode = file_get_inode(fdr->file);
  ASSERT(inode != NULL);
  int ret = inode_num(inode);
  lock_release(&fileSys_lock);
  return ret;
}

#endif
