#include "../include/cache.h"

#include <linux/mm.h>

#include "../config/settings.h"

unsigned long long vtop(void *vaddr)
{
	return (vmalloc_to_pfn (vaddr) << 12) | ((unsigned long long)vaddr & (PAGE_SIZE-1));
}

unsigned int count_bits(unsigned long long n)
{
	unsigned int count = 0;
	while (n)
	{
		n &= (n-1);
		count++;
	}
	return count;
}

unsigned int get_l3_slice(void *vaddr)
{
	int i = L3_SLICE_BITS - 1;
	unsigned int ret = 0;
	unsigned long long paddr = vtop (vaddr);
	unsigned long long mask[3] = {0x1b5f575440ULL, 0x2eb5faa880ULL, 0x3cccc93100ULL};
	while (i >= 0 && i < 3)
	{
		ret = (ret << 1) | (count_bits (mask[i] & paddr) % 2);
		i--;
	}
	return ret;
}

unsigned int get_l1_set(void *vaddr)
{
	return ((unsigned long long) (vaddr) >> L1_LINE_BITS) & ((L1_CACHE_SETS/L1_CACHE_SLICES) - 1);
}

unsigned int get_l2_set(void *vaddr)
{
	// is virtual or physically indexed?
#if L2_PHYS_INDEXED == 1
	return (vtop (vaddr) >> L2_LINE_BITS) & ((L2_CACHE_SETS/L2_CACHE_SLICES) - 1);
#else
	return ((unsigned long long)(vaddr) >> L2_LINE_BITS) & ((L2_CACHE_SETS/L2_CACHE_SLICES) - 1);
#endif
}

unsigned int get_l3_set(void *vaddr)
{
	return (vtop (vaddr) >> L3_LINE_BITS) & ((L3_CACHE_SETS/L3_CACHE_SLICES) - 1);
}

