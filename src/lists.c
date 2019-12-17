#include "../include/lists.h"
#include "../config/settings.h"
#include "../include/cache.h"
#include "../include/cachequery.h"

#include <linux/random.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

static Block *sets_l3[L3_CACHE_SETS];
static Block *sets_l2[L2_CACHE_SETS];
static Block *sets_l1[L1_CACHE_SETS];

Block** get_sets_l3(void)
{
	return sets_l3;
}

Block* get_set_l3(int i)
{
	return sets_l3[i];
}

Block** get_sets_l2(void)
{
	return sets_l2;
}

Block* get_set_l2(int i)
{
	return sets_l2[i];
}

Block** get_sets_l1(void)
{
	return sets_l1;
}

Block* get_set_l1(int i)
{
	return sets_l1[i];
}

void buffer_to_cachesets(char *pool, size_t length, Block **sets, int lvl)
{
	Block *ptr = NULL;
	size_t i = 0;
	unsigned int set = 0;
	while (i < length)
	{
		ptr = (Block*)(&pool[i]);
		ptr->set3 = get_l3_set (ptr);
		ptr->slice = get_l3_slice (ptr);
		ptr->set2 = get_l2_set (ptr);
		ptr->set1 = get_l1_set (ptr);
		ptr->evict2 = NULL;
		ptr->evict2_sz = 0;
		ptr->evict1 = NULL;
		ptr->evict1_sz = 0;
		switch (lvl)
		{
			case LVL_1:
				set = ptr->set1;
				break;
			case LVL_2:
				set = ptr->set2;
				break;
			case LVL_3:
			default:
				set = (ptr->set3 << L3_SLICE_BITS) | ptr->slice;
				break;
		}
		// add virtual address to corresponding list
		if (sets[set])
		{
			ptr->next = sets[set];
			sets[set]->prev = ptr;
		}
		else
		{
			ptr->next = NULL;
		}
		ptr->prev = NULL;
		sets[set] = ptr;
		i += sizeof(Block);
	}
}

void shuffle_list(Block **ptr, int n)
{
	Block *a, *b, *tmp;
	int i, j, r, c;
	if (*ptr && n > 2)
	{
		for (i = n - 1; i > 1; i--)
		{
			get_random_bytes(&r, sizeof(r));
			j = 1 + ((r & 0x7FFFFFFF) % (i - 1));
			for (b = *ptr, c = 0; b && c < i; b = b->next, c++); // b points to i-th element
			for (a = *ptr, c = 0; a && c < j; a = a->next, c++); // a points to j-th element
			if (a->next == b || b->prev == a || a->next == b->prev) continue;
			// swap siblings
			if (a->next)
				a->next->prev = b;
			if (a->prev)
				a->prev->next = b;
			if (b->next)
				b->next->prev = a;
			if (b->prev)
				b->prev->next = a;
			// swap elements
			tmp = a->prev;
			a->prev = b->prev;
			b->prev = tmp;
			tmp = a->next;
			a->next = b->next;
			b->next = tmp;
		}
	}
}

void init_lists(char *pool_l3, char *pool_l2, char *pool_l1)
{
	int i;
	// Empty lists
	for (i = 0; i < L3_CACHE_SETS; i++)
	{
		sets_l3[i] = NULL;
	}
	for (i = 0; i < L2_CACHE_SETS; i++)
	{
		sets_l2[i] = NULL;
	}
	for (i = 0; i < L1_CACHE_SETS; i++)
	{
		sets_l1[i] = NULL;
	}

	// Init structure
	buffer_to_cachesets (pool_l3, L3_POOL_SZ, sets_l3, LVL_3);
	for (i = 0; i < L3_CACHE_SETS; i++)
	{
		shuffle_list (&sets_l3[i], list_length (sets_l3[i]));
	}
	buffer_to_cachesets (pool_l2, L2_POOL_SZ, sets_l2, LVL_2);
	for (i = 0; i < L2_CACHE_SETS; i++)
	{
		shuffle_list (&sets_l2[i], list_length (sets_l2[i]));
	}
	buffer_to_cachesets (pool_l1, L1_POOL_SZ, sets_l1, LVL_1);
	for (i = 0; i < L1_CACHE_SETS; i++)
	{
		shuffle_list (&sets_l1[i], list_length (sets_l1[i]));
	}
}

