#include "../include/parser.h"

#include <linux/vmalloc.h>
#include <linux/string.h>

#include "../include/x86.h"
#include "../include/msrdrv.h"
#include "../include/config.h"
#include "../include/cache.h"
#include "../include/lists.h"

static int is_digit_char(char c) {
	return c >= '0' && c <= '9';
}

static int is_separator_char(char c)
{
	return c == '_';
}

static int is_query_char(char c)
{
	return c == '?';
}

static int is_flush_char(char c)
{
	return c == '!';
}

#define MAX_NUM_DIGITS 6
int str_to_num(const char *ptr, int len)
{
	int res;
	char buf[MAX_NUM_DIGITS];
	if (!ptr || len > MAX_NUM_DIGITS)
	{
		return -1;
	}
	strncpy (buf, ptr, len);
	buf[len] = 0;
	if (!kstrtoint (buf, 10, &res))
	{
		return res;
	}
	return -1;
}

block_list *read_block(lexer_state *state, int max_set)
{
	const char *left, *right, *eob;
	char c;
	block_list *t;

	if (!state || !state->ptr || !(state->ptr < state->eob) || (*(state->ptr) == '\0'))
	{
		return NULL;
	}
	t = kmalloc (sizeof(block_list), GFP_KERNEL);
	if (!t)
	{
		printk (KERN_INFO "Error on block allocation\n");
		return NULL;
	}
	left = right = state->ptr;
	eob = state->eob;
	c = 0;
	// ignore padding
	while ((left < eob) && (c = *left) && ((c == ' ') || (c == '\n') || (c == '\t'))) left++;
	right = left;
	if (!(left < eob) || (*left == '\0'))
	{
		goto sink;
	}
	if (is_flush_char(c))
	{
		t->block.id = 0; // set to something
		t->block.ask = 0;
		t->block.flush = 0;
		t->block.invalidate = 1;
		left = ++right;
		// insert in list
		state->ptr = left;
		if (state->tail)
		{
			state->tail->next = t;
			state->tail = t;
		}
		else
		{
			state->tail = state->head = t;
		}
	}
	// TODO: remove code to ignore cacheset after _, now is defined by path
	// or add proper support to interleave mem accesses
	else if (is_digit_char(c))
	{
		while ((right < eob) && is_digit_char(c)) c = *(++right);
		// assign unique id for name from left-to-right
		t->block.id = str_to_num(left, right-left);
		left = right;
		if (!is_separator_char(c)) {
			goto err;
		}
		left = ++right;
		if ((left < eob) && (c = *left) && is_digit_char(c))
		{
			while ((right < eob) && is_digit_char(c)) c = *(++right);
			t->block.set = str_to_num(left, right-left);
			// fix this! :S
			if (t->block.set >= max_set)
			{
				goto err;
			}
			left = right;
			if (is_query_char(c))
			{
				t->block.ask = 1;
				t->block.flush = 0;
				t->block.invalidate = 0;
				left = ++right;
			}
			else if (is_flush_char(c))
			{
				t->block.flush = 1;
				t->block.ask = 0;
				t->block.invalidate = 0;
				left = ++right;
			}
			else
			{
				t->block.ask = 0;
				t->block.flush = 0;
				t->block.invalidate = 0;
			}
			// insert in list
			state->ptr = left;
			if (state->tail)
			{
				state->tail->next = t;
				state->tail = t;
			}
			else
			{
				state->tail = state->head = t;
			}
		}
		else
		{
			goto err;
		}
	}
	else
	{
		goto err;
	}
	t->next = NULL;
	return t;
	err:
		printk (KERN_ERR "Invalid syntax\n");
	sink:
		kfree (t);
		return NULL;
}

void lexer_init(lexer_state *lexer)
{
	lexer->ptr = NULL;
	lexer->eob = NULL;
	lexer->size = 0;
	lexer->head = NULL;
	lexer->tail = NULL;
}

void lexer_clean(lexer_state *lexer)
{
	block_list *t, *tmp;

	if (lexer == NULL)
	{
		return;
	}
	t = lexer->head;
	while (t)
	{
		tmp = t->next;
		kfree (t);
		t = tmp;
	}
	lexer->size = 0;
	lexer->ptr = lexer->eob = NULL;
	lexer->tail = lexer->head = NULL;
}

int parse(lexer_state *lexer, const char *source, int size, int max_set)
{
	unsigned int n = 0;
	block_list *b;

	// reset for reuse
	lexer_clean (lexer);

	lexer->ptr = source;
	lexer->eob = source + size;
	lexer->size = size;
	while ((b = read_block (lexer, max_set))) n++;
	return n;
}

