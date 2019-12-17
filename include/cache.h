#ifndef __CACHE_H
#define __CACHE_H

unsigned int get_l3_slice(void *vaddr);
unsigned int get_l3_set(void *vaddr);
unsigned int get_l2_set(void *vaddr);
unsigned int get_l1_set(void *vaddr);

#endif /* __CACHE_H */
