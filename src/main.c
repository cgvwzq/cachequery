#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/page.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>

#include "../include/msrdrv.h"
#include "../include/parser.h"
#include "../include/cache.h"
#include "../include/config.h"
#include "../include/histogram.h"
#include "../include/lists.h"
#include "../include/cachequery.h"
#include "../include/x86.h"
#include "../config/settings.h"

// Static global vars
static Config conf;

static char *pool_l3 = NULL;
static char *pool_l2 = NULL;
static char *pool_l1 = NULL;

static int cal[MAX_TIME];
static int hist[MAX_TIME];

// Main Kobjects
static struct kobject *cachequery_kobj;
static struct cacheset_obj *cacheset_l3_objs[L3_CACHE_SETS];
static struct cacheset_obj *cacheset_l2_objs[L2_CACHE_SETS];
static struct cacheset_obj *cacheset_l1_objs[L1_CACHE_SETS];
static struct config_obj **config_objs;

static struct kset *l3cachesets_kset;
static struct kset *l2cachesets_kset;
static struct kset *l1cachesets_kset;
static struct kset *config_kset;

// START OF SYSFS MESS
// this  needs to be rewritten, right now it's a mix of legacy copy-paste stuff that makes little sense
struct cacheset_obj {
	struct kobject kobj;
	unsigned int index;
	unsigned char level;
	lexer_state lexer;
	struct smart_buffer code;
	char name[20];
};
#define to_cacheset_obj(x) container_of(x, struct cacheset_obj, kobj)
struct config_obj {
	struct kobject kobj;
	unsigned int index;
	char name[20];
};
#define to_config_obj(x) container_of(x, struct config_obj, kobj)

struct cacheset_attribute {
	struct attribute attr;
	ssize_t (*show)(struct cacheset_obj *cacheset, struct cacheset_attribute *attr, char *buf);
	ssize_t (*store)(struct cacheset_obj *cacheset, struct cacheset_attribute *attr, char *buf, size_t count);
};
#define to_cacheset_attr(x) container_of(x, struct cacheset_attribute, attr)

struct config_attribute {
	struct attribute attr;
	ssize_t (*show)(struct config_obj *config, struct config_attribute *attr, char *buf);
	ssize_t (*store)(struct config_obj *config, struct config_attribute *attr, char *buf, size_t count);
};
#define to_config_attr(x) container_of(x, struct config_attribute, attr)

static ssize_t cacheset_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct cacheset_attribute *attribute;
	struct cacheset_obj *x;

	attribute = to_cacheset_attr(attr);
	x = to_cacheset_obj(kobj);

	if (!attribute->show)
		return -EIO;

	return attribute->show(x, attribute, buf);
}

static ssize_t cacheset_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t len)
{
	struct cacheset_attribute *attribute;
	struct cacheset_obj *x;

	attribute = to_cacheset_attr(attr);
	x = to_cacheset_obj(kobj);

	if (!attribute->store)
		return -EIO;

	return attribute->store(x, attribute, (char*)buf, len);
}

static ssize_t config_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct config_attribute *attribute;
	struct config_obj *x;

	attribute = to_config_attr(attr);
	x = to_config_obj(kobj);

	if (!attribute->show)
		return -EIO;

	return attribute->show(x, attribute, buf);
}

static ssize_t config_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t len)
{
	struct config_attribute *attribute;
	struct config_obj *x;

	attribute = to_config_attr(attr);
	x = to_config_obj(kobj);

	if (!attribute->store)
		return -EIO;

	return attribute->store(x, attribute, (char*)buf, len);
}

static const struct sysfs_ops config_sysfs_ops = {
	.show = config_attr_show,
	.store = config_attr_store,
};

static const struct sysfs_ops cacheset_sysfs_ops = {
	.show = cacheset_attr_show,
	.store = cacheset_attr_store,
};

static void cacheset_release(struct kobject *kobj)
{
	struct cacheset_obj *cacheset;
	cacheset = to_cacheset_obj (kobj);
}

