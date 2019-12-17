#ifndef __SETTINGS_H
#define __SETTINGS_H

/* SELECT ARCHITECTURE */
#define STR(s) STR2(s)
#define STR2(s) #s
#define EXPAND(s) s

#include STR(EXPAND(CPU).h)
/* END OF SELECTION */

/* DEFAULT RUNTIME OPTIONS */
/* We ignore memory access taking longer than MAX_TIME */
#define MAX_TIME 10000
/* Number of repetitions used during calibration phase */
#define NUM_CALIBRATIONS 10000
/* Number of repetitions for distinguishing HIT/MISS */
#define NUM_REPETITIONS 100
/* Force a TLB preload for the address to profile */
#define TLB_PRELOAD TRUE
/* Choose a cache set to thrash before any memory access in the query */
#define THRASH_SET -1
/* Number of blocks used for the thrashing sequence */
#define THRASHING_SIZE 32
/* Only return the concrete result for the latest access. Ignores profiling. */
#define ONLY_ONE_TIME FALSE
/* Use performance counters instead of rdtsc */
#define USE_PMC FALSE
/* Use core cycle instead of TSC */
#define CORE_CYCLES TRUE

/* COMPILE TIME OPTIONS */
/* Maximum numbers of pages used for code allocation */
#define JIT_CODE_SZ 12
/* Size of eviction sets */
#define EVICTION_SZ(LVL)	((LVL##_CACHE_WAYS/2)*(LVL##_WAYS_BITS+1)) // for PLRU http://www.rw.cdl.uni-saarland.de/~grund/papers/rts07-predictability.pdf
/* Multiplicative factor for size of eviction sets, required for non-temporal accesses */
#define MARGIN 6
/* Extra L1D non-temporal memory accesses after a cache eviction (clean LFB) */
#define LFB 0
/* Extra L2 non-temporal memory accesses after cache victim (clean SQ) */
#define SQ 0
/* VERBOSE MODE */
#define VERBOSE 1 // 0 - disabled, 1 - verbose, 2 - very verbose
#define PRINT(...)			if (VERBOSE > 0) printk(__VA_ARGS__);
#define WPRINT(...)			if (VERBOSE > 1) printk(__VA_ARGS__);

/* POOL SIZES FOR EVICTION SETS */
#define LVL_3 3
#define L3_POOL_SZ (18*1024*1024)
#define L3_CACHE_SLICES (1<<L3_SLICE_BITS)
#define L3_CACHE_LINE (1<<L3_LINE_BITS)
#define L3_CACHE_SETS (1<<(L3_SET_BITS+L3_SLICE_BITS))

#define LVL_2 2
#define L2_POOL_SZ (12*1024*1024)
#define L2_CACHE_SLICES (1<<L2_SLICE_BITS)
#define L2_CACHE_LINE (1<<L2_LINE_BITS)
#define L2_CACHE_SETS (1<<(L2_SET_BITS+L2_SLICE_BITS))

#define LVL_1 1
#define L1_POOL_SZ (2*1024*1024)
#define L1_CACHE_SLICES (1<<L1_SLICE_BITS)
#define L1_CACHE_LINE (1<<L1_LINE_BITS)
#define L1_CACHE_SETS (1<<(L1_SET_BITS+L1_SLICE_BITS))

#endif /* __SETTINGS_H */
