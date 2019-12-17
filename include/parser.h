#ifndef __PARSER_H
#define __PARSER_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>

#include "cachequery.h"
#include "config.h"

/* token */

typedef struct block_t {
	unsigned int id;
	unsigned int set;
	char ask;
	char flush;
	char invalidate;
} block_t;

/* lexer */

typedef struct block_list {
	block_t block;
	struct block_list *next;
} block_list;

typedef struct lexer_state {
	const char *ptr;
	const char *eob;
	block_list *head;
	block_list *tail;
	int size;
} lexer_state;

/* generator */

struct smart_buffer {
	char *start;
	char *p;
	char *limit;
	size_t len;
	size_t asks;
};

/* funcs */

block_list *read_block(lexer_state *state, int max_set);

void lexer_init(lexer_state *lexer);

int parse(lexer_state *lexer, const char *source, int size, int max_set);

void *allocate_code_pages(struct smart_buffer *code, Block **sets, unsigned char level, int s, int pages);

void free_code_pages(struct smart_buffer *code);

void smart_buffer_init(struct smart_buffer *code);

int generate_code(lexer_state *lexer, unsigned int target_set, struct smart_buffer *code, Block **sets, unsigned char level, int t_up, int t_low, Config *conf);

int opcode(struct smart_buffer *code, char *ops, size_t len);
#define OPCODE(code, ops) { if (opcode(code, ops, sizeof(ops)/sizeof(char)) != 0) { goto err; } }

#endif /* __PARSER_H */