static void config_release(struct kobject *kobj)
{
	struct config_obj *config;
	config = to_config_obj (kobj);
}

// Declare functions
ssize_t val_show(struct cacheset_obj *kobj, char *out, Block **sets, size_t set_length, size_t evict_size, int t_hit, int t_miss, unsigned char level);
int calibrate (Block **set, int nsets, int ways, unsigned char measure_miss);

static ssize_t cacheset_show(struct cacheset_obj *kobj, struct cacheset_attribute *attr, char *buf)
{
	if (kobj->level == 3)
	{
		return val_show (kobj, buf, get_sets_l3(), L3_CACHE_SETS, 2*L3_CACHE_WAYS, get_l3_hit_threshold(&conf), get_l3_miss_threshold(&conf), 3);
	}
	else if (kobj->level == 2)
	{
		return val_show (kobj, buf, get_sets_l2(), L2_CACHE_SETS, EVICTION_SZ(L2), get_l2_hit_threshold(&conf), get_l2_miss_threshold(&conf), 2);
	}
	else if (kobj->level == 1)
	{
		return val_show (kobj, buf, get_sets_l1(), L1_CACHE_SETS, EVICTION_SZ(L1), get_l1_hit_threshold(&conf), get_l1_miss_threshold(&conf), 1);
	}
	else
	{
		return 0;
	}
}

static ssize_t code_show(struct cacheset_obj *kobj, struct cacheset_attribute *attr, char *buf)
{
	ssize_t len = MIN(kobj->code.len , PAGE_SIZE);
	// hope everything is consistent here
	if (kobj->code.start != NULL && kobj->code.len > 0)
	{
		memcpy (buf, kobj->code.start, len);
	}
	return len;
}

static ssize_t cacheset_store(struct cacheset_obj *cacheset_obj, struct cacheset_attribute *attr, char *buf, size_t count)
{
	if (cacheset_obj->level == 3)
	{
		parse (&cacheset_obj->lexer, buf, count, L3_CACHE_SETS);
	}
	else if (cacheset_obj->level == 2)
	{
		parse (&cacheset_obj->lexer, buf, count, L2_CACHE_SETS);
	}
	else if (cacheset_obj->level == 1)
	{
		parse (&cacheset_obj->lexer, buf, count, L1_CACHE_SETS);
	}
	else
	{
		return 0;
	}
	// free code to regenerate for use new sequence
	free_code_pages (&cacheset_obj->code);
	return count;
}

static ssize_t config_show(struct config_obj *config_obj, struct config_attribute *attr, char *buf)
{
	return conf_show_property (&conf, config_obj->index, buf);
}

static ssize_t config_store(struct config_obj *config_obj, struct config_attribute *attr, char *buf, size_t count)
{
	conf_store_property (&conf, config_obj->index, buf, count);
	return count;
}

static struct cacheset_attribute cacheset_attribute = __ATTR(run, 0664, cacheset_show, cacheset_store);
static struct cacheset_attribute code_attribute = __ATTR(code, 0664, code_show, NULL);
static struct config_attribute conf_attribute = __ATTR(value, 0664, config_show, config_store);

// Group of attributes
static struct attribute *attrs[] = {
	NULL,
};

// Specify name creates a directory under kernel
static struct attribute_group attr_group = {
	.attrs = attrs,
};

// Attributes for genereal cacheset_kobjs
static struct attribute *cacheset_default_attrs[] = {
	&cacheset_attribute.attr,
	&code_attribute.attr,
	NULL,
};

// Attributes for genereal config_kobjs
static struct attribute *config_default_attrs[] = {
	&conf_attribute.attr,
	NULL,
};

static struct kobj_type cacheset_ktype = {
	.sysfs_ops = &cacheset_sysfs_ops,
	.release = cacheset_release,
	.default_attrs = cacheset_default_attrs,
};

static struct kobj_type config_ktype = {
	.sysfs_ops = &config_sysfs_ops,
	.release = config_release,
	.default_attrs = config_default_attrs,
};

