//
// Created by dell on 2020/6/29.
//

#ifndef PINTOS_FRAME_H
#define PINTOS_FRAME_H

#include<hash.h>
#include"lib/kernel/hash.h"
#include"threads/synch.h"
#include"threads/palloc.h"

void vm_frame_init(void);
void *vm_frame_allocate(enum palloc_flags flags, void *usr_page);
void vm_frame_free(void*);
void vm_frame_remove_entry(void*);
void vm_frame_pin(void *ker_page);
void vm_frame_unpin(void *ker_page);


#endif //PINTOS_FRAME_H
