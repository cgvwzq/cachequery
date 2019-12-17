#ifndef __MSRDRV_H
#define __MSRDRV_H

#include <linux/kernel.h>
#include <linux/types.h>

// List of MSR event selection registers IA32_PERFECTSELx
#define MSR_IA32_PERFEVTSEL0			0x00000186
#define MSR_IA32_PERFEVTSEL1			0x00000187
#define MSR_IA32_PERFEVTSEL2			0x00000188
#define MSR_IA32_PERFEVTSEL3			0x00000189

// List of MSR counter registers IA32_PPMCx
#define _MSR_IA32_PMC0					0x000000c1
#define _MSR_IA32_PMC1					0x000000c2
#define _MSR_IA32_PMC2					0x000000c3
#define _MSR_IA32_PMC3					0x000000c4

//
#define MSR_IA32_PERF_CABABILITIES		0x00000345
#define MSR_IA32_PERF_GLOBAL_STATUS 	0x0000038e
#define MSR_IA32_PERF_GLOBAL_CTRL 		0x0000038f
#define MSR_IA32_PERF_GLOBAL_OVF_CTRL 	0x00000390

// Precise Events for the Skylake, Kaby Lake and Coffee Lake Microarchitectures
// This subset is the same also for Haswell
// PERFECT | UMASK
#define MEM_LOAD_RETIRED_L1_HIT()		PERFEVTSELx(0xD1, 0x01)
#define MEM_LOAD_RETIRED_L1_MISS()		PERFEVTSELx(0xD1, 0x08)
#define MEM_LOAD_RETIRED_L2_HIT()		PERFEVTSELx(0xD1, 0x02)
#define MEM_LOAD_RETIRED_L2_MISS()		PERFEVTSELx(0xD1, 0x10)
#define MEM_LOAD_RETIRED_L3_HIT()		PERFEVTSELx(0xD1, 0x04)
#define MEM_LOAD_RETIRED_L3_MISS()		PERFEVTSELx(0xD1, 0x20)
#define CPU_CLK_UNHALTED()				PERFEVTSELx(0x3c, 0x00)


// I32_PERFEVTSELx layout (PMC v3)
// 7:0 Event Select (event number fom event tables, for the event we are interested in)
// 15:8 Unit Mask UMASK (umask value from event tables, for the event we are interested in)
#define EVTSEL_USR						BIT(16) // if set, counts during exec in ring != 0
#define EVTSEL_OS						BIT(17) // if set, counts during exec in ring = 0
#define EVTSEL_EDGE						BIT(18) // if set, enables edge detection of the event
#define EVTSEL_PC						BIT(19) // pin control
#define EVTSEL_INT						BIT(20) // generate interrupt through APIC on overflow (usually 48-bit)
#define EVTSEL_ANY						BIT(21) // increment counter with event on any hardware therad on any physical core
#define EVTSEL_EN						BIT(22) // enable the counter
#define EVTSEL_INV						BIT(23) // invert counter mask, changes meaning of CMASK
// 31:24 CMASK
#define EVTSEL_CMASK					0x0 // if non-zero, PMC only increment when event is triggered >= (or < if INV is set) CMASK times in a single cycle
// 63:31 reserved

// EVT = [EVTSEL, UMASK]
#define PERFEVTSELx(EVTSEL,UMASK)		(EVTSEL|(UMASK<<8)|EVTSEL_USR|EVTSEL_OS|EVTSEL_EN)

enum MsrOperation {
	MSR_NOP   = 0,
	MSR_READ  = 1,
	MSR_WRITE = 2,
	MSR_STOP  = 3,
	MSR_RDTSC = 4
};

struct MsrInOut {
	unsigned int op;              // MsrOperation
	unsigned int ecx;             // msr identifier
	union {
		struct {
			unsigned int eax;     // low double word
			unsigned int edx;     // high double word
		};
		unsigned long long value; // quad word
	};
}; // msrdrv.h:27:1: warning: packed attribute is unnecessary for ‘MsrInOut’ [-Wpacked]

void prepare_counters(int lvl);
void disable_counters(void);

#endif /* __MSRDRV_H */