static struct cacheset_obj *create_cacheset_obj(unsigned int index, unsigned char level, struct kset *kset)
{
	struct cacheset_obj *x;
	int retval;

	x = kzalloc(sizeof(*x), GFP_KERNEL);
	if (!x)
		return NULL;

	x->kobj.kset = kset;
	x->index = index;
	x->level = level;
	lexer_init (&x->lexer);
	smart_buffer_init (&x->code);
	snprintf (x->name, 20, "%d", index);

	retval = kobject_init_and_add (&x->kobj, &cacheset_ktype, NULL, "%s", x->name);
	if (retval)
	{
		kobject_put(&x->kobj);
		return NULL;
	}

	kobject_uevent(&x->kobj, KOBJ_ADD);
	return x;
}

static struct config_obj *create_config_obj(unsigned int index, t_property *prop, struct kset *kset)
{
	struct config_obj *x;
	int retval;

	x = kzalloc(sizeof(*x), GFP_KERNEL);
	if (!x)
		return NULL;

	x->kobj.kset = kset;
	x->index = index;
	snprintf (x->name, 20, "%s", prop->name);

	retval = kobject_init_and_add (&x->kobj, &config_ktype, NULL, "%s", x->name);
	if (retval)
	{
		kobject_put(&x->kobj);
		return NULL;
	}

	kobject_uevent(&x->kobj, KOBJ_ADD);
	return x;
}

static void destroy_config_obj(struct config_obj *config)
{
	kobject_put(&config->kobj);
}

static void destroy_cacheset_obj(struct cacheset_obj *cacheset)
{
	free_code_pages (&cacheset->code);
	kobject_put(&cacheset->kobj);
}


