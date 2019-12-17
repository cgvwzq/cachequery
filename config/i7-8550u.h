/* KabyLake R laptop i7-8550U */
#define SERIAL 1 // 0 - lfence, 1 - mfence, 2 - cpuid
#define USE_RDTSCP 0 // 0 - not supported, 1 - supported

#define L3_CACHE_WAYS 16
#define L3_WAYS_BITS 4
#define L3_SET_BITS 10
#define L3_LINE_BITS 6
#define L3_SLICE_BITS 3

#define L2_PHYS_INDEXED 1
#define L2_CACHE_WAYS 4
#define L2_WAYS_BITS 3
#define L2_SET_BITS 10
#define L2_LINE_BITS 6
#define L2_SLICE_BITS 0

#define L1_CACHE_WAYS 8
#define L1_WAYS_BITS 3
#define L1_SET_BITS 6
#define L1_LINE_BITS 6
#define L1_SLICE_BITS 0

//#define IS_LEADER_SET(x,s) (FALSE)
#define IS_LEADER_SET(x,s) ((((x>>5)^(x&0x1f)) == (x&0x2)))

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
#define L3_HIT_THRESHOLD 150 // 168-174
#define L3_MISS_THRESHOLD 250 // > 290
#define L2_HIT_THRESHOLD 141 // 144-145
#define L2_MISS_THRESHOLD 160 // 175-..
#define L1_HIT_THRESHOLD 1 // 139-141
#define L1_MISS_THRESHOLD 144 //144-145

/*
// rftscp + 2ghz
#define L3_HIT_THRESHOLD 78 // 79-96
#define L3_MISS_THRESHOLD 197 // >197
#define L2_HIT_THRESHOLD 52 // 53-65
#define L2_MISS_THRESHOLD 88 // 88-102
#define L1_HIT_THRESHOLD 1 // 47- 57
#define L1_MISS_THRESHOLD 55 // 53 - 65
*/