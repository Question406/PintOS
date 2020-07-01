//
// Created by dell on 2020/6/29.
//

#ifndef PINTOS_SWAP_H
#define PINTOS_SWAP_H

typedef uint32_t swap_idx_t;

void vm_swap_init(void);
swap_idx_t vm_swap_out(void *page);
void vm_swap_in(swap_idx_t swap_idx, void *page);
void vm_swap_free(swap_idx_t swap_idx);

#endif //PINTOS_SWAP_H
