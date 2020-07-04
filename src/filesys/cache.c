#include <debug.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/synch.h"

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

/* For synchronizing. Only one operation with buffer at the same time*/
static struct lock buffer_cache_lock;

/* Init the buffer cache when the file sysytem init*/
void
buffer_cache_init(void)
{
    lock_init(&buffer_cache_lock);
    for(size_t i = 0; i < BUFFER_CACHE_SIZE; i++){
        cache[i].valid = false;
        cache[i].dirty = false;
        cache[i].second_time = false;
    }
}


/* Close the buffer cache when the file system close*/
void
buffer_cache_close(void)
{
    lock_acquire(&buffer_cache_lock);
    for(size_t i = 0; i < BUFFER_CACHE_SIZE; i++){
        if(cache[i].valid == true && cache[i].dirty == true){
            block_write(fs_device, cache[i].sector, cache[i].data);
        }
    }
    lock_release(&buffer_cache_lock);
}

/* Look up buffer cache by sector id*/
static struct buffer_cache_entry* buffer_cache_lookup(block_sector_t sector){
    for(size_t i = 0; i < BUFFER_CACHE_SIZE; i++){
        if(cache[i].valid == true && cache[i].sector == sector)
            return &cache[i];
    }
    return NULL;
}

/* Clock(Second Chance) algorithm*/
static struct buffer_cache_entry* buffer_cache_evict(void){
    static size_t pointer = 0;
    ASSERT(lock_held_by_current_thread(&buffer_cache_lock));
    while(true){
        if(cache[pointer].valid == false)
            return &(cache[pointer]);

        if(cache[pointer].second_time == true)
            cache[pointer].second_time = false;
        else
            break;

        pointer = (pointer+1)%BUFFER_CACHE_SIZE;
    }

    ASSERT(cache[pointer].valid);
    if(cache[pointer].dirty == true){
        block_write(fs_device, cache[pointer].sector, cache[pointer].data);
    }
    cache[pointer].valid = false;
    return &(cache[pointer]);
}

/* Try to read data from buffer to target*/
void
buffer_cache_read(block_sector_t sector, void *target)
{
    lock_acquire(&buffer_cache_lock);
    struct buffer_cache_entry* target_entry = buffer_cache_lookup(sector);
    if(target_entry == NULL){
        target_entry = buffer_cache_evict();
        target_entry->valid = true;
        target_entry->dirty = false;
//        target_entry->second_time = true;
        target_entry->sector = sector;
        block_read(fs_device, sector, target_entry->data);
    }
    target_entry->second_time = true;
    memcpy(target, target_entry->data, BLOCK_SECTOR_SIZE);


    lock_release(&buffer_cache_lock);
}


/* Try to write data from source to buffer*/
void
buffer_cache_write(block_sector_t sector, const void *source)
{
    lock_acquire(&buffer_cache_lock);
    struct buffer_cache_entry* source_entry = buffer_cache_lookup(sector);
    if(source_entry == NULL){
        source_entry = buffer_cache_evict();
        source_entry->valid = true;
//        source_entry->dirty = false;
//        source_entry->second_time = true;
        source_entry->sector = sector;
        block_read(fs_device, sector, source_entry->data);
    }
    source_entry->second_time = true;
    source_entry->dirty = true;
    memcpy(source_entry->data, source, BLOCK_SECTOR_SIZE);

    lock_release(&buffer_cache_lock);
}
