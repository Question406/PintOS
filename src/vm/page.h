//
// Created by dell on 2020/6/30.
//

#ifndef PINTOS_PAGE_H
#define PINTOS_PAGE_H

#include <hash.h>
#include "vm/swap.h"
#include "filesys/off_t.h"

enum page_status{
    ALL_ZERO,
    ON_FRAME,
    ON_SWAP,
    FROM_FILESYS
};

struct supplemental_page_table_entry{
    void *usr_page;
    void *ker_page;
    enum page_status status;
    bool dirty;

    struct hash_elem elem;

    swap_idx_t swap_idx;
    struct file *file;
    off_t file_offset;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool can_write;
};

struct supplemental_page_table{
    struct hash page_hash;
};

struct supplemental_page_table* vm_supt_create (void);
void vm_supt_destroy (struct supplemental_page_table *);
bool vm_supt_install_frame (struct supplemental_page_table *supt, void *usr_page, void *ker_page);
bool vm_supt_install_zeropage (struct supplemental_page_table *supt, void *);
bool vm_supt_set_swap (struct supplemental_page_table *supt, void *, swap_idx_t);
bool vm_supt_install_filesys (struct supplemental_page_table *supt, void *page, struct file * file, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool can_write);

struct supplemental_page_table_entry* vm_supt_lookup (struct supplemental_page_table *supt, void *);
bool vm_supt_has_entry (struct supplemental_page_table *, void *page);
bool vm_supt_set_dirty (struct supplemental_page_table *supt, void *, bool);
bool vm_load_page(struct supplemental_page_table *supt, uint32_t *pagedir, void *usr_page);
bool vm_supt_mm_unmap(struct supplemental_page_table *supt, uint32_t *pagedir, void *page, struct file *file, off_t offset, size_t bytes);
void vm_pin_page(struct supplemental_page_table *supt, void *page);
void vm_unpin_page(struct supplemental_page_table *supt, void *page);

#endif //PINTOS_PAGE_H
