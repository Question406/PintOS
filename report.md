## 分工：

Project1 & 2： 计家宝
Project3:      贾雨祺
Project4:      吕优 张扬天

## Project1 Threads：

​	对于sleep，给thread结构体添加了wake_tick域，表示thread从sleep中应该被唤醒的时间，sleepelem域用于sleep队列的元素维护。对于priority的维护，在thread中维护当前thread拥有的锁，和现在正在等待的锁，在timer等函数里按优先级维护ready队列。对于mlfqs算法，按pintosbook方案添加nice_val,recent_cpu域，fixed_point.c用作假实数库函数。

## Project2 Userprog：

​	为了保证父线程会为子线程收尸，包括异常退出，提前退出，先于父进程结束的子进程等种种情况，新建了process_control_block的结构体。在process_execute时为每一个进程从内核申请一页用于pcb，用sema_syncPaSon这个信号量保证父线程等待子线程start_process运行完成后继续运行，sema_waiting维护process_wait关系。对于非orphan的线程，父线程退出时为其回收pcb的空间，对于orphan进程，自己退出时回收自己的pcb空间。child_fail_load表示子进程load失败无法成功运行。retVal用于父进程回收子进程返回值。

## Project3 Virtual Memory：

使用supplemental page table当page fault发生时来补充页表，每一个supplemental_page_table_entry包括一个dirty位表示是否修改。使用frame table来实现回收页， 每一个frame_table_entry包括一个bool值来表示frame是否该被evicted，在frame中实现时钟算法。使用swap table（利用内部的bitmap实现）来选不用的swap slot.

## Project 4 File System



### Assignment

This project is completed by Lv You and Zhang Yangtian., which takes about a week to finish.(still have bugs)

### A. Indexed and Extensible Files

#### Data structure

```C
struct inode_disk
  {
    block_sector_t direct_block[123];
    block_sector_t indirect_block;
    block_sector_t double_indirect_block;    

    bool isdir;           
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
  };

```

direct_block saves the direct block in Pintos. indirect_block  and double_indirect_block are like multi-level-page. An inode_disk  entry is 512 bytes long.

#### storage strategy

It will first attempt to store file in direct block. If the space is not enough, it will store in the indirect block and double indirect block.

### B. Subdirectories 

#### Data structure

```C
/* A directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry 
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };
```

The directory structures are quite normal and don't need to explain.

And if a directory is not empty, we choose not to remove it.


### C. Buffer Cache

#### Data structure

```C
#define BUFFER_CACHE_SIZE 64

struct buffer_cache_entry{
    bool valid;
    bool dirty;
    bool second_time;   //for clock algorithm
    block_sector_t sector;
    uint8_t data[BLOCK_SECTOR_SIZE];
};

/* All the entries*/
static struct buffer_cache_entry cache[BUFFER_CACHE_SIZE];
```
A buffer cache entry has 512 bytes data. Apart from the buffer itself, it has several auxiliary bits. The valid bit is used for recording whether this entry in buffer entry table is active (all the entry is not active in the beginning). The dirty bit is used for write-behind, recording whether this block has been modified.

The eviction policy we applied is clock algorithm. And the second-time bit is used for recoding the status of the entry.





## Grade：

​	threads: [link](src/threads/build/grade)

![avatar](img/grade_thread.png)

​	userprog: [link](src/userprog/build/grade)
​    
![avatar](img/grade_userprog.png)

​	virtualmemory: [link](src/vm/build/grade)
​    
![avatar](img/grade_virtualmemory.png)

​	filesystem: [link](src/filesys/build/grade)
​    
![avatar](img/grade_filesys.png)