void* allocate_code_pages(struct smart_buffer *code, Block **sets, unsigned char level, int s, int pages)
{
	void *ret;
	int i, code_l3, code_slice, code_l2, code_l1, count = 0, max_attempts = 10000;

alloc:
	count++;
	if (!(ret = __vmalloc (pages*PAGE_SIZE, GFP_KERNEL, PAGE_KERNEL_EXEC)))
	{
		return NULL;
	}

	// check that code doesn't interfer with eviction sets
	for (i = 0; i < pages && count < max_attempts; i++)
	{
		code_l3 = get_l3_set ((void*) (ret+(i*PAGE_SIZE)));
		code_slice = get_l3_slice ((void*) (ret+(i*PAGE_SIZE)));
		code_l2 = get_l2_set ((void*) (ret+(i*PAGE_SIZE)));
		code_l1 = get_l1_set ((void*) (ret+(i*PAGE_SIZE)));

		// can't create large portions of code w/o filling l2
		if (level == 3)
		{
			if (((code_l3>>6) == (sets[s]->set3>>6)) && (code_slice  == sets[s]->slice))
			{
				WPRINT ("[debug] Warning: code interfering with debugged cache set\n");
				vfree (ret);
				goto alloc;
			}
		}
		else if (level == 2 || level == 1)
		{
			if ((((code_l3>>6) == (sets[s]->set3>>6)) && (code_slice == sets[s]->slice))
				|| ((code_l2>>6) == (sets[s]->set2>>6)))
			{
				WPRINT ("[debug] Warning: code interfering with debugged cache set\n");
				vfree (ret);
				goto alloc;
			}
		}
	}

	code->start = ret;
	code->p = ret;
	code->limit = (char*)((unsigned long long)(ret) + pages*PAGE_SIZE);
	code->len = 0;

	if (!(count < max_attempts))
	{
		PRINT ("[debug] couldn't find a non-interference blob\n");
	}
	PRINT ("[debug] code allocation at 0x%p-0x%p\n", code->p, code->limit);

	return ret;
}

void free_code_pages (struct smart_buffer *code)
{
	vfree (code->start);
	smart_buffer_init (code);
}

void smart_buffer_init (struct smart_buffer *code)
{
	code->start = NULL;
	code->p = NULL;
	code->limit = NULL;
	code->len = 0;
	code->asks = 0;
}

// Just use this region for all sets to avoid wasting time with dynamic memory
static unsigned int banned_l3_for_l1[L3_CACHE_SETS];
static unsigned int banned_l3_for_l2[L3_CACHE_SETS];
static unsigned int banned_l2_for_l1[L2_CACHE_SETS];
static Block* query_blocks[128];

// copy array of opcodes (x86.h) into buffer if still fits, if not return -1
int opcode(struct smart_buffer *code, char *ops, size_t len)
{
	if ((code->p + len) >= code->limit)
	{
		return -1;
	}
	memcpy (code->p, ops, len);
	code->p += len;
	code->len += len;
	return 0;
}

