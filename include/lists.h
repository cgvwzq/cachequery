#ifndef __LISTS_H
#define __LISTS_H

#include <linux/types.h>
#include "../config/settings.h"
#include "cachequery.h"

void init_lists(char *pool_l3, char *pool_l2, char *pool_l1);
void init_evictionsets(void);
void reset_sets(void);

Block **get_sets_l1(void);
Block *get_set_l1(int i);
Block **get_sets_l2(void);
Block *get_set_l2(int i);
Block **get_sets_l3(void);
Block *get_set_l3(int i);

ssize_t list_cacheset_addresses (char *buf, Block *set);
void find_l2_eviction_set(Block *set, Block **l2);
void find_l1_eviction_set(Block *set, Block **l1);
void clean_l3_set(int i);
void clean_l2_set(int i);
int list_length(Block *ptr);

#endif /* __HISTOGRAM_H */
