//
// Created by dell on 2020/6/30.
//
#include <string.h>
#include <hash.h>
#include "lib/kernel/hash.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "filesys/file.h"

static unsigned spte_hash_func(const struct hash_elem *elem, void *aux);
static bool spte_less_func(const struct hash_elem *, const struct hash_elem*, void *aux);
static void spte_destroy_func(struct hash_elem *elem, void *aux);


struct supplemental_page_table* vm_supt_create (void){
    struct supplemental_page_table *supt = (struct supplemental_page_table*)malloc(sizeof(struct supplemental_page_table));
    hash_init(&supt->page_hash, spte_hash_func, spte_less_func, NULL);
    return supt;
}

void vm_supt_destroy (struct supplemental_page_table *supt){
    ASSERT (supt != NULL);
    hash_destroy(&supt->page_hash, spte_destroy_func);
    free(supt);
}

bool vm_supt_install_frame (struct supplemental_page_table *supt, void *usr_page, void *ker_page){
    struct supplemental_page_table_entry *spte =
            (struct supplemental_page_table_entry*) malloc (sizeof(struct supplemental_page_table_entry));
    spte->usr_page = usr_page;
    spte->ker_page = ker_page;
    spte->status = ON_FRAME;
    spte->dirty = false;
    spte->swap_idx = -1;
    struct hash_elem *prev_elem;
    prev_elem = hash_insert(&supt->page_hash, &spte->elem);
    if(prev_elem == NULL)
        return true;
    free(supt);
    return false;
}

bool vm_supt_install_zeropage (struct supplemental_page_table *supt, void *usr_page){
    struct supplemental_page_table_entry *spte =
            (struct supplemental_page_table_entry *) malloc (sizeof(struct supplemental_page_table_entry));
    spte->usr_page = usr_page;
    spte->ker_page = NULL;
    spte->status = ALL_ZERO;
    spte->dirty = false;
    struct hash_elem *prev_elem;
    prev_elem = hash_insert(&supt->page_hash, &spte->elem);
    if(prev_elem == NULL)
        return true;
    PANIC ("Duplicated SUPT entry for zeropage");
    return false;
}

bool vm_supt_set_swap (struct supplemental_page_table *supt, void *page, swap_idx_t swap_idx){
    struct supplemental_page_table_entry *spte = vm_supt_lookup(supt, page);
    if(spte == NULL)
        return false;
    spte->ker_page = NULL;
    spte->status =ON_SWAP;
    spte->swap_idx = swap_idx;
    return true;
}

bool vm_supt_install_filesys (struct supplemental_page_table *supt, void *page, struct file * file, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool can_write){
    struct supplemental_page_table_entry * spte =
            (struct supplemental_page_table_entry *)malloc(sizeof(struct supplemental_page_table_entry));
    spte->usr_page = page;
    spte->ker_page = NULL;
    spte->status = FROM_FILESYS;
    spte->dirty = false;
    spte->file = file;
    spte->file_offset = offset;
    spte->read_bytes = read_bytes;
    spte->zero_bytes = zero_bytes;
    spte->can_write = can_write;
    struct hash_elem *prev_elem = hash_insert(&supt->page_hash, &spte->elem);
    if(prev_elem == NULL)
        return true;
    return false;
}

struct supplemental_page_table_entry* vm_supt_lookup (struct supplemental_page_table *supt, void *page){
    struct supplemental_page_table_entry spte;
    spte.usr_page = page;
    struct hash_elem *elem = hash_find(&supt->page_hash, &spte.elem);
    if(elem == NULL)
        return NULL;
    return hash_entry(elem, struct supplemental_page_table_entry, elem);
}

bool vm_supt_has_entry (struct supplemental_page_table *supt, void *page){
    struct supplemental_page_table_entry* spte = vm_supt_lookup(supt, page);
    if(spte == NULL)
        return false;
    return true;
}

bool vm_supt_set_dirty (struct supplemental_page_table *supt, void *page, bool value){
    struct supplemental_page_table_entry *spte = vm_supt_lookup(supt, page);
    if(spte == NULL)
        PANIC("set dirty - the request page doesn't exist");
    spte->dirty = value || spte->dirty;
    return true;
}

static bool vm_load_page_from_filesys(struct supplemental_page_table_entry *spte, void *ker_page){
    file_seek(spte->file, spte->file_offset);
    int read_bytes = file_read(spte->file, ker_page, spte->read_bytes);
    if(read_bytes != (int)spte->read_bytes)
        return false;

    ASSERT (spte->read_bytes+spte->zero_bytes == PGSIZE);
    memset (ker_page+read_bytes, 0, spte->zero_bytes);
    return true;
}

