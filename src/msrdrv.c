#include "../include/msrdrv.h"

static struct MsrInOut msr_start_l3[] = {
	{ .op=MSR_WRITE, .ecx=MSR_IA32_PERFEVTSEL0, { .eax=MEM_LOAD_RETIRED_L3_MISS(), .edx=0x00 }},
	{ .op=MSR_WRITE, .ecx=MSR_IA32_PERF_GLOBAL_CTRL, { .eax=0x1, .edx=0x0 }}, // enables PMC0
	{ .op=MSR_STOP, .ecx=0x00, .value=0x00 },
};
static struct MsrInOut msr_start_l2[] = {
	{ .op=MSR_WRITE, .ecx=MSR_IA32_PERFEVTSEL0, { .eax=MEM_LOAD_RETIRED_L2_MISS(), .edx=0x00 }},
	{ .op=MSR_WRITE, .ecx=MSR_IA32_PERF_GLOBAL_CTRL, { .eax=0x1, .edx=0x0 }}, // enables PMC0
	{ .op=MSR_STOP, .ecx=0x00, .value=0x00 },
};
static struct MsrInOut msr_start_l1[] = {
	{ .op=MSR_WRITE, .ecx=MSR_IA32_PERFEVTSEL0, { .eax=MEM_LOAD_RETIRED_L1_MISS(), .edx=0x00 }},
	{ .op=MSR_WRITE, .ecx=MSR_IA32_PERF_GLOBAL_CTRL, { .eax=0x1, .edx=0x0 }}, // enables PMC0
	{ .op=MSR_STOP, .ecx=0x00, .value=0x00 },
};
static struct MsrInOut msr_start_cycles[] = {
	{ .op=MSR_WRITE, .ecx=MSR_IA32_PERFEVTSEL0, { .eax=CPU_CLK_UNHALTED(), .edx=0x00 }},
	{ .op=MSR_WRITE, .ecx=MSR_IA32_PERF_GLOBAL_CTRL, { .eax=0x1, .edx=0x0 }}, // enables PMC0
	{ .op=MSR_STOP, .ecx=0x00, .value=0x00 },
};
static struct MsrInOut msr_stop[] = {
	{ .op=MSR_READ, .ecx=_MSR_IA32_PMC0, { .eax=0x00, .edx=0x00 }},
	{ .op=MSR_WRITE, .ecx=MSR_IA32_PERF_GLOBAL_CTRL, { .eax=0x00, .edx=0x00 }},
	{ .op=MSR_STOP, .ecx=0x00, .value=0x00 },
};

long long read_msr(unsigned int ecx) {
	unsigned int edx = 0, eax = 0;
	unsigned long long result = 0;
	__asm__ __volatile__("rdmsr" : "=a"(eax), "=d"(edx) : "c"(ecx));
	result = eax | (unsigned long long)edx << 0x20;
//	printk(KERN_ALERT "Module msrdrv: Read 0x%016llx (0x%08x:0x%08x) from MSR 0x%08x\n", result, edx, eax, ecx);
	return result;
}

void write_msr(int ecx, unsigned int eax, unsigned int edx) {
//	printk(KERN_ALERT "Module msrdrv: Writing 0x%08x:0x%08x to MSR 0x%04x\n", edx, eax, ecx);
	__asm__ __volatile__("wrmsr" : : "c"(ecx), "a"(eax), "d"(edx));
}

long msrdrv_run(struct MsrInOut *msrops, int n)
{
	int i;
	for (i = 0; i < n; i++)
	{
		switch (msrops[i].op)
		{
			case MSR_NOP:
			//	printk(KERN_ALERT "Module msrdrv: seen MSR_NOP command\n");
				break;
			case MSR_STOP:
			//	printk(KERN_ALERT "Module msrdrv: seen MSR_STOP command\n");
				goto label_end;
			case MSR_READ:
			//	printk(KERN_ALERT "Module msrdrv: seen MSR_READ command\n");
				msrops[i].value = read_msr(msrops[i].ecx);
				break;
			case MSR_WRITE:
			//	printk(KERN_ALERT "Module msrdrv: seen MSR_WRITE command\n");
				write_msr(msrops[i].ecx, msrops[i].eax, msrops[i].edx);
				break;
			default:
			//	printk(KERN_ALERT "Module msrdrv: Unknown option 0x%x\n", msrops[i].op);
				return 1;
		}

	}
	label_end:

	return 0;
}

void
prepare_counters (int level)
{
	switch (level)
	{
		case 1:
			msrdrv_run (msr_start_l1, 2);
			break;
		case 2:
			msrdrv_run (msr_start_l2, 2);
			break;
		case 3:
			msrdrv_run (msr_start_l3, 2);
			break;
		default:
			msrdrv_run (msr_start_cycles, 2);
			break;
	}
}

void
disable_counters (void)
{
	msrdrv_run (msr_stop, 2);
}
