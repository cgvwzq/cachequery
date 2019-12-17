#include "../include/config.h"

#include <linux/mm.h>
#include <linux/kernel.h>

#include "../config/settings.h"
#include "../include/cachequery.h"

#define WRAP_GET_UINT(name) ssize_t wrap_get_##name(Config *conf, char *input) { return (ssize_t) sprintf (input, "%u\n", get_##name(conf)); }
#define WRAP_SET_UINT(name) void wrap_set_##name(Config *conf, char *input, ssize_t count) { unsigned int res; if (kstrtouint(input,10,&res) == 0) { set_##name(conf, res);} }

// TO FIX: Not working with negatives
#define WRAP_GET_INT(name) ssize_t wrap_get_##name(Config *conf, char *input) { return (ssize_t) sprintf (input, "%d\n", get_##name(conf)); }
#define WRAP_SET_INT(name) void wrap_set_##name(Config *conf, char *input, ssize_t count) { int res; if (kstrtoint(input,10,&res) == 0) { set_##name(conf, res);} }

#define WRAP_GET_BOOL(name) ssize_t wrap_get_##name(Config *conf, char *input) { return (ssize_t) sprintf (input, "%x\n", get_##name(conf)); }
#define WRAP_SET_BOOL(name) void wrap_set_##name(Config *conf, char *input, ssize_t count) { unsigned char res; if (input[0] == '0') { res = 0; } else { res = 1; } set_##name(conf, res); }

/* Wrappers to parse/return strings */
// TODO: refactor macros, still missing some for enums
WRAP_GET_UINT(max_access_time)
WRAP_SET_UINT(max_access_time);
WRAP_GET_UINT(num_calibrations)
WRAP_SET_UINT(num_calibrations);
WRAP_GET_UINT(num_repetitions);
WRAP_SET_UINT(num_repetitions);
WRAP_GET_BOOL(tlb_preload);
WRAP_SET_BOOL(tlb_preload);
WRAP_GET_INT(thrash_set);
WRAP_SET_INT(thrash_set);
WRAP_GET_UINT(thrash_size);
WRAP_SET_UINT(thrash_size);
WRAP_GET_BOOL(only_one_time);
WRAP_SET_BOOL(only_one_time);
WRAP_GET_BOOL(use_pmc);
WRAP_SET_BOOL(use_pmc);
WRAP_GET_BOOL(core_cycles);
WRAP_SET_BOOL(core_cycles);
WRAP_GET_UINT(l3_hit_threshold);
WRAP_SET_UINT(l3_hit_threshold);
WRAP_GET_UINT(l3_miss_threshold);
WRAP_SET_UINT(l3_miss_threshold);
WRAP_GET_UINT(l2_hit_threshold);
WRAP_SET_UINT(l2_hit_threshold);
WRAP_GET_UINT(l2_miss_threshold);
WRAP_SET_UINT(l2_miss_threshold);
WRAP_GET_UINT(l1_hit_threshold);
WRAP_SET_UINT(l1_hit_threshold);
WRAP_GET_UINT(l1_miss_threshold);
WRAP_SET_UINT(l1_miss_threshold);

static t_property conf_properties[] = {
    { .name = "max_access_time", .cb_read = &wrap_get_max_access_time, .cb_write = &wrap_set_max_access_time },
    { .name = "num_calibrations", .cb_read = &wrap_get_num_calibrations, .cb_write = &wrap_set_num_calibrations },
    { .name = "num_repetitions", .cb_read = &wrap_get_num_repetitions, .cb_write = &wrap_set_num_repetitions },
    { .name = "tlb_preload", .cb_read = &wrap_get_tlb_preload, .cb_write = &wrap_set_tlb_preload },
    { .name = "thrash_set", .cb_read = &wrap_get_thrash_set, .cb_write = &wrap_set_thrash_set },
    { .name = "thrash_size", .cb_read = &wrap_get_thrash_size, .cb_write = &wrap_set_thrash_size },
    { .name = "only_one_time", .cb_read = &wrap_get_only_one_time, .cb_write = &wrap_set_only_one_time},
    { .name = "use_pmc", .cb_read = &wrap_get_use_pmc, .cb_write = &wrap_set_use_pmc },
    { .name = "core_cycles", .cb_read = &wrap_get_core_cycles, .cb_write = &wrap_set_core_cycles },
    { .name = "debug", .cb_read = NULL, .cb_write = NULL },
    { .name = "serialization", .cb_read = NULL, .cb_write = NULL },
    { .name = "l3_hit_threshold", .cb_read = &wrap_get_l3_hit_threshold, .cb_write = &wrap_set_l3_hit_threshold },
    { .name = "l3_miss_threshold", .cb_read = &wrap_get_l3_miss_threshold, .cb_write = &wrap_set_l3_miss_threshold },
    { .name = "l2_hit_threshold", .cb_read = &wrap_get_l2_hit_threshold, .cb_write = &wrap_set_l2_hit_threshold },
    { .name = "l2_miss_threshold", .cb_read = &wrap_get_l2_miss_threshold, .cb_write = &wrap_set_l2_miss_threshold },
    { .name = "l1_hit_threshold", .cb_read = &wrap_get_l1_hit_threshold, .cb_write = &wrap_set_l1_hit_threshold },
    { .name = "l1_miss_threshold", .cb_read = &wrap_get_l1_miss_threshold, .cb_write = &wrap_set_l1_miss_threshold },
};

