#ifndef __CACHEQUERY_H
#define __CACHEQUERY_H

// Definitions
typedef struct block Block;

struct block
{
        Block *next;
        Block *prev;
        unsigned int set1, set2, set3;
		unsigned int slice;
		Block **evict2, **evict1;
		unsigned int evict2_sz, evict1_sz;
		char pad[8]; // up to 64B
};

// Macros
#define TRUE 1
#define FALSE 0

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif /* __CACHEQUERY_H */