#include "../config/settings.h"
#include "parser.h"

#ifndef __X86_H
#define __X86_H

#define C(...) (char[]){__VA_ARGS__}

#define PUSH_RBP()			C(0x55)
#define MOV_RBP_RSP()		C(0x48, 0x89, 0xe5)
#define PUSH_RBX()			C(0x53)
#define MOV_ECX_DWORD(...)	C(0xb9, __VA_ARGS__)
#define XOR_RSI_RSI()		C(0x48, 0x31, 0xf6)
#define WRMSR()				C(0x0f, 0x30)
#define MOV_RDI_DWORD(...)	C(0x48, 0xc7, 0xc7, __VA_ARGS__)

#define LOAD_RAX(Q)			C(0x48, 0xa1,\
	Q&0xff, (Q>>8)&0xff, (Q>>16)&0xff, (Q>>24)&0xff,\
	(Q>>32)&0xff, (Q>>40)&0xff, (Q>>48)&0xff, (Q>>56)&0xff)
#define MOV_RAX_CT(Q)		C(0x48, 0xb8,\
	(Q&0xff), (Q>>8)&0xff, (Q>>16)&0xff, (Q>>24)&0xff,\
	(Q>>32)&0xff, (Q>>40)&0xff, (Q>>48)&0xff, (Q>>56)&0xff)
#define CLFLUSH_RAX()		C(0x0f, 0xae, 0x38)
#define XOR_EAX_EAX()		C(0x31, 0xc0)
#define XOR_RAX_RAX()		C(0x48, 0x31, 0xc0)
#define XOR_EDX_EDX()		C(0x31, 0xd2)
#define XOR_RDX_RDX()		C(0x48, 0x31, 0xd2)
#define XOR_RDI_RDI()		C(0x48, 0x31, 0xff)
#define NEG_RDI()			C(0x48, 0xf7, 0xdf)
#define CPUID()				C(0x48, 0x31, 0xc0, 0x0f, 0xa2)
#define LFENCE()			C(0x0f, 0xae, 0xe8)
#define MFENCE()			C(0x0f, 0xae, 0xf0)
#define SFENCE()			C(0x0f, 0xae, 0xf8)
#define RDMSR()				C(0x0f, 0x32)
#define RDPMC()				C(0x0f, 0x33)
#define SHL_RSI()			C(0x48, 0xd1, 0xe6)
#define CMP_EAX_EDI()		C(0x39, 0xf8)
#define CMOVGE_EAX_EDI()	C(0x0f, 0x4d, 0xc7)
#define CMOVAE_EAX_EDI()	C(0x0f, 0x43, 0xc7)
#define CMOVAE_RAX_RDI()	C(0x48, 0x0f, 0x43, 0xc7)
#define OR_RSI_RAX()		C(0x48, 0x09, 0xc6)
#define MOV_RAX_RSI()		C(0x48, 0x89, 0xf0)
#define POP_RBX()			C(0x5b)
#define POP_RBP()			C(0x5d)
#define RETQ()				C(0xc3)
#define RDTSCP()			C(0x0f, 0x01, 0xf9) // warning: not supported in old architectures
#define RDTSC()				C(0x0f, 0x31)
#define SHL_RDX_CT(_0)		C(0x48, 0xc1, 0xe2, _0&0xff)
#define OR_RAX_RDX()		C(0x48, 0x09, 0xd0)
#define MOV_RDI_RAX()		C(0x48, 0x89, 0xc7)
#define MOV_RDX_RDI()		C(0x48, 0x89, 0xfa)
#define MOV_RAX_RDI()		C(0x48, 0x89, 0xf8)
#define SUB_RAX_RDI()		C(0x48, 0x29, 0xf8)
#define SUB_RDI_RAX()		C(0x48, 0x29, 0xc7)
#define MOV_EDX_DWORD(...)	C(0xba, __VA_ARGS__)
#define MOV_ECX_DWORD(...)	C(0xb9, __VA_ARGS__)
#define CMP_RDX_CT(_0)		C(0x48, 0x81, 0xfa, _0&0xff, (_0>>8)&0xff, (_0>>16)&0xff, (_0>>24)&0xff)
#define MOV_RDX_RAX()		C(0x48, 0x89, 0xc2)
#define JBE_NEAR(_0)		C(0x0f, 0x86,(_0-6)&0xff, ((_0-6)>>8)&0xff, ((_0-6)>>16)&0xff, ((_0-6)>>24)&0xff)
#define JMP_SHORT(_0)		C(0xeb, (_0-2)&0xff)
#define MOVNTDQA_RAX()		C(0x66, 0x0f, 0x38, 0x2a, 0x08)
#define MOV_RAX_CR0()		C(0x0f, 0x20, 0xc0)
#define MOV_CR0_RAX()		C(0x0f, 0x22, 0xc0)
#define WBINVD()			C(0x0f, 0x09)

#if SERIAL == 0
	#define SERIALIZE()		LFENCE()
#elif SERIAL == 1
	#define SERIALIZE()		MFENCE()
#else
	#define SERIALIZE()		CPUID()
#endif

#define MEASURE_PRE_TSC(code)			\
	{									\
		OPCODE(code, CPUID());			\
		OPCODE(code, RDTSC());			\
		OPCODE(code, SERIALIZE());		\
		OPCODE(code, SHL_RDX_CT(32));	\
		OPCODE(code, OR_RAX_RDX());		\
		OPCODE(code, MOV_RDI_RAX());	\
	}

#if USE_RDTSCP == 1
#define MEASURE_POST_TSC(code)			\
	{									\
		OPCODE(code, RDTSCP());			\
		OPCODE(code, SHL_RDX_CT(32));	\
		OPCODE(code, OR_RAX_RDX());		\
		OPCODE(code, SUB_RDI_RAX());	\
		OPCODE(code, NEG_RDI());		\
	}
#else
#define MEASURE_POST_TSC(code)			\
	{									\
		OPCODE(code, SERIALIZE());		\
		OPCODE(code, RDTSC());			\
		OPCODE(code, SHL_RDX_CT(32));	\
		OPCODE(code, OR_RAX_RDX());		\
		OPCODE(code, SUB_RDI_RAX());	\
		OPCODE(code, NEG_RDI());		\
	}
#endif

// wrmsr does not serialize well in all archs, add fence anyway
#define RESET_PMC0(code)				\
	{									\
		OPCODE(code, MOV_ECX_DWORD(_MSR_IA32_PMC0, 0x00, 0x00, 0x00)); \
		OPCODE(code, XOR_EAX_EAX());	\
		OPCODE(code, XOR_EDX_EDX());	\
		OPCODE(code, SERIALIZE());		\
		OPCODE(code, WRMSR());			\
	}

#define MEASURE_POST_CORE(code)			\
	{									\
		OPCODE(code, SERIALIZE());		\
		OPCODE(code, MOV_ECX_DWORD(_MSR_IA32_PMC0, 0x00, 0x00, 0x00)); \
		OPCODE(code, RDMSR());			\
		OPCODE(code, SHL_RDX_CT(32));	\
		OPCODE(code, OR_RAX_RDX());		\
		OPCODE(code, MOV_RDI_RAX());	\
	}

#endif /* __X86_H */