ssize_t conf_show_property (Config *conf, unsigned int index, char *buf)
{
    // write string on buf and return number of bytes written
    if (conf_properties[index].cb_read)
    {
        return conf_properties[index].cb_read(conf, buf);
    }
    return 0;
}

ssize_t conf_store_property (Config *conf, unsigned int index, char *buf, ssize_t count)
{
    // read from  buf and return bytes read
    if (conf_properties[index].cb_write)
    {
        conf_properties[index].cb_write(conf, buf, count);
    }
    return 0;
}

/* Initialize config with default values  */
void init_config(Config *conf)
{
    conf->properties = conf_properties;
    conf->length = sizeof(conf_properties)/sizeof(t_property);
    set_max_access_time(conf, MAX_TIME);
    set_num_calibrations(conf, NUM_CALIBRATIONS);
    set_num_repetitions(conf, NUM_REPETITIONS);
    set_tlb_preload(conf, TLB_PRELOAD);
    set_thrash_set(conf, THRASH_SET);
    set_thrash_size(conf, THRASHING_SIZE);
    set_only_one_time(conf, ONLY_ONE_TIME);
    set_use_pmc(conf, USE_PMC);
    set_core_cycles(conf, CORE_CYCLES);
    set_debug(conf, ENABLED); // TODO
    set_serialization(conf, MFENCE); // TODO
    set_l3_hit_threshold(conf, L3_HIT_THRESHOLD);
    set_l3_miss_threshold(conf, L3_MISS_THRESHOLD);
    set_l2_hit_threshold(conf, L2_HIT_THRESHOLD);
    set_l2_miss_threshold(conf, L2_MISS_THRESHOLD);
    set_l1_hit_threshold(conf, L1_HIT_THRESHOLD);
    set_l1_miss_threshold(conf, L1_MISS_THRESHOLD);
}

/* Getters and setters */
unsigned int get_max_access_time(Config *conf)
{
    return conf->max_access_time;
}
void set_max_access_time(Config *conf, unsigned int max_access_time)
{
    conf->max_access_time = max_access_time;
}

unsigned int get_num_calibrations(Config *conf)
{
    return conf->num_calibrations;
}
void set_num_calibrations(Config *conf, unsigned int num_calibrations)
{
    conf->num_calibrations = num_calibrations;
}

unsigned int get_num_repetitions(Config *conf)
{
    return conf->num_repetitions;
}
void set_num_repetitions(Config *conf, unsigned int num_repetitions)
{
    conf->num_repetitions = num_repetitions;
}

unsigned char get_tlb_preload(Config *conf)
{
    return conf->tlb_preload;
}
void set_tlb_preload(Config *conf, unsigned char tlb_preload)
{
    conf->tlb_preload = tlb_preload;
}

int get_thrash_set(Config *conf)
{
    return conf->thrash_set;
}
void set_thrash_set(Config *conf, int thrash_set)
{
    conf->thrash_set = thrash_set;
}

unsigned int get_thrash_size(Config *conf)
{
    return conf->thrash_size;
}
void set_thrash_size(Config *conf, unsigned int thrash_size)
{
    conf->thrash_size = thrash_size;
}

unsigned char get_only_one_time(Config *conf)
{
    return conf->only_one_time;
}
void set_only_one_time(Config *conf, unsigned char only_one_time)
{
    conf->only_one_time = only_one_time;
}

unsigned char get_use_pmc(Config *conf)
{
    return conf->use_pmc;
}
void set_use_pmc(Config *conf, unsigned char use_pmc)
{
    conf->use_pmc = use_pmc;
}

unsigned char get_core_cycles(Config *conf)
{
    return conf->core_cycles;
}
void set_core_cycles(Config *conf, unsigned char core_cycles)
{
    conf->core_cycles = core_cycles;
}

enum Debug get_debug(Config *conf)
{
    return conf->debug;
}
void set_debug(Config *conf, enum Debug debug)
{
    conf->debug = debug;
}

enum Serialization get_serialization(Config *conf)
{
    return conf->serialization;
}
void set_serialization(Config *conf, enum Serialization serialization)
{
    conf->serialization = serialization;
}

unsigned int get_l3_hit_threshold(Config *conf)
{
    return conf->l3_hit_threshold;
}
void set_l3_hit_threshold(Config *conf, unsigned int t)
{
    conf->l3_hit_threshold = t;
}

unsigned int get_l3_miss_threshold(Config *conf)
{
    return conf->l3_miss_threshold;
}
void set_l3_miss_threshold(Config *conf, unsigned int t)
{
    conf->l3_miss_threshold = t;
}

unsigned int get_l2_hit_threshold(Config *conf)
{
    return conf->l2_hit_threshold;
}
void set_l2_hit_threshold(Config *conf, unsigned int t)
{
    conf->l2_hit_threshold = t;
}

unsigned int get_l2_miss_threshold(Config *conf)
{
    return conf->l2_miss_threshold;
}
void set_l2_miss_threshold(Config *conf, unsigned int t)
{
    conf->l2_miss_threshold = t;
}

unsigned int get_l1_hit_threshold(Config *conf)
{
    return conf->l1_hit_threshold;
}
void set_l1_hit_threshold(Config *conf, unsigned int t)
{
    conf->l1_hit_threshold = t;
}

unsigned int get_l1_miss_threshold(Config *conf)
{
    return conf->l1_miss_threshold;
}
void set_l1_miss_threshold(Config *conf, unsigned int t)
{
    conf->l1_miss_threshold = t;
}
