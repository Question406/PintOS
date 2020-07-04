#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
static char zeros[BLOCK_SECTOR_SIZE];
/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t direct_block[123];
    block_sector_t indirect_block;
    block_sector_t double_indirect_block;    

    bool isdir;           
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
  };

 size_t min(size_t a, size_t b) 
 {
   return a < b ? a : b;
 }

 static bool inode_indirect_allocate(block_sector_t* p, size_t num_sectors, int deep)
 {
   if (deep == 0) {
     if (*p == 0) {
        if (!free_map_allocate(1, p))
          return false;
     }
     buffer_cache_write(*p, zeros);
     return true;
   }
   block_sector_t blocks[128];
   if (*p == 0) 
   {
      free_map_allocate(1, p);
      buffer_cache_write(*p, zeros);
   }
   buffer_cache_read(*p, blocks);

   size_t unit = deep == 1 ? 1 : 128;
   size_t i;
   size_t l = (num_sectors + unit - 1) / unit;

   for (i = 0; i < l; i++) 
   {
      size_t tmp = min(num_sectors, unit);
      if (!inode_indirect_allocate(&blocks[i], tmp, deep - 1))
        return false;
      num_sectors -= tmp;
   }

   ASSERT(num_sectors == 0);
   buffer_cache_write(*p, blocks);
   return true;   
 }
 
/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

 static bool inode_allocate (struct inode_disk *disk_inode, off_t length) 
 {
   if (length < 0) return false;

   size_t num_sectors = bytes_to_sectors(length);
   size_t i,l;
  // first allocate direct blocks
   l = min(num_sectors, 123);
   for (i = 0; i < l; i++) 
   {
     if (disk_inode->direct_block[i] == 0) 
     {
       if (!free_map_allocate(1, &disk_inode -> direct_block[i])) 
          return false;
       buffer_cache_write(disk_inode->direct_block[i], zeros);
     }
   }
   num_sectors -= l;
   if (num_sectors == 0) return true;

   //second allocate indirect blocks
   l = min(num_sectors, 128);
   if (!inode_indirect_allocate(&disk_inode->indirect_block, l, 1))
      return false;
   num_sectors -= l;
   if (num_sectors == 0) return true;

   //third allocate double indirect blocks
   l = min(num_sectors, 128 * 128);
   if (!inode_indirect_allocate(&disk_inode->double_indirect_block, l, 2))
      return false;
   num_sectors -= l;
   if (num_sectors == 0) return true;

   ASSERT(num_sectors == 0);
   return false;

 }


/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };


static block_sector_t 
index_to_sector(const struct inode_disk *ptr, off_t index) 
{
  off_t curpos = 0;

  //look up in the direct block
  if (index < 123)
    return ptr->direct_block[index];
  
  //look up in the indirect block
  curpos += 123;
  if (index < curpos + 128) 
  {
    block_sector_t blocks[128];
    buffer_cache_read(ptr->indirect_block, blocks);
    return blocks[index - curpos];
  }

  //look up in the double-indirect block
  curpos += 128;
  if (index < curpos + 128 * 128) 
  {
    off_t index1 = (index - curpos) / 128;
    off_t index2 = (index - curpos) % 128;

    block_sector_t blocks[128];
    buffer_cache_read(ptr->double_indirect_block, blocks);
    buffer_cache_read(blocks[index1], blocks);
    return blocks[index2];
  }

  return -1;

}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (0 <= pos && pos < inode->data.length) 
  {
    off_t index = pos / BLOCK_SECTOR_SIZE;
    return index_to_sector(&inode->data, index);
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool isdir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->isdir = isdir;
      if (inode_allocate(disk_inode, disk_inode->length))
        {
          buffer_cache_write (sector, disk_inode);
          success = true; 
        } 
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  buffer_cache_read (inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

void inode_delete_indirect(block_sector_t ptr, size_t num_sectors, int deep)
{
  if (deep == 0) 
  {
    free_map_release(ptr, 1);
    return;
  }

  block_sector_t blocks[128];
  buffer_cache_read(ptr, blocks);

  size_t unit = deep == 1 ? 1 : 128;
  size_t i, l = DIV_ROUND_UP(num_sectors, unit);

  for (i = 0; i < l; i++) 
  {
    size_t num = min(num_sectors, unit);
    inode_delete_indirect(blocks[i], num, deep - 1);
    num_sectors -= num;
  }

  ASSERT(num_sectors == 0);
  free_map_release(ptr, 1);
}

void inode_delete(struct inode *inode) 
{
  off_t len = inode -> data.length;
  if (len < 0) return false;

  size_t num_sectors = bytes_to_sectors(len);
  size_t i, l;

  //free direct block
  l = min(num_sectors, 123);
  for (i = 0; i < l; i++)
    free_map_release(inode->data.direct_block[i], 1);
  num_sectors -= l;
  if (num_sectors == 0) return;

  l = min(num_sectors, 128);
  inode_delete_indirect(inode->data.indirect_block, l, 1);
  num_sectors -= l;
  if (num_sectors == 0) return;

  l = min(num_sectors, 128 * 128);
  inode_delete_indirect(inode->data.double_indirect_block, l, 2);
  if (num_sectors == 0) return;

  ASSERT("GG");
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          inode_delete(inode);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          buffer_cache_read (sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          buffer_cache_read (sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  if (byte_to_sector(inode, offset + size - 1) == -1u) 
  {
    if (!inode_allocate(&inode -> data, offset + size)) 
      return 0; //fail to extend
    inode->data.length = offset + size;
    buffer_cache_write(inode->sector, &inode->data);

  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          buffer_cache_write (sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            buffer_cache_read (sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          buffer_cache_write (sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

bool
inode_dir(const struct inode *inode) 
{
  return inode->data.isdir;
}

int 
inode_num(const struct inode *inode)
{
  return (int)inode->sector;
}

bool 
inode_is_removed(const struct inode *inode) 
{
  return inode->removed;
}
