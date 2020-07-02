#include <bitmap.h>
#include "threads/vaddr.h"
#include "devices/block.h"
#include "vm/swap.h"

static struct block *swap_block;
static struct bitmap *swap_avail;
static const size_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;
static size_t swap_size;

void vm_swap_init () {
    ASSERT (SECTORS_PER_PAGE > 0);
    swap_block = block_get_role(BLOCK_SWAP);
    swap_size = block_size(swap_block) / SECTORS_PER_PAGE;
    swap_avail = bitmap_create(swap_size);
    bitmap_set_all(swap_avail, true);
}

swap_idx_t vm_swap_out (void *page) {
    size_t swap_idx = bitmap_scan (swap_avail, 0, 1, true);
    for (size_t i = 0; i < SECTORS_PER_PAGE; ++ i) {
        block_write(swap_block, swap_idx * SECTORS_PER_PAGE + i, page + (BLOCK_SECTOR_SIZE * i));
    }
    bitmap_set(swap_avail, swap_idx, false);
    return swap_idx;
}

void vm_swap_in (swap_idx_t swap_idx, void *page){
    for (size_t i = 0; i < SECTORS_PER_PAGE; ++ i) {
        block_read (swap_block, swap_idx * SECTORS_PER_PAGE + i, page + (BLOCK_SECTOR_SIZE * i));
    }
    bitmap_set(swap_avail, swap_idx, true);
}

void vm_swap_free (swap_idx_t swap_idx){
    bitmap_set(swap_avail, swap_idx, true);
}