// Rest of function definitions
int calibrate (Block **set, int nsets, int ways, unsigned char measure_miss)
{
	size_t i, s, c;
	int t_val, diff, kk;
	int w1, w2;
	Block *evict, **evict1, **evict2, *tmp;
	struct smart_buffer code;
	unsigned long long (*run)(void) = NULL, ret = 0;
	unsigned long flags;

	memset (cal, 0, sizeof(cal));

	// Calibrate with set 0
	s = 0;
	// Lazy compute L2 eviction set
	if (nsets == L3_CACHE_SETS && set[s]->evict2 == NULL)
	{
		tmp = set[s];
		while (tmp)
		{
			find_l2_eviction_set (tmp, get_sets_l2());
			tmp = tmp->next;
		}
	}
	// Allocate not conflicting pages
	if (!allocate_code_pages (&code, set, nsets, s, JIT_CODE_SZ))
	{
		goto err;
	}

	evict1 = set[s]->evict1;
	evict2 = set[s]->evict2;
	evict = set[s]->next;

	// preamble
	OPCODE(&code, PUSH_RBP());
	OPCODE(&code, MOV_RBP_RSP());
	OPCODE(&code, PUSH_RBX());

	// flush block
	OPCODE(&code, MOV_RAX_CT((unsigned long long)(set[s])));
	OPCODE(&code, CLFLUSH_RAX());
	OPCODE(&code, CPUID());

	// access block
	OPCODE(&code, SERIALIZE());
	OPCODE(&code, LOAD_RAX((unsigned long long)(set[s])));
	OPCODE(&code, SERIALIZE());

	PRINT ("[debug] load %p:\ts1=%d\ts2=%d\ts3=%d\th=%d\n",
			set[s], set[s]->set1, set[s]->set2, set[s]->set3, set[s]->slice);

	// evict block from L1 and LFB
	if (set[s]->evict1_sz > 0)
	{
		for (i = 0, w1 = 0; i < set[s]->evict1_sz && w1 < EVICTION_SZ(L1); i++)
		{
			if (((evict1[i]->set3 != set[s]->set3) || (evict1[i]->slice != set[s]->slice))
				&& (evict1[i]->set2 != set[s]->set2))
			{
				OPCODE(&code, LOAD_RAX((unsigned long long)(evict1[i])));
				OPCODE(&code, SERIALIZE());
				WPRINT("\teset1: %p\ts1=%d\ts2=%d\ts3=%d\th=%d\n", evict1[i], evict1[i]->set1, evict1[i]->set2, evict1[i]->set3, evict1[i]->slice);
				w1 += 1;
			}
		}
		if (set[s]->evict2_sz < 1) // only if in L2
		{
			for (w1 = 0; i < set[s]->evict1_sz && w1 < LFB; i++)
			{
				if (((evict1[i]->set3 != set[s]->set3) || (evict1[i]->slice != set[s]->slice))
					&& (evict1[i]->set2 != set[s]->set2))
				{
//					OPCODE(code, LOAD_RAX((unsigned long long)(evict1[i])));
					OPCODE(&code, MOV_RAX_CT((unsigned long long)(evict1[i]))); // move pointer to rax
					OPCODE (&code, MOVNTDQA_RAX()); // movntqda xmm1, [rax]
					WPRINT("\tlfb: %p\ts1=%d\ts2=%d\ts3=%d\th=%d\n", evict1[i], evict1[i]->set1, evict1[i]->set2, evict1[i]->set3, evict1[i]->slice);
					w1 += 1;
				}

			}
			OPCODE(&code, SERIALIZE());
		}
	}

	// evict block from L2 and SQ
	if (set[s]->evict2_sz > 0)
	{
		for (i = 0, w2 = 0; i < set[s]->evict2_sz && w2 < EVICTION_SZ(L2); i++)
		{
			if ((evict2[i]->set3 != set[s]->set3) || (evict2[i]->slice != set[s]->slice))
			{
				OPCODE(&code, LOAD_RAX((unsigned long long)(evict2[i])));
				OPCODE(&code, SERIALIZE());
				WPRINT("\teset2: %p\ts1=%d\ts2=%d\ts3=%d\th=%d\n", evict2[i], evict2[i]->set1, evict2[i]->set2, evict2[i]->set3, evict2[i]->slice);
				w2 += 1;
			}
		}
		for (w2 = 0; i < set[s]->evict2_sz && w2 < SQ; i++)
		{
			if ((evict2[i]->set3 != set[s]->set3) || (evict2[i]->slice != set[s]->slice))
			{
//				OPCODE(code, LOAD_RAX((unsigned long long)(evict2[i])));
				OPCODE(&code, MOV_RAX_CT((unsigned long long)(evict2[i]))); // move pointer to rax
				OPCODE (&code, MOVNTDQA_RAX()); // movntqda xmm1, [rax]
				WPRINT("\tsq: %p\ts1=%d\ts2=%d\ts3=%d\th=%d\n", evict2[i], evict2[i]->set1, evict2[i]->set2, evict2[i]->set3, evict2[i]->slice);
				w2 += 1;
			}
		}
		OPCODE(&code, SERIALIZE());
	}

	if (measure_miss)
	{
		// evict current cache level
		c = 0;
		while (evict && c < ways)
		{
			if ((nsets == L1_CACHE_SETS && ((evict->set2 != set[s]->set2) && (evict->set3 != set[s]->set3 || evict->slice != set[s]->slice)))
				|| ((nsets == L2_CACHE_SETS) && (evict->set3 != set[s]->set3 || evict->slice != set[s]->slice))
				|| (nsets == L3_CACHE_SETS))
			{
				OPCODE(&code, LOAD_RAX((unsigned long long)(evict)));
				OPCODE(&code, SERIALIZE());
				WPRINT("\teset: %p\ts1=%d\ts2=%d\ts3=%d\th=%d\n", evict, evict->set1, evict->set2, evict->set3, evict->slice);
				c += 1;
			}
			evict = evict->next;
		}
	}

	// refresh address TLB if L2 or L3
	if (get_tlb_preload(&conf) && set[s]->evict1_sz > 0)
	{
		unsigned long long tlb = ((unsigned long long)set[s]);
		tlb = ((tlb >> 12) << 12) | ((tlb & 0xfff) ^ 0x7e0); // different set index-bits
		WPRINT("[debug] tlb refresh %llx s1=%d s2=%d s3=%d h=%d\n",
			tlb, get_l1_set((void*)tlb), get_l2_set((void*)tlb), get_l3_set((void*)tlb), get_l3_slice((void*)tlb));
		OPCODE(&code, LOAD_RAX(tlb));
		OPCODE(&code, SERIALIZE());
	}

	if (get_core_cycles(&conf))
	{
		RESET_PMC0(&code);

	}
	else
	{
		MEASURE_PRE_TSC(&code);
	}

	// Access block
	OPCODE(&code, LOAD_RAX((unsigned long long)(set[s])));

	if (get_core_cycles(&conf))
	{
		MEASURE_POST_CORE(&code);
	}
	else
	{
		MEASURE_POST_TSC(&code);
	}

	// ret diff (rax)
	OPCODE(&code, MOV_RAX_RDI());
	OPCODE(&code, POP_RBX());
	OPCODE(&code, POP_RBP());
	OPCODE(&code, RETQ());

	// warm
	run = (unsigned long long(*)(void))(code.start);
	run ();

	for (i=0, kk=0; i<get_num_calibrations(&conf); i++)
	{
		// EXEC CODE
		// Prepare performance counters
		if (get_core_cycles(&conf))
		{
			prepare_counters (4); // > 3 for core cycles instead of cache miss
		}
		preempt_disable ();
		raw_local_irq_save (flags);
		//
		ret = run ();
		//
		raw_local_irq_restore (flags);
		preempt_enable ();
		// Disable counters
		if (get_core_cycles(&conf))
		{
			disable_counters ();
		}
		//EOC
		diff = (int)ret;
		if (diff < 0)
		{
			diff = 0;
		}
		if (diff < get_max_access_time(&conf) && i > 10)
		{
			cal[diff]++;
			kk++;
		}
	}
	print_hist (cal);
	t_val = get_min (cal);

	free_code_pages (&code);
	return t_val;

	err:
		printk ("err: calibration\n");
		return -1;
}

