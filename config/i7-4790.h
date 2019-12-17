/* Haswell desktop i7-4790 */
#define SERIAL 1 // 0 - lfence, 1 - mfence, 2 - cpuid
#define USE_RDTSCP 0 // 0 - not supported, 1 - supported

#define L3_CACHE_WAYS 16
#define L3_WAYS_BITS 4
#define L3_SET_BITS 11
#define L3_LINE_BITS 6
#define L3_SLICE_BITS 2

#define L2_PHYS_INDEXED 1
#define L2_CACHE_WAYS 8
#define L2_WAYS_BITS 3
#define L2_SET_BITS 9
#define L2_LINE_BITS 6
#define L2_SLICE_BITS 0

#define L1_CACHE_WAYS 8
#define L1_WAYS_BITS 3
#define L1_SET_BITS 6
#define L1_LINE_BITS 6
#define L1_SLICE_BITS 0

#define IS_LEADER_SET(x,s) ((s == 0) && (((x>>5) == 8)) || (((x>>5) == 12))) // 512-575 & 768-831 in slice0

/*
// Automatic calibration
#define L3_HIT_THRESHOLD 0
#define L3_MISS_THRESHOLD 0
#define L2_HIT_THRESHOLD 0
#define L2_MISS_THRESHOLD 0
#define L1_HIT_THRESHOLD 0
#define L1_MISS_THRESHOLD 0
*/

// core cycles
#define L3_HIT_THRESHOLD 190 //231 // 232-240
#define L3_MISS_THRESHOLD 250 //400 // 440-...
#define L2_HIT_THRESHOLD 170 // 192-200
#define L2_MISS_THRESHOLD 194 // 232-240-248
#define L1_HIT_THRESHOLD 1 // 184
#define L1_MISS_THRESHOLD 173 // 192-200

/*
// rdtsc + 2ghz
#define L3_HIT_THRESHOLD 231 // 232-240
#define L3_MISS_THRESHOLD 400 // 440-...
#define L2_HIT_THRESHOLD 191 // 192-200
#define L2_MISS_THRESHOLD 231 // 232-240-248
#define L1_HIT_THRESHOLD 1 // 184
#define L1_MISS_THRESHOLD 192 // 192-200
*/