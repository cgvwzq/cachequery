#ifndef __CONFIG_H
#define __CONFIG_H

#include <linux/kobject.h>

// Definitions
typedef struct config Config;

enum Debug {DISABLED = 1, ENABLED = 2, VERBOSE = 3};
enum Serialization { LFENCE = 1, MFENCE = 2, CPUID = 3};

struct conf_property {
    const char *name;
    ssize_t (*cb_read)(Config*,char*);
    void (*cb_write)(Config*,char*,ssize_t);
} typedef t_property;

struct config
{
        t_property *properties;
        unsigned int length;

        /* maximum time considered for a memory access (remove outliers) */
        unsigned int max_access_time;

        /* number of repetitions during calibration */
        unsigned int num_calibrations;

        /* number of repetitions per query */
        unsigned int num_repetitions;

        /* preload TLB entry */
        unsigned char tlb_preload;

        /* cause thrashing on set (-1 for disabled) */
        int thrash_set;

        /* number of addresses used for thrashing */
        unsigned int thrash_size;

        /* if true print in syslog the time distribution of last access */
        unsigned char only_one_time;

        /* if true use cache related PMC instead of time */
        unsigned char use_pmc;

        /* if true use PMC CTR1 instead of RDTSC */
        unsigned char core_cycles;

        /* DISABLED, ENABLED, VERBOSE */
        enum Debug debug;

        /* LFENCE, MFENCE, CPUID */
        enum Serialization serialization;

        /* time thresholds */
        unsigned int l3_hit_threshold;
        unsigned int l3_miss_threshold;
        unsigned int l2_hit_threshold;
        unsigned int l2_miss_threshold;
        unsigned int l1_hit_threshold;
        unsigned int l1_miss_threshold;

};

/* Initialize config with defualt values given in settings.h */
void init_config(Config *conf);

/* Wrappers for sysfs */
ssize_t conf_show_property (Config *conf, unsigned int index, char *buf);
ssize_t conf_store_property (Config *conf, unsigned int index, char *buf, ssize_t count);

/* Getters and setters */
unsigned int get_max_access_time(Config *conf);
void set_max_access_time(Config *conf, unsigned int max_access_time);

unsigned int get_num_calibrations(Config *conf);
void set_num_calibrations(Config *conf, unsigned int num_calibrations);

unsigned int get_num_repetitions(Config *conf);
void set_num_repetitions(Config *conf, unsigned int num_repetitions);

unsigned char get_tlb_preload(Config *conf);
void set_tlb_preload(Config *conf, unsigned char tlb_preload);

int get_thrash_set(Config *conf);
void set_thrash_set(Config *conf, int thrash_set);

unsigned int get_thrash_size(Config *conf);
void set_thrash_size(Config *conf, unsigned int thrash_size);

unsigned char get_only_one_time(Config *conf);
void set_only_one_time(Config *conf, unsigned char only_one_time);

unsigned char get_use_pmc(Config *conf);
void set_use_pmc(Config *conf, unsigned char use_pmc);

unsigned char get_core_cycles(Config *conf);
void set_core_cycles(Config *conf, unsigned char core_cycles);

enum Debug get_debug(Config *conf);
void set_debug(Config *conf, enum Debug debug);

enum Serialization get_serialization(Config *conf);
void set_serialization(Config *conf, enum Serialization serialization);

unsigned int get_l3_hit_threshold(Config *conf);
void set_l3_hit_threshold(Config *conf, unsigned int t);

unsigned int get_l3_miss_threshold(Config *conf);
void set_l3_miss_threshold(Config *conf, unsigned int t);

unsigned int get_l2_hit_threshold(Config *conf);
void set_l2_hit_threshold(Config *conf, unsigned int t);

unsigned int get_l2_miss_threshold(Config *conf);
void set_l2_miss_threshold(Config *conf, unsigned int t);

unsigned int get_l1_hit_threshold(Config *conf);
void set_l1_hit_threshold(Config *conf, unsigned int t);

unsigned int get_l1_miss_threshold(Config *conf);
void set_l1_miss_threshold(Config *conf, unsigned int t);

#endif /* __CONFIG_H */