ssize_t val_show(struct cacheset_obj *kobj, char *out, Block **sets, size_t set_length, size_t evict_size, int t_hit, int t_miss, unsigned char level)
{
	size_t trace[128];
	size_t i = 0, rep, fails, ret;
	unsigned long flags;
	unsigned long long (*run)(void) = NULL, retmask = 0;
	unsigned long long bit;
	struct smart_buffer *code = &kobj->code;
	unsigned int num_repetitions = get_num_repetitions(&conf);

	if (!get_use_pmc(&conf))
	{
		// Calibration
		if (t_hit < 1)
		{
			PRINT ("Start hit calibration\n");
			t_hit = calibrate (sets, set_length, evict_size, 0);
			PRINT ("Calibrated hit: %d\n", t_hit);
		}
		if (t_miss < 1)
		{
			PRINT ("Start miss calibration\n");
			t_miss = calibrate (sets, set_length, evict_size, 1);
			PRINT ("Calibrated miss: %d\n", t_miss);
		}
	}

	// We reuse the code until there's a new sequence (write triggers parse())
	// TODO: add check for change on config
	if (code->start == NULL)
	{
		if (generate_code (&kobj->lexer, kobj->index, code, sets, level, t_miss, t_hit, &conf) < 0)
		{
			printk (KERN_INFO "Err: Unitialized code\n");
			return 0;
		}
	}

	// Fix ptr to generated code
	run = (unsigned long long(*)(void))code->start;

	// Reset counters
	memset (trace, 0, sizeof(trace));
	memset (hist, 0, sizeof(hist));

	// warmup code
	run();

	for (rep = 0, fails = 0; rep < num_repetitions && fails < num_repetitions; rep++)
	{
		// EXEC TEST CODE
		// Prepare performance counters
		if (get_use_pmc(&conf))
		{
			prepare_counters (level);
		}
		else if (get_core_cycles(&conf))
		{
			prepare_counters (4);

		}
		preempt_disable ();
		raw_local_irq_save (flags);
		//
		retmask = run();
		//
		raw_local_irq_restore (flags);
		preempt_enable ();
		// Disable counters
		if (get_use_pmc(&conf) || get_core_cycles(&conf))
		{
			disable_counters ();
		}
		// EOC
		if (retmask == 0xffffffffffffffff)
		{
			printk (KERN_INFO "[err] code exec exception\n");
			ret = sprintf (out, "Runtime exception!\n");
			goto err;
			// repeat instead of abort
			rep--;
			fails++;
		}
		if (get_only_one_time(&conf))
		{
			/* safe distribution of timing measurements for last block */
			if (retmask < MAX_TIME)
			{
				hist[retmask] += 1;
			}
			else
			{
				PRINT ("MAX_TIME exceeded\n");
			}
		}
		else if (code->asks > 0)
		{
			i = 0;
			bit = 1ULL << (code->asks - 1);
			while (bit != 0ULL)
			{
				// count hit
				if ((retmask & bit) == 0ULL)
				{
					trace[i] += 1;
				}
				bit >>= 1;
				i += 1;
			}
		}
		// yield (); // force preemption
	}

	if (fails >= num_repetitions)
	{
		printk (KERN_INFO "Error: exceeded fails max\n");
		ret = 0;
		goto err;
	}

	// return length of string
	ret = 0;
	if (get_only_one_time(&conf))
	{
		PRINT ("[time] Total reps: %lu (%lu fails)\n", rep, fails);
		print_hist (hist);
		PRINT ("[time] Mean: %d\n", get_mean(hist, i-1));
		PRINT ("[time] Mode: %d\n", get_mode(hist));
		PRINT ("[time] Below MISS threshold: %d\n", get_n_below(hist, t_miss));
	}
	else
	{
		// buffer is of size PAGE_SIZE
		for (i = 0; i < code->asks; i++)
		{
			ret += sprintf (&out[ret], "%ld ", trace[i]);
		}
		ret += sprintf (&out[ret], "\n");
	}

	err:
	return ret;

}

