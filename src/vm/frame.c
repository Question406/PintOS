//
// Created by dell on 2020/6/29.
//
#include <hash.h>
#include <list.h>
#include <stdio.h>

#include "vm/frame.h"

#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"

static struct lock frame_lock;
static struct hash frame_hash;
static struct list frame_list;
static struct list_elem *clk_pointer;

struct frame_table_entry{
    void *ker_page;
    void *usr_page;
    struct hash_elem h_elem;
    struct list_elem l_elem;

    struct thread *thd;
    bool pinned;
};

static struct frame_table_entry *pick_frame_to_evicted(uint32_t* pagedir);
static void vm_frame_do_free(void *ker_page, bool free_page);

static unsigned frame_hash_func(const struct hash_elem *elem, void *aux);
static bool frame_less_func(const struct hash_elem *, const struct hash_elem *, void *aux);

void vm_frame_init(){
    lock_init(&frame_lock);
    hash_init(&frame_hash, frame_hash_func, frame_less_func, NULL);
    list_init(&frame_list);
    clk_pointer = NULL;
}

void *vm_frame_allocate(enum palloc_flags flags, void *usr_page){
    lock_acquire(&frame_lock);
    void *frame_page = palloc_get_page(PAL_USER|flags);
    if(frame_page == NULL){
        struct frame_table_entry *page_evicted = pick_frame_to_evicted(thread_current()->pagedir);
        ASSERT (page_evicted != NULL && page_evicted->thd != NULL);

        ASSERT (page_evicted -> thd ->pagedir != (void*)0xcccccccc);
        pagedir_clear_page(page_evicted->thd->pagedir, page_evicted->usr_page);

        bool dirty = false;
        dirty = dirty || pagedir_is_dirty(page_evicted->thd->pagedir, page_evicted->usr_page);
        dirty = dirty || pagedir_is_dirty(page_evicted->thd->pagedir, page_evicted->ker_page);

        swap_idx_t swap_idx = vm_swap_out(page_evicted->ker_page);
        vm_supt_set_swap(page_evicted->thd->supt, page_evicted->usr_page, swap_idx);
        vm_supt_set_dirty(page_evicted->thd->supt, page_evicted->usr_page, dirty);
        vm_frame_do_free(page_evicted->ker_page, true);
        frame_page = palloc_get_page(PAL_USER|flags);
        ASSERT (frame_page != NULL);
    }

    struct frame_table_entry *frame = malloc(sizeof(struct frame_table_entry));
    if(frame == NULL) {
        lock_release(&frame_lock);
        return NULL;
    }

    frame->thd = thread_current();
    frame->usr_page = usr_page;
    frame->ker_page = frame_page;
    frame->pinned = true;
    hash_insert(&frame_hash, &frame->h_elem);
    list_push_back(&frame_list, &frame->l_elem);

    lock_release(&frame_lock);
    return frame_page;
}

void vm_frame_free(void* ker_page){
    lock_acquire(&frame_lock);
    vm_frame_do_free(ker_page, true);
    lock_release(&frame_lock);
}

void vm_frame_remove_entry(void* ker_page){
    lock_acquire(&frame_lock);
    vm_frame_do_free(ker_page, false);
    lock_release(&frame_lock);
}

static void vm_frame_set_pinned(void *ker_page, bool new_value){
    lock_acquire(&frame_lock);
    struct frame_table_entry tmp;
    tmp.ker_page = ker_page;
    struct hash_elem *h_elem = hash_find(&frame_hash,&(tmp.h_elem));
    if(h_elem == NULL){
        //printf("%d", 1);
        PANIC("The frame to be pinned/unpinned does not exist");
    }
    struct frame_table_entry *f;
    f = hash_entry(h_elem, struct frame_table_entry, h_elem);
    f->pinned = new_value;
    lock_release(&frame_lock);
}

void vm_frame_pin(void *ker_page){
    vm_frame_set_pinned(ker_page, true);
}

void vm_frame_unpin(void *ker_page){
    vm_frame_set_pinned(ker_page, false);
}

void vm_frame_do_free(void *ker_page, bool free_page){
    ASSERT (lock_held_by_current_thread(&frame_lock) == true);
    ASSERT (is_kernel_vaddr(ker_page));
    ASSERT (pg_ofs(ker_page) == 0);

    struct frame_table_entry tmp;
    tmp.ker_page = ker_page;
    struct hash_elem *h_elem = hash_find(&frame_hash, &(tmp.h_elem));
    if(h_elem == NULL){
        PANIC("The page to be freed is not stored in the table");
    }

    struct frame_table_entry *f;
    f = hash_entry(h_elem, struct frame_table_entry, h_elem);
    hash_delete(&frame_hash, &f->h_elem);
    list_remove(&f->l_elem);
    if(free_page)
        palloc_free_page(ker_page);
    free(f);
}

struct frame_table_entry* clk_frame_next(void){
    if(list_empty(&frame_list)){
        PANIC("Frame table is empty, can't happen - there is a leak somewhere");
    }
    if(clk_pointer == NULL || clk_pointer == list_end(&frame_list)){
        clk_pointer = list_begin(&frame_list);
    }
    else{
        clk_pointer = list_next(clk_pointer);
    }
    struct frame_table_entry *f = list_entry(clk_pointer, struct frame_table_entry, l_elem);
    return f;
}

struct frame_table_entry* pick_frame_to_evicted(uint32_t* pagedir){
    size_t n = hash_size(&frame_hash);
    if(n==0){
        PANIC("Frame table is empty, can't happen - there is a leak somewhere");
    }
    for(size_t i = 0; i<=n+n; ++i){
        struct frame_table_entry *f = clk_frame_next();
        if(f->pinned) continue;
        if(pagedir_is_accessed(pagedir, f->usr_page)){
            pagedir_set_accessed(pagedir, f->usr_page, false);
            continue;
        }
        return f;
    }
    PANIC ("Can't evict any frame -- Not enough memory!\n");
}

static unsigned frame_hash_func(const struct hash_elem *elem, void *aux UNUSED){
    struct frame_table_entry *f = hash_entry(elem, struct frame_table_entry, h_elem);
    return hash_bytes(&f->ker_page, sizeof f->ker_page);
}

static bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
    struct frame_table_entry *a_entry = hash_entry(a, struct frame_table_entry, h_elem);
    struct frame_table_entry *b_entry = hash_entry(b, struct frame_table_entry, h_elem);
    return a_entry->ker_page < b_entry->ker_page;
}
