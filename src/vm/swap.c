//
// Created by dell on 2020/6/29.
//
#include<bitmap.h>
#include"devices/block.h"
#include"threads/vaddr.h"
#include"vm/swap.h"

static size_t swap_size;
static const size_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;

static struct bitmap *swap_avail;
static struct block *swap_block;

void vm_swap_init(){
    ASSERT (SECTORS_PER_PAGE > 0);
    swap_block = block_get_role(BLOCK_SWAP);
    if(swap_block== NULL){
        PANIC ("Error: Can't initialize swap block");
    }
    swap_size = block_size(swap_block)/SECTORS_PER_PAGE;
    swap_avail = bitmap_create(swap_size);
    bitmap_set_all(swap_avail, true);
}

swap_idx_t vm_swap_out(void *page){
    ASSERT(page >= PHYS_BASE);
    size_t swap_idx = bitmap_scan(swap_avail, 0, 1, true);
    for(size_t i=0; i<SECTORS_PER_PAGE; ++i){
        block_write(swap_block, swap_idx*SECTORS_PER_PAGE+i, page+(BLOCK_SECTOR_SIZE*i));
    }
    bitmap_set(swap_avail, swap_idx, false);
    return swap_idx;
}

void vm_swap_in(swap_idx_t swap_idx, void *page){
    ASSERT(page >= PHYS_BASE);
    ASSERT(swap_idx < swap_size);
    if(bitmap_test(swap_avail, swap_idx) == true){
        PANIC ("Error, invalid read access to unassigned swap block");
    }
    for(size_t i=0; i<SECTORS_PER_PAGE; ++i){
        block_read(swap_block, swap_idx*SECTORS_PER_PAGE+i, page+(BLOCK_SECTOR_SIZE*i));
    }
    bitmap_set(swap_avail, swap_idx, true);
}

void vm_swap_free(swap_idx_t swap_idx){
    ASSERT(swap_idx < swap_size);
    if(bitmap_test(swap_avail, swap_idx) == true){
        PANIC ("Error, invalid free request to unassigned swap block");
    }
    bitmap_set(swap_avail, swap_idx, true);
}