static int __init cachequery_init(void)
{
	int ret;
	unsigned int i, j;

	// Create and add object
	cachequery_kobj = kobject_create_and_add ("cachequery", kernel_kobj);
	if (!cachequery_kobj)
	{
		return -ENOMEM;
	}

	// Create files for group
	ret = sysfs_create_group (cachequery_kobj, &attr_group);
	if (ret)
	{
		goto err;
	}

	// Allocate pools
	if (!(pool_l3 = __vmalloc (L3_POOL_SZ, GFP_KERNEL|__GFP_HIGHMEM|__GFP_COMP, PAGE_KERNEL_LARGE)))
	{
		ret = -ENOMEM;
		goto err;
	}
	if (!(pool_l2 = __vmalloc (L2_POOL_SZ, GFP_KERNEL|__GFP_HIGHMEM|__GFP_COMP, PAGE_KERNEL_LARGE)))
	{
		ret = -ENOMEM;
		goto clean2;
	}
	if (!(pool_l1 = __vmalloc (L1_POOL_SZ, GFP_KERNEL|__GFP_HIGHMEM|__GFP_COMP, PAGE_KERNEL_LARGE)))
	{
		ret = -ENOMEM;
		goto clean1;
	}

	// Init and shuffle lists of congruent addresses
	init_lists(pool_l3, pool_l2, pool_l1);

	// Start computing eviction sets
	init_evictionsets();

	// Create structures
	l3cachesets_kset = kset_create_and_add("l3_sets", NULL, cachequery_kobj);
	if (!l3cachesets_kset)
		goto clean;
	l2cachesets_kset = kset_create_and_add("l2_sets", NULL, cachequery_kobj);
	if (!l2cachesets_kset)
		goto unreg3;
	l1cachesets_kset = kset_create_and_add("l1_sets", NULL, cachequery_kobj);
	if (!l1cachesets_kset)
		goto unreg2;
	config_kset = kset_create_and_add("config", NULL, cachequery_kobj);
	if (!config_kset)
		goto unreg1;

	for (i = 0; i < L3_CACHE_SETS; i++)
	{
		// add error check and clean
		cacheset_l3_objs[i] = create_cacheset_obj(i, 3, l3cachesets_kset);
		if (!cacheset_l3_objs[i])
			goto obj3;
	}
	for (i = 0; i < L2_CACHE_SETS; i++)
	{
		// add error check and clean
		cacheset_l2_objs[i] = create_cacheset_obj(i, 2, l2cachesets_kset);
		if (!cacheset_l2_objs[i])
			goto obj2;
	}
	for (i = 0; i < L1_CACHE_SETS; i++)
	{
		// add error check and clean
		cacheset_l1_objs[i] = create_cacheset_obj(i, 1, l1cachesets_kset);
		if (!cacheset_l1_objs[i])
			goto obj1;
	}

	// Initialize config
	printk (KERN_INFO "Creating config...\n");
	init_config (&conf);
	if (!(config_objs = vmalloc (sizeof(struct config_obj*)*conf.length)))
	{
		ret = -ENOMEM;
		goto obj1;
	}
	for (i = 0; i < conf.length; i++)
	{
		// add error check and clean
		config_objs[i] = create_config_obj(i, &conf.properties[i], config_kset);
		if (!config_objs[i])
			goto conf;
	}

	// Success
	printk (KERN_INFO "Loaded cachequery.ko\n");
	return 0;

	// Beach
	conf:
		for (j =0; j < i; j++)
			destroy_config_obj (config_objs[j]);
		i = L1_CACHE_SETS;
	obj1:
		for (j = 0; j < i; j++)
			destroy_cacheset_obj (cacheset_l1_objs[j]);
		i = L2_CACHE_SETS;
	obj2:
		for (j = 0; j < i; j++)
			destroy_cacheset_obj (cacheset_l2_objs[j]);
		i = L3_CACHE_SETS;
	obj3:
		for (j = 0; j < i; j++)
			destroy_cacheset_obj (cacheset_l3_objs[j]);

		kset_unregister (config_kset);
	unreg1:
		kset_unregister (l1cachesets_kset);
	unreg2:
		kset_unregister (l2cachesets_kset);
	unreg3:
		kset_unregister (l3cachesets_kset);
	clean:
		for (i = 0; i < L3_CACHE_SETS; i++)
		{
			clean_l3_set (i);
		}
		for (i = 0; i < L2_CACHE_SETS; i++)
		{
			clean_l2_set (i);
		}
		vfree (config_objs);
		vfree (pool_l1);
	clean1:
		vfree (pool_l2);
	clean2:
		vfree (pool_l3);
	err:
		kobject_put (cachequery_kobj);
		return ret;
}