void init_evictionsets(void)
{
	int i;
	Block *head = NULL, *ptr = NULL;
	// Find L1 eviction sets for L3 addresses, L2's are computed lazily on code generation
	for (i = 0; i < L3_CACHE_SETS; i++)
	{
		head = sets_l3[i];
		find_l1_eviction_set (head, sets_l1);
		ptr = head;
		while (ptr)
		{
			// L3 congruent addresses share L1 eviction sets
			ptr->evict1 = head->evict1;
			ptr->evict1_sz = head->evict1_sz;
			ptr = ptr->next;
		}
	}
	// Find eviction sets for L2 addresses
	head = NULL;
	ptr = NULL;
	for (i = 0; i < L2_CACHE_SETS; i++)
	{
		head = sets_l2[i];
		find_l1_eviction_set (head, sets_l1);
		ptr = head;
		while (ptr)
		{
			// L2 congruent addresses share L1 eviction sets
			ptr->evict1 = head->evict1;
			ptr->evict1_sz = head->evict1_sz;
			ptr = ptr->next;
		}
	}
}

void clean_l3_set (int i)
{
	Block *ptr = sets_l3[i];;
	while (ptr)
	{
		kfree (ptr->evict2);
		ptr = ptr->next;
	}
	kfree (sets_l3[i]->evict1);
}

void clean_l2_set(int i)
{
	Block *ptr = sets_l2[i];
	kfree (ptr->evict1);
}

ssize_t list_cacheset_addresses (char *buf, Block *set)
{
	ssize_t ret = 0;
	Block *ptr = set;
	while (ptr && ret < PAGE_SIZE - 1)
	{
		ret += sprintf(&buf[ret], "%px ", ptr);
		ptr = ptr->next;
	}
	ret += sprintf(&buf[ret], "\n");
	return ret;
}

void find_l2_eviction_set(Block *set, Block **l2)
{
	unsigned int i, count;
	Block *ptr;
	// Eviction set for L2
	set->evict2 = kmalloc (MARGIN * EVICTION_SZ(L2) * sizeof(Block*), GFP_KERNEL);
	count = 0;
	for (i = 0; i < L2_CACHE_SETS && count < MARGIN * EVICTION_SZ(L2); i++)
	{
		ptr = l2[i];
		while (ptr && count < MARGIN * EVICTION_SZ(L2))
		{
			if ((ptr->set2 == set->set2) && (ptr->set3 != set->set3 || ptr->slice != set->slice))
			{
				set->evict2[count] = ptr;
				count++;
			}
			ptr = ptr->next;
		}
	}
	set->evict2_sz = count;
}

void find_l1_eviction_set(Block *set, Block **l1)
{
	unsigned int i, count;
	Block *ptr;
	set->evict1 = kmalloc (MARGIN * EVICTION_SZ(L1) * sizeof(Block*), GFP_KERNEL);
	count = 0;
	for (i = 0; i < L1_CACHE_SETS && count < MARGIN * EVICTION_SZ(L1); i++)
	{
		ptr = l1[i];
		while (ptr && count < MARGIN * EVICTION_SZ(L1))
		{
			// find same L1 set, but different L3 and L2
			if ((ptr->set1 == set->set1) && (ptr->set2 != set->set2) && (ptr->set3 != set->set3 || ptr->slice != set->slice))
			{
				set->evict1[count] = ptr;
				count++;
			}
			ptr = ptr->next;
		}
	}
	set->evict1_sz = count;
}

int list_length(Block *ptr)
{
	int l = 0;
	while (ptr)
	{
		l = l + 1;
		ptr = ptr->next;
	}
	return l;
}