bool vm_load_page(struct supplemental_page_table *supt, uint32_t *pagedir, void *usr_page){
    struct supplemental_page_table_entry *spte = vm_supt_lookup(supt, usr_page);
    if(spte == NULL)
        return false;
    if(spte->status == ON_FRAME)
        return true;

    void *frame_page = vm_frame_alloca(PAL_USER, usr_page);
    if(frame_page == NULL)
        return false;

    bool can_write = true;
    switch(spte->status){
        case ALL_ZERO: memset(frame_page, 0, PGSIZE); break;
        case ON_SWAP: vm_swap_in(spte->swap_idx, frame_page); break;
        case ON_FRAME: break;
        case FROM_FILESYS:
            if(!vm_load_page_from_filesys(spte, frame_page)){
                vm_frame_free(frame_page);
                return false;
            }
            can_write = spte->can_write;
            break;
        default:
            PANIC ("unreachable state");
    }

    if(!pagedir_set_page(pagedir, usr_page, frame_page, can_write)){
        vm_frame_free(frame_page);
        return false;
    }

    spte->ker_page = frame_page;
    spte->status = ON_FRAME;
    pagedir_set_dirty(pagedir, frame_page, false);

    vm_frame_unpin(frame_page);
    return true;
}

bool vm_supt_mm_unmap(struct supplemental_page_table *supt, uint32_t *pagedir, void *page, struct file *file, off_t offset, size_t bytes){
    struct supplemental_page_table_entry *spte = vm_supt_lookup(supt, page);
    if(spte == NULL)
        PANIC ("munmap - some page is missing; can't happen!");
    if(spte->status == ON_FRAME){
        ASSERT (spte->ker_page != NULL);
        vm_frame_pin(spte->ker_page);
    }

    switch(spte->status){
        case ON_SWAP:{
            bool dirty = spte->dirty;
            dirty = dirty || pagedir_is_dirty(pagedir, spte->usr_page);
            if(dirty) {
                void *tmp_page = palloc_get_page(0);
                vm_swap_in(spte->swap_idx, tmp_page);
                file_write_at(file, tmp_page, PGSIZE, offset);
                palloc_free_page(tmp_page);
            }
            else
                vm_swap_free(spte->swap_idx);
           }
           break;
        case ON_FRAME:
            ASSERT (spte->ker_page != NULL);
            bool dirty = spte->dirty;
            dirty = dirty || pagedir_is_dirty(pagedir, spte->usr_page);
            dirty = dirty || pagedir_is_dirty(pagedir, spte->ker_page);
            if(dirty)
                file_write_at(file, spte->usr_page, bytes, offset);
            vm_frame_free(spte->ker_page);
            pagedir_clear_page(pagedir, spte->usr_page);
            break;
        case FROM_FILESYS: break;
        default:
            PANIC ("unreachable state");
    }

    hash_delete(&supt->page_hash, &spte->elem);
    return true;
}

void vm_pin_page(struct supplemental_page_table *supt, void *page){
    struct supplemental_page_table_entry *spte = vm_supt_lookup(supt, page);
    if(spte == NULL)
        return;
    ASSERT(spte->status == ON_FRAME);
    vm_frame_pin(spte->ker_page);
}

void vm_unpin_page(struct supplemental_page_table *supt, void *page){
    struct supplemental_page_table_entry * spte = vm_supt_lookup(supt, page);
    if(spte == NULL)
        PANIC ("request page is not exist");
    if(spte->status == ON_FRAME)
        vm_frame_unpin(spte->ker_page);
}

static unsigned spte_hash_func(const struct hash_elem *elem, void *aux UNUSED){
    struct supplemental_page_table_entry *spte = hash_entry(elem, struct supplemental_page_table_entry, elem);
    return hash_int((int)spte->usr_page);
}

static bool spte_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
    struct supplemental_page_table_entry *a_entry = hash_entry(a, struct supplemental_page_table_entry, elem);
    struct supplemental_page_table_entry *b_entry = hash_entry(b, struct supplemental_page_table_entry, elem);
    return a_entry->usr_page < b_entry->usr_page;
}

static void spte_destroy_func(struct hash_elem *elem, void *aux UNUSED){
    struct supplemental_page_table_entry *spte = hash_entry(elem, struct supplemental_page_table_entry, elem);
    if(spte->ker_page != NULL){
        ASSERT (spte->status == ON_FRAME);
        vm_frame_remove(spte->ker_page);
    }
    else if(spte->status == ON_SWAP)
        vm_swap_free(spte->swap_idx);
    free(spte);
}