int generate_code(lexer_state *lexer, unsigned int target_set, struct smart_buffer *code, Block **sets, unsigned char level, int t_up, int t_low, Config *conf)
{
	block_list *b;
	Block *tmp, *sub;
	int i, w1 = 0, w2 = 0;
	size_t asks = 0;
	int error_label = 0, jmp_at, s;
	unsigned long long err_code = -1;
	Block *others;

	// Check lexer state
	if (!lexer || !lexer->head || !lexer->tail)
	{
		goto err;
	}
	b = lexer->head;

	// If necessary, compute L2 eviction sets on demand
	s = target_set;
	if (level == 3 && sets[s]->evict2 == NULL)
	{
		tmp = sets[s];
		while (tmp)
		{
			find_l2_eviction_set (tmp, get_sets_l2());
			tmp = tmp->next;
		}
	}

	// If cacheset had code, free it and generate new
	if (code->start != NULL)
	{
		free_code_pages (code);
	}

	if (allocate_code_pages (code, sets, level, s, JIT_CODE_SZ) == NULL)
	{
		goto err;
	}

	// Preamble
	OPCODE(code, PUSH_RBP());
	OPCODE(code, MOV_RBP_RSP());
	OPCODE(code, PUSH_RBX());

	jmp_at = 15;
	OPCODE(code, JMP_SHORT(jmp_at)); // skip error handler
	// exception handler
	error_label = code->len;
	OPCODE(code, MOV_RAX_CT(err_code));
	OPCODE(code, POP_RBX());
	OPCODE(code, POP_RBP());
	OPCODE(code, RETQ());
	// end-of-exception (total size: 2 + 10 + 1 + 1 + 1)

	OPCODE(code, XOR_RSI_RSI()); // set return value to zero

	// Identify cache sets that need to be avoided by eviction sets
	memset (banned_l3_for_l1, 0, sizeof(banned_l3_for_l1));
	memset (banned_l3_for_l2, 0, sizeof(banned_l3_for_l2));
	memset (banned_l2_for_l1, 0, sizeof(banned_l2_for_l1));
	memset (query_blocks, 0, sizeof(query_blocks));

	// ban sets used by code to avoid them when forming eviction sets
	for (i = 0; i < JIT_CODE_SZ; i+=sizeof(Block))
	{
		banned_l3_for_l2[get_l3_set ((void*) (code->start+i)) | get_l3_slice ((void*) (code->start+i))]++;
		banned_l3_for_l1[get_l3_set ((void*) (code->start+i)) | get_l3_slice ((void*) (code->start+i))]++;
		banned_l2_for_l1[get_l2_set ((void*) (code->start+i))]++;
	}

	// NOTE: RIGHT NOW WE IGNORE THE CACHE_SET
	// Safe pointers to query blocks
	tmp = sets[s];

	while (b)
	{
		if (query_blocks[b->block.id])
		{
			b = b->next;
			continue;
		}
		while (tmp)
		{
			// if block doesn't collide in lower levels with previous query block
			// (for L2 blocks we fix slice=4)
			if (((level == 1) && banned_l3_for_l1[(tmp->set3 << L3_SLICE_BITS) | tmp->slice] == 0 && banned_l2_for_l1[tmp->set2] == 0)
				|| ((level == 2) && (tmp->slice == L3_CACHE_SLICES-1) && banned_l3_for_l2[(tmp->set3 << L3_SLICE_BITS) | tmp->slice] == 0)
				|| (level == 3))
			{
				break;
			}
			else
			{
				tmp = tmp->next;
			}
		}
		if (!tmp)
		{
			goto beach;
		}
		// store it
		query_blocks[b->block.id] = tmp;
		// ban it
		if (tmp->evict1_sz > 0)
		{
			banned_l3_for_l1[((tmp->set3 << L3_SLICE_BITS) | tmp->slice)]++;
			banned_l2_for_l1[tmp->set2]++;
		}
		if (tmp->evict2_sz > 0)
		{
			banned_l3_for_l2[((tmp->set3 << L3_SLICE_BITS) | tmp->slice)]++;
		}
		// next
		tmp = tmp->next;
		b = b->next;
	}

	// Invalidate cache hierarchy
	// OPCODE(code, WBINVD());

	// Access and flush all blocks in set
	tmp = sets[s];
	while (tmp)
	{
		OPCODE(code, LOAD_RAX((unsigned long long)tmp));
		OPCODE(code, SERIALIZE());
		OPCODE(code, LOAD_RAX((unsigned long long)tmp));
		OPCODE(code, SERIALIZE());
		OPCODE(code, LOAD_RAX((unsigned long long)tmp));
		OPCODE(code, SERIALIZE());
		tmp = tmp->next;
	}
	tmp = sets[s];
	while (tmp)
	{
		OPCODE(code, MOV_RAX_CT((unsigned long long)tmp));
		OPCODE(code, CLFLUSH_RAX());
		tmp = tmp->next;
	}
	OPCODE(code, SERIALIZE());

	PRINT("[debug] ---------------------------------\n");

	// Start query
	b = lexer->head;
	while (b)
	{
		// Set pointer into right block
		tmp = query_blocks[b->block.id];
		if (!tmp)
		{
			goto beach;
		}

		OPCODE(code, CPUID());

		if (b->block.invalidate)
		{
			PRINT ("[debug] invalidate cache\n");
			OPCODE(code, WBINVD());
			OPCODE(code, SERIALIZE());
			goto cont;
		}
		// If a flush req
		else if (b->block.flush)
		{
			PRINT ("[debug] flush %p (%u):\ts1=%d\ts2=%d\ts3=%d\th=%d\n",
				tmp, b->block.id, tmp->set1, tmp->set2, tmp->set3, tmp->slice);
			OPCODE(code, MOV_RAX_CT((unsigned long long)tmp));
			OPCODE(code, CLFLUSH_RAX());
			OPCODE(code, SERIALIZE());
			goto cont; // continue w/o accessing block
		}
		// Do we profile this memory access?
		else if ((get_only_one_time(conf) && b->next == NULL)
			|| (b->block.ask && asks < 63))
		{
			// Perform TLB preload if set
			if (get_tlb_preload(conf) && tmp->evict1_sz > 0)
			{
				unsigned long long tlb = ((unsigned long long)tmp);
				tlb = ((tlb >> 12) << 12) | ((tlb & 0xfff) ^ 0x7e0); // same page, different cache set
				PRINT("[debug] tlb refresh %llx s1=%d s2=%d s3=%d h=%d\n",
					tlb, get_l1_set((void*)tlb), get_l2_set((void*)tlb), get_l3_set((void*)tlb), get_l3_slice((void*)tlb));
				OPCODE(code, LOAD_RAX(tlb));
				OPCODE(code, SERIALIZE());
			}
			if (get_use_pmc(conf) || get_core_cycles(conf))
			{
				// Reset performance counters
				RESET_PMC0(code); // (wrmsr serializes)
			}
			else
			{
				// RAX,RDX <- RDTSC
				// RDI <- RAX | (RDX << 32)
				MEASURE_PRE_TSC(code);
			}
		}

		// Access block
		OPCODE(code, LOAD_RAX((unsigned long long)tmp));

		// Do we profile this memory access?
		if ((get_only_one_time(conf) && b->next == NULL && !b->block.flush)
			|| (b->block.ask && asks < 63))
		{
			if (get_use_pmc(conf))
			{
				// Read performance counters
				// rdmsr PCM0
				OPCODE(code, SERIALIZE());
				OPCODE(code, MOV_ECX_DWORD(_MSR_IA32_PMC0, 0x00, 0x00, 0x00));
				OPCODE(code, RDMSR());
				if (!get_only_one_time(conf))
				{
					// if counter was increased
					OPCODE(code, MOV_RDI_DWORD(0x01, 0x00, 0x00, 0x00));
					OPCODE(code, CMP_EAX_EDI());
					// update bit
					OPCODE(code, CMOVAE_EAX_EDI());
					OPCODE(code, SHL_RSI());
				}
				else
				{
					// result already in RAX
				}
			}
			else
			{
				if (get_core_cycles(conf))
				{
					// RDI <- cycle since RESET_PMC()
					MEASURE_POST_CORE(code);
				}
				else
				{
					// RAX,RDX <- RDTSC
					// RDI <- RDI - RAX
					// NEG RDI
					MEASURE_POST_TSC(code);
				}
				if (!get_only_one_time(conf))
				{
					// Compare with threshold and update result bitmask
					OPCODE(code, MOV_RDX_RDI());
					OPCODE(code, XOR_RAX_RAX());
					OPCODE(code, MOV_RDI_DWORD(0x01, 0x00, 0x00, 0x00));
					// if time less or equal than t_low goto err
					OPCODE(code, CMP_RDX_CT(t_low));
					jmp_at = error_label - code->len; // calc relative position
					OPCODE(code, JBE_NEAR(jmp_at));
					// if time above or equal than t_up set bit to 1
					OPCODE(code, CMP_RDX_CT(t_up));
					// update bit
					OPCODE(code, CMOVAE_RAX_RDI());
					OPCODE(code, SHL_RSI());
				}
				else
				{
					// Just return the load time delay
					OPCODE(code, MOV_RAX_RDI());
				}
			}
			// RSI contains ret value, never modified during execution
			OPCODE(code, OR_RSI_RAX());
			asks += 1;
		}
		else
		{
			OPCODE(code, SERIALIZE());
		}

		PRINT ("[debug] load %p (%u):\ts1=%d\ts2=%d\ts3=%d\th=%d\n",
				tmp, b->block.id, tmp->set1, tmp->set2, tmp->set3, tmp->slice);

		// Evict block from L1 and LFB
		if (tmp->evict1_sz > 0)
		{
			for (i = 0, w1 = 0; i < tmp->evict1_sz && w1 < EVICTION_SZ(L1); i++)
			{
				sub = tmp->evict1[i];
				if ((banned_l3_for_l1[((sub->set3 << L3_SLICE_BITS) | sub->slice)] == 0)
					&& (banned_l2_for_l1[sub->set2] == 0) && !IS_LEADER_SET(sub->set3,sub->slice))
				{
					OPCODE(code, LOAD_RAX((unsigned long long)(sub)));
					OPCODE(code, SERIALIZE());
					WPRINT("\teset1: %p\ts1=%d\ts2=%d\ts3=%d\th=%d\n", sub, sub->set1, sub->set2, sub->set3, sub->slice);
					w1 += 1;
				}
			}
			if (tmp->evict2_sz < 1) // only if in L2
			{
				// In theory fences solve this, but keep it for now
				for (; i < tmp->evict1_sz && w1 < EVICTION_SZ(L1) + LFB; i++)
				{
					sub = tmp->evict1[i];
					if ((banned_l3_for_l1[((sub->set3 << L3_SLICE_BITS) | sub->slice)] == 0)
						&& (banned_l2_for_l1[sub->set2] == 0))
					{
						OPCODE(code, MOV_RAX_CT((unsigned long long)sub)); // move pointer to rax
						OPCODE (code, MOVNTDQA_RAX()); // movntqda xmm1, [rax]
						WPRINT("\tlfb: %p\ts1=%d\ts2=%d\ts3=%d\th=%d\n", sub, sub->set1, sub->set2, sub->set3, sub->slice);
						w1 += 1;
					}
				}
				OPCODE(code, SERIALIZE());
			}
			if (i == tmp->evict1_sz)
			{
				printk (KERN_INFO "[info] Needs more blocks to evict L1D + LFB\n");
			}
		}

		// Evict block from L2 and SQ (must be same slice)
		if (tmp->evict2_sz > 0)
		{
			for (i = 0, w2 = 0; i < tmp->evict2_sz && w2 < EVICTION_SZ(L2) * 2; i++)
			{
				sub = tmp->evict2[i];
				// L2 policy is complex in some CPUs, we double access to improve eviction
				if ((banned_l3_for_l2[((sub->set3 << L3_SLICE_BITS) | sub->slice)] == 0) && (!IS_LEADER_SET(sub->set3, sub->slice) || IS_LEADER_SET(tmp->set3, tmp->slice)))
				{
					OPCODE(code, LOAD_RAX((unsigned long long)(sub)));
					OPCODE(code, SERIALIZE());
					OPCODE(code, LOAD_RAX((unsigned long long)(sub)));
					OPCODE(code, SERIALIZE());
					WPRINT("\teset2: %p\ts1=%d\ts2=%d\ts3=%d\th=%d\n", sub, sub->set1, sub->set2, sub->set3, sub->slice);
					w2 += 1;
				}
			}
			// In theory fences solve this, but keep it for now
			for (; i < tmp->evict2_sz && w2 < EVICTION_SZ(L2) + SQ; i++)
			{
				sub = tmp->evict2[i];
				if (banned_l3_for_l2[((sub->set3 << L3_SLICE_BITS) | sub->slice)] == 0)
				{
					//OPCODE(code, LOAD_RAX((unsigned long long)(sub)));
					OPCODE(code, MOV_RAX_CT((unsigned long long)sub)); // move pointer to rax
					OPCODE (code, MOVNTDQA_RAX()); // movntqda xmm1, [rax]
					WPRINT("\tsq: %p\ts1=%d\ts2=%d\ts3=%d\th=%d\n", sub, sub->set1, sub->set2, sub->set3, sub->slice);
					w2 += 1;
				}
			}
			OPCODE(code, SERIALIZE());
			if (i == tmp->evict2_sz)
			{
				printk (KERN_INFO "[info] Need more blocks to evict L2 + SQ\n");
			}
		}
		PRINT ("[debug] ev1=%d\tev2=%d\n", w1, w2);

		// Thrash leader set
		if (get_thrash_set(conf) > -1 && tmp->evict2_sz > 0)
		{
			others = sets[get_thrash_set(conf)]; // should check bounds
			for (i = 0; i < get_thrash_size(conf) && others != NULL; i++)
			{
				OPCODE(code, LOAD_RAX((unsigned long long)others));
				OPCODE(code, SERIALIZE());
				WPRINT("\tthrashing: %p\ts1=%d\ts2=%d\ts3=%d\th=%d\n", others, others->set1, others->set2, others->set3, others->slice);
				others = others->next;
			}
			PRINT("[debug] thrash set %d with %d addresses...\n", get_thrash_set(conf), get_thrash_size(conf));
		}

	cont:
		b = b->next;
	}
	// epilogue
	OPCODE(code, MOV_RAX_RSI()); // ret value
	OPCODE(code, POP_RBX());
	OPCODE(code, POP_RBP());
	OPCODE(code, RETQ());

	PRINT ("[debug] code length=%zu bytes\n", code->len);
	PRINT("[debug] ---------------------------------\n");

	code->asks = asks;
	return code->len;

	beach:
		free_code_pages (code);
	err:
		PRINT ("[debug] err: code length=%zu bytes\n", code->len);
		return -1;
}