static void __exit cachequery_exit(void)
{
	unsigned int i;
	for (i = 0; i < L2_CACHE_SETS; i++)
	{
		clean_l2_set (i);
	}
	for (i = 0; i < L3_CACHE_SETS; i++)
	{
		clean_l3_set (i);
	}
	vfree (pool_l3);
	vfree (pool_l2);
	vfree (pool_l1);
	for (i = 0; i < L3_CACHE_SETS; i++)
		destroy_cacheset_obj (cacheset_l3_objs[i]);
	for (i = 0; i < L2_CACHE_SETS; i++)
		destroy_cacheset_obj (cacheset_l2_objs[i]);
	for (i = 0; i < L1_CACHE_SETS; i++)
		destroy_cacheset_obj (cacheset_l1_objs[i]);
	for (i =0; i < conf.length; i++)
		destroy_config_obj (config_objs[i]);
	kset_unregister (l3cachesets_kset);
	kset_unregister (l2cachesets_kset);
	kset_unregister (l1cachesets_kset);
	kset_unregister (config_kset);
	kobject_put (cachequery_kobj);
	printk (KERN_INFO "Unlodaded cachequery.ko\n");
}

module_init(cachequery_init);
module_exit(cachequery_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pepe Vila - @cgvwzq");
MODULE_DESCRIPTION("CacheQuery's Kernel module");
