#include <stdio.h>
#include <string.h>
#include "tagha.h"

const char *RegIDToStr(const enum RegID id)
{
	switch( id ) {
		case ras: return "ras";
		case rbs: return "rbs";
		case rcs: return "rcs";
		case rds: return "rds";
		case res: return "res";
		case rfs: return "rfs";
		case rgs: return "rgs";
		case rhs: return "rhs";
		case ris: return "ris";
		case rjs: return "rjs";
		case rks: return "rks";
		case rsp: return "rsp";
		case rbp: return "rbp";
		case rip: return "rip";
		default:  return "bad register id";
	}
}

void PrintAddrMode(const enum AddrMode mode)
{
	char str[150] = {0};
		strncat(str, "Addressing Modes: ", 150);
	if( mode & Immediate )
		strncat(str, "Immediate|", 150);
	if( mode & Register )
		strncat(str, "Register|", 150);
	if( mode & RegIndirect )
		strncat(str, "RegIndirect|", 150);
	if( mode & Direct )
		strncat(str, "Direct|", 150);
	if( mode & Byte )
		strncat(str, "Byte|", 150);
	if( mode & TwoBytes )
		strncat(str, "TwoBytes|", 150);
	if( mode & FourBytes )
		strncat(str, "FourBytes|", 150);
	if( mode & EightBytes )
		strncat(str, "EightBytes|", 150);
	if( !mode )
		strncat(str, "0", 150);
	puts(str);
}

void PrintRegData(const struct TaghaScript *script)
{
	puts("\n\tPRINTING REGISTER DATA ==========================\n");
	for( uint8_t i=0 ; i<regsize ; i++ )
		printf("register[%s] == %" PRIu64 "\n", RegIDToStr(i), script->m_Regs[i].UInt64);
	puts("\tEND OF PRINTING REGISTER DATA ===============\n");
}

//#include <unistd.h>	// sleep() func
int Tagha_Exec(struct TaghaVM *restrict vm, int argc, union CValue argv[])
{
	if( !vm or !vm->m_pScript )
		return -1;
	
	struct TaghaScript *script = vm->m_pScript;
	if( !script or !script->m_pmapFuncs )
		return -1;
	else if( !script->m_Regs[rip].UCharPtr ) {
		TaghaScript_PrintErr(script, __func__, "NULL instruction ptr! if 'main' doesn't exist, call a function by name.");
		return -1;
	}
	
	fnNative_t	pfNative = NULL;
	bool
		safemode = script->m_bSafeMode,
		debugmode = script->m_bDebugMode
	;
	uint8_t
		instr,
		addrmode	// what addressing mode to use
	;
	union CValue
		a,	// 1st operand temporary
		b	// 2nd operand temporary
	;
	
#define X(x) #x ,
	// for debugging purposes.
	const char *opcode2str[] = { INSTR_SET };
#undef X
	
#define X(x) &&exec_##x ,
	// our instruction dispatch table.
	const void *dispatch[] = { INSTR_SET };
#undef X
#undef INSTR_SET
	
	(--script->m_Regs[rsp].SelfPtr)->UInt64 = (uintptr_t)argv;
	(--script->m_Regs[rsp].SelfPtr)->Int64 = argc;
	if( debugmode )
		printf("Tagha_Exec :: pushed argc: %i and pushed argv %p\n", argc, argv);
	
	int32_t offset;
	while( 1 ) {
		script->m_uiMaxInstrs--;
		if( !script->m_uiMaxInstrs )
			break;
		
		safemode = script->m_bSafeMode;
		debugmode = script->m_bDebugMode;
		if( safemode ) {
			if( script->m_Regs[rip].UCharPtr < script->m_pText ) {
				TaghaScript_PrintErr(script, __func__, "instruction address out of lower bounds!");
				goto *dispatch[halt];
			}
			else if( script->m_Regs[rip].UCharPtr - script->m_pText >= script->m_uiInstrSize ) {
				TaghaScript_PrintErr(script, __func__, "instruction address out of upper bounds!");
				goto *dispatch[halt];
			}
			else if( *script->m_Regs[rip].UCharPtr > nop ) {
				TaghaScript_PrintErr(script, __func__, "illegal instruction exception! | instruction == \'%" PRIu32 "\'", *script->m_Regs[rip].UCharPtr);
				goto *dispatch[halt];
			}
		}
		a.UInt64 = b.UInt64 = 0;	// reset our temporaries.
		offset = 0;
		
		// fetch opcode and addressing mode.
		instr = *script->m_Regs[rip].UCharPtr++;
		addrmode = *script->m_Regs[rip].UCharPtr++;
		
		// this is for debugging.
#ifdef _UNISTD_H
		sleep(1);
#endif
		if( debugmode ) {
			printf("opcode == '%s' | ", opcode2str[instr]);
			PrintAddrMode(addrmode);
		}
		goto *dispatch[instr];
		
		exec_nop:;
			continue;
		
		exec_halt:;
			TaghaScript_PrintMem(script);
			PrintRegData(script);
			return script->m_Regs[ras].UInt64;
		
		exec_push:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				(--script->m_Regs[rsp].SelfPtr)->UInt64 = a.UInt64;
			else if( addrmode & Register )
				(--script->m_Regs[rsp].SelfPtr)->UInt64 = script->m_Regs[a.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				(--script->m_Regs[rsp].SelfPtr)->UInt64 = *(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset);
			}
			else if( addrmode & Direct ) {
				(--script->m_Regs[rsp].SelfPtr)->UInt64 = *a.UInt64Ptr;
			}
			continue;
		}
		exec_pop:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Register )
				script->m_Regs[a.UInt64].UInt64 = (*script->m_Regs[rsp].SelfPtr++).UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) = (*script->m_Regs[rsp].SelfPtr++).UInt64;
			}
			else if( addrmode & (Direct|Immediate) ) {
				*a.UInt64Ptr = (*script->m_Regs[rsp].SelfPtr++).UInt64;
			}
			continue;
		}
		exec_neg:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Register )
				script->m_Regs[a.UInt64].UInt64 = -script->m_Regs[a.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					*(script->m_Regs[a.UInt64].CharPtr+offset) = -*(script->m_Regs[a.UInt64].CharPtr+offset);
				else if( addrmode & TwoBytes )
					*(int16_t *)(script->m_Regs[a.UInt64].CharPtr+offset) = -*(int16_t *)(script->m_Regs[a.UInt64].CharPtr+offset);
				else if( addrmode & FourBytes )
					*(int32_t *)(script->m_Regs[a.UInt64].CharPtr+offset) = -*(int32_t *)(script->m_Regs[a.UInt64].CharPtr+offset);
				else if( addrmode & EightBytes )
					*(int64_t *)(script->m_Regs[a.UInt64].CharPtr+offset) = -*(int64_t *)(script->m_Regs[a.UInt64].CharPtr+offset);
			}
			else if( addrmode & (Direct|Immediate) ) {
				if( addrmode & Byte )
					*a.CharPtr = -*a.CharPtr;
				else if( addrmode & TwoBytes )
					*a.ShortPtr = -*a.ShortPtr;
				else if( addrmode & FourBytes )
					*a.Int32Ptr = -*a.Int32Ptr;
				else if( addrmode & EightBytes )
					*a.Int64Ptr = -*a.Int64Ptr;
			}
			continue;
		}
		
		exec_inc:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Register )
				script->m_Regs[a.UInt64].UInt64 += 1;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					*(script->m_Regs[a.UInt64].UCharPtr+offset) += 1;
				else if( addrmode & TwoBytes )
					*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) += 1;
				else if( addrmode & FourBytes )
					*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) += 1;
				else if( addrmode & EightBytes )
					*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) += 1;
			}
			else if( addrmode & (Direct|Immediate) ) {
				if( addrmode & Byte )
					*a.UCharPtr += 1;
				else if( addrmode & TwoBytes )
					*a.UShortPtr += 1;
				else if( addrmode & FourBytes )
					*a.UInt32Ptr += 1;
				else if( addrmode & EightBytes )
					*a.UInt64Ptr += 1;
			}
			continue;
		}
		exec_dec:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Register )
				script->m_Regs[a.UInt64].UInt64 -= 1;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					*(script->m_Regs[a.UInt64].UCharPtr+offset) -= 1;
				else if( addrmode & TwoBytes )
					*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= 1;
				else if( addrmode & FourBytes )
					*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= 1;
				else if( addrmode & EightBytes )
					*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= 1;
			}
			else if( addrmode & (Direct|Immediate) ) {
				if( addrmode & Byte )
					*a.UCharPtr -= 1;
				else if( addrmode & TwoBytes )
					*a.UShortPtr -= 1;
				else if( addrmode & FourBytes )
					*a.UInt32Ptr -= 1;
				else if( addrmode & EightBytes )
					*a.UInt64Ptr -= 1;
			}
			continue;
		}
		exec_bnot:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Register )
				script->m_Regs[a.UInt64].UInt64 = ~script->m_Regs[a.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					*(script->m_Regs[a.UInt64].UCharPtr+offset) = ~*(script->m_Regs[a.UInt64].UCharPtr+offset);
				else if( addrmode & TwoBytes )
					*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) = ~*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset);
				else if( addrmode & FourBytes )
					*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) = ~*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset);
				else if( addrmode & EightBytes )
					*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) = ~*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset);
			}
			else if( addrmode & (Direct|Immediate) ) {
				if( addrmode & Byte )
					*a.UCharPtr = ~*a.UCharPtr;
				else if( addrmode & TwoBytes )
					*a.UShortPtr = ~*a.UShortPtr;
				else if( addrmode & FourBytes )
					*a.UInt32Ptr = ~*a.UInt32Ptr;
				else if( addrmode & EightBytes )
					*a.UInt64Ptr = ~*a.UInt64Ptr;
			}
			continue;
		}
		
		exec_jmp:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_Regs[rip].UCharPtr = script->m_pText + a.UInt64;
			else if( addrmode & Register )
				script->m_Regs[rip].UCharPtr = script->m_pText + script->m_Regs[a.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				script->m_Regs[rip].UCharPtr = script->m_pText + *(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset);
			}
			else if( addrmode & Direct ) {
				script->m_Regs[rip].UCharPtr = script->m_pText + *a.UInt64Ptr;
			}
			continue;
		}
		
		exec_jz:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_Regs[rip].UCharPtr = (script->m_bZeroFlag) ? script->m_pText + a.UInt64 : script->m_Regs[rip].UCharPtr;
			else if( addrmode & Register )
				script->m_Regs[rip].UCharPtr = (script->m_bZeroFlag) ? script->m_pText + script->m_Regs[a.UInt64].UInt64 : script->m_Regs[rip].UCharPtr;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				script->m_Regs[rip].UCharPtr = (script->m_bZeroFlag) ? script->m_pText + *(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) : script->m_Regs[rip].UCharPtr;
			}
			else if( addrmode & Direct ) {
				script->m_Regs[rip].UCharPtr = (script->m_bZeroFlag) ? script->m_pText + *a.UInt64Ptr : script->m_Regs[rip].UCharPtr;
			}
			continue;
		}
		exec_jnz:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_Regs[rip].UCharPtr = (!script->m_bZeroFlag) ? script->m_pText + a.UInt64 : script->m_Regs[rip].UCharPtr;
			else if( addrmode & Register )
				script->m_Regs[rip].UCharPtr = (!script->m_bZeroFlag) ? script->m_pText + script->m_Regs[a.UInt64].UInt64 : script->m_Regs[rip].UCharPtr;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				script->m_Regs[rip].UCharPtr = (!script->m_bZeroFlag) ? script->m_pText + *(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) : script->m_Regs[rip].UCharPtr;
			}
			else if( addrmode & Direct ) {
				script->m_Regs[rip].UCharPtr = (!script->m_bZeroFlag) ? script->m_pText + *a.UInt64Ptr : script->m_Regs[rip].UCharPtr;
			}
			continue;
		}
		
		exec_call:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			(--script->m_Regs[rsp].SelfPtr)->UInt64 = script->m_Regs[rip].UInt64;	// push rip
			(--script->m_Regs[rsp].SelfPtr)->UInt64 = script->m_Regs[rbp].UInt64;	// push rbp
			script->m_Regs[rbp] = script->m_Regs[rsp];	// mov rbp, rsp
			if( addrmode & Immediate )
				script->m_Regs[rip].UCharPtr = script->m_pText + a.UInt64;
			else if( addrmode & Register )
				script->m_Regs[rip].UCharPtr = script->m_pText + script->m_Regs[a.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				script->m_Regs[rip].UCharPtr = script->m_pText + *(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset);
			}
			else if( addrmode & Direct ) {
				script->m_Regs[rip].UCharPtr = script->m_pText + *a.UInt64Ptr;
			}
			continue;
		}
		exec_ret:; {
			// if we hit a return and stack is empty, break entirely.
			if( script->m_Regs[rbp].UCharPtr == (script->m_pMemory + (script->m_uiMemsize-1)) ) {
				if( debugmode )
					puts("ret :: return on empty stack");
				goto *dispatch[halt];
			}
			script->m_Regs[rsp] = script->m_Regs[rbp];	// mov rsp, rbp
			script->m_Regs[rbp].UInt64 = (*script->m_Regs[rsp].SelfPtr++).UInt64;	// pop rbp
			script->m_Regs[rip].UInt64 = (*script->m_Regs[rsp].SelfPtr++).UInt64;	// pop rip
			if( addrmode & Immediate )
				script->m_Regs[rsp].UCharPtr += *script->m_Regs[rip].UInt64Ptr++;
			
			continue;
		}
		
		exec_callnat:; {
			if( safemode and !script->m_pstrNativeCalls ) {
				TaghaScript_PrintErr(script, __func__, "exec_callnat :: native table is NULL!");
				script->m_Regs[rip].UInt64Ptr++;
				script->m_Regs[rip].UInt32Ptr++;
				continue;
			}
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			char *nativestr;
			uint64_t index;
			
			if( addrmode & Immediate )
				index = a.UInt64;
			else if( addrmode & Register )
				index = script->m_Regs[a.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				index = *(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset);
			}
			else if( addrmode & Direct ) {
				index = *a.UInt64Ptr;
			}
			
			if( safemode and index >= script->m_uiNatives  ) {
				TaghaScript_PrintErr(script, __func__, "exec_callnat :: native index \'%" PRIu64 "\' is out of bounds!", index);
				script->m_Regs[rip].UInt32Ptr++;
				continue;
			}
			nativestr = script->m_pstrNativeCalls[index];
			
			pfNative = (fnNative_t)(uintptr_t) map_find(vm->m_pmapNatives, nativestr);
			if( safemode and !pfNative ) {
				TaghaScript_PrintErr(script, __func__, "exec_callnat :: native \'%s\' not registered!", nativestr);
				script->m_Regs[rip].UInt32Ptr++;
				continue;
			}
			// how many arguments pushed for native to use.
			const uint32_t argcount = *script->m_Regs[rip].UInt32Ptr++;
			if( debugmode )
				printf("callnat: Calling func addr: %p with %" PRIu32 " args pushed.\n", pfNative, argcount);
			
			// ERROR: you can't initialize an array using a variable as size.
			// have no choice but to use memset.
			CValue params[argcount];
			memset(params, 0, sizeof(CValue)*argcount);
			memcpy(params, script->m_Regs[rsp].SelfPtr, sizeof(CValue)*argcount);
			script->m_Regs[rsp].SelfPtr += argcount;
			printf("exec_callnat :: calling C function '%s'.\n", nativestr);
			script->m_Regs[ras].UInt64 = 0;
			(*pfNative)(script, params, script->m_Regs+ras, argcount, vm);
			continue;
		}
		
		exec_movr:; {	// dest is a reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_Regs[a.UInt64].UInt64 = b.UInt64;
			else if( addrmode & Register )
				script->m_Regs[a.UInt64].UInt64 = script->m_Regs[b.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].UChar = *(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].UShort = *(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].UInt32 = *(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].UInt64 = *(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].UChar = *b.UCharPtr;
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].UShort = *b.UShortPtr;
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].UInt32 = *b.UInt32Ptr;
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].UInt64 = *b.UInt64Ptr;
			}
			continue;
		}
		exec_movm:; {	// dest is memory and src is reg/imm/mem
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {	// moving value to register-based address
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) { // value is immediate constant
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) = b.UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) = b.UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) = b.UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) = b.UInt64;
				}
				else if( addrmode & Register ) { // value is in register
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) = script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) = script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) = script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) = script->m_Regs[b.UInt64].UInt64;
				}
			}
			else if( addrmode & Direct ) {	// moving value to direct address
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*a.UCharPtr = b.UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr = b.UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr = b.UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr = b.UInt64;
				}
				else if( addrmode & Register ) { // moving register to direct address
					if( addrmode & Byte )
						*a.UCharPtr = script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr = script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr = script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr = script->m_Regs[b.UInt64].UInt64;
				}
			}
			continue;
		}
		exec_lea:; {	// load effective address
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Register )
				script->m_Regs[a.UInt64].UInt64 = script->m_Regs[b.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				script->m_Regs[a.UInt64].UInt64 = script->m_Regs[b.UInt64].UInt64+offset;
			}
			else if( addrmode & (Direct|Immediate) )
				script->m_Regs[a.UInt64].UInt64 = b.UInt64;
			continue;
		}
		/*
		exec_leo:; {	// load effective offset - loads a memory offset.
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & (Direct|Immediate) )
				script->m_Regs[a.UInt64].UInt64 = (uintptr_t)(script->m_pMemory + b.UInt64);
			else if( addrmode & Register )
				script->m_Regs[a.UInt64].UInt64 = (uintptr_t)(script->m_pMemory + script->m_Regs[b.UInt64].UInt64);
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				script->m_Regs[a.UInt64].UInt64 = (uintptr_t)(script->m_pMemory + script->m_Regs[b.UInt64].UInt64 + offset);
			}
			continue;
		}
		*/
		exec_addr:; {	// dest is reg, src is reg/memory/imm
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_Regs[a.UInt64].Int64 += b.Int64;
			else if( addrmode & Register )
				script->m_Regs[a.UInt64].Int64 += script->m_Regs[b.UInt64].Int64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].Char += *(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].Short += *(int16_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].Int32 += *(int32_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].Int64 += *(int64_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].Char += *b.CharPtr;
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].Short += *b.ShortPtr;
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].Int32 += *b.Int32Ptr;
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].Int64 += *b.Int64Ptr;
			}
			continue;
		}
		exec_addm:; {	// dest is memory and src is reg/imm
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].CharPtr+offset) += b.Char;
					else if( addrmode & TwoBytes )
						*(int16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) += b.Short;
					else if( addrmode & FourBytes )
						*(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) += b.Int32;
					else if( addrmode & EightBytes )
						*(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) += b.Int64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].CharPtr+offset) += script->m_Regs[b.UInt64].Char;
					else if( addrmode & TwoBytes )
						*(int16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) += script->m_Regs[b.UInt64].Short;
					else if( addrmode & FourBytes )
						*(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) += script->m_Regs[b.UInt64].Int32;
					else if( addrmode & EightBytes )
						*(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) += script->m_Regs[b.UInt64].Int64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*a.CharPtr += b.Char;
					else if( addrmode & TwoBytes )
						*a.ShortPtr += b.Short;
					else if( addrmode & FourBytes )
						*a.Int32Ptr += b.Int32;
					else if( addrmode & EightBytes )
						*a.Int64Ptr += b.Int64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*a.CharPtr += script->m_Regs[b.UInt64].Char;
					else if( addrmode & TwoBytes )
						*a.ShortPtr += script->m_Regs[b.UInt64].Short;
					else if( addrmode & FourBytes )
						*a.Int32Ptr += script->m_Regs[b.UInt64].Int32;
					else if( addrmode & EightBytes )
						*a.Int64Ptr += script->m_Regs[b.UInt64].Int64;
				}
			}
			continue;
		}
		exec_uaddr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_Regs[a.UInt64].UInt64 += b.UInt64;
			else if( addrmode & Register )
				script->m_Regs[a.UInt64].UInt64 += script->m_Regs[b.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].UChar += *(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].UShort += *(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].UInt32 += *(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].UInt64 += *(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].UChar += *b.UCharPtr;
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].UShort += *b.UShortPtr;
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].UInt32 += *b.UInt32Ptr;
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].UInt64 += *b.UInt64Ptr;
			}
			continue;
		}
		exec_uaddm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) += b.UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) += b.UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) += b.UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) += b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) += script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) += script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) += script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) += script->m_Regs[b.UInt64].UInt64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*a.UCharPtr += b.UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr += b.UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr += b.UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr += b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*a.UCharPtr += script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr += script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr += script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr += script->m_Regs[b.UInt64].UInt64;
				}
			}
			continue;
		}
		
		exec_subr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_Regs[a.UInt64].Int64 -= b.Int64;
			else if( addrmode & Register )
				script->m_Regs[a.UInt64].Int64 -= script->m_Regs[b.UInt64].Int64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].Char -= *(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].Short -= *(int16_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].Int32 -= *(int32_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].Int64 -= *(int64_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].Char -= *b.CharPtr;
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].Short -= *b.ShortPtr;
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].Int32 -= *b.Int32Ptr;
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].Int64 -= *b.Int64Ptr;
			}
			continue;
		}
		exec_subm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].CharPtr+offset) -= b.Char;
					else if( addrmode & TwoBytes )
						*(int16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= b.Short;
					else if( addrmode & FourBytes )
						*(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= b.Int32;
					else if( addrmode & EightBytes )
						*(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= b.Int64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].CharPtr+offset) -= script->m_Regs[b.UInt64].Char;
					else if( addrmode & TwoBytes )
						*(int16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= script->m_Regs[b.UInt64].Short;
					else if( addrmode & FourBytes )
						*(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= script->m_Regs[b.UInt64].Int32;
					else if( addrmode & EightBytes )
						*(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= script->m_Regs[b.UInt64].Int64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*a.CharPtr -= b.Char;
					else if( addrmode & TwoBytes )
						*a.ShortPtr -= b.Short;
					else if( addrmode & FourBytes )
						*a.Int32Ptr -= b.Int32;
					else if( addrmode & EightBytes )
						*a.Int64Ptr -= b.Int64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*a.CharPtr -= script->m_Regs[b.UInt64].Char;
					else if( addrmode & TwoBytes )
						*a.ShortPtr -= script->m_Regs[b.UInt64].Short;
					else if( addrmode & FourBytes )
						*a.Int32Ptr -= script->m_Regs[b.UInt64].Int32;
					else if( addrmode & EightBytes )
						*a.Int64Ptr -= script->m_Regs[b.UInt64].Int64;
				}
			}
			continue;
		}
		exec_usubr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_Regs[a.UInt64].UInt64 -= b.UInt64;
			else if( addrmode & Register )
				script->m_Regs[a.UInt64].UInt64 -= script->m_Regs[b.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].UChar -= *(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].UShort -= *(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].UInt32 -= *(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].UInt64 -= *(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].UChar -= *b.UCharPtr;
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].UShort -= *b.UShortPtr;
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].UInt32 -= *b.UInt32Ptr;
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].UInt64 -= *b.UInt64Ptr;
			}
			continue;
		}
		exec_usubm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) -= b.UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= b.UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= b.UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) -= script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= script->m_Regs[b.UInt64].UInt64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*a.UCharPtr -= b.UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr -= b.UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr -= b.UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr -= b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*a.UCharPtr -= script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr -= script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr -= script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr -= script->m_Regs[b.UInt64].UInt64;
				}
			}
			continue;
		}
		
		exec_mulr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_Regs[a.UInt64].Int64 *= b.Int64;
			else if( addrmode & Register )
				script->m_Regs[a.UInt64].Int64 *= script->m_Regs[b.UInt64].Int64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].Char *= *(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].Short *= *(int16_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].Int32 *= *(int32_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].Int64 *= *(int64_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].Char *= *b.CharPtr;
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].Short *= *b.ShortPtr;
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].Int32 *= *b.Int32Ptr;
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].Int64 *= *b.Int64Ptr;
			}
			continue;
		}
		exec_mulm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].CharPtr+offset) *= b.Char;
					else if( addrmode & TwoBytes )
						*(int16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) *= b.Short;
					else if( addrmode & FourBytes )
						*(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) *= b.Int32;
					else if( addrmode & EightBytes )
						*(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) *= b.Int64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].CharPtr+offset) *= script->m_Regs[b.UInt64].Char;
					else if( addrmode & TwoBytes )
						*(int16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) *= script->m_Regs[b.UInt64].Short;
					else if( addrmode & FourBytes )
						*(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) *= script->m_Regs[b.UInt64].Int32;
					else if( addrmode & EightBytes )
						*(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) *= script->m_Regs[b.UInt64].Int64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*a.CharPtr *= b.Char;
					else if( addrmode & TwoBytes )
						*a.ShortPtr *= b.Short;
					else if( addrmode & FourBytes )
						*a.Int32Ptr *= b.Int32;
					else if( addrmode & EightBytes )
						*a.Int64Ptr *= b.Int64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*a.CharPtr *= script->m_Regs[b.UInt64].Char;
					else if( addrmode & TwoBytes )
						*a.ShortPtr *= script->m_Regs[b.UInt64].Short;
					else if( addrmode & FourBytes )
						*a.Int32Ptr *= script->m_Regs[b.UInt64].Int32;
					else if( addrmode & EightBytes )
						*a.Int64Ptr *= script->m_Regs[b.UInt64].Int64;
				}
			}
			continue;
		}
		exec_umulr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_Regs[a.UInt64].UInt64 *= b.UInt64;
			else if( addrmode & Register )
				script->m_Regs[a.UInt64].UInt64 *= script->m_Regs[b.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].UChar *= *(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].UShort *= *(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].UInt32 *= *(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].UInt64 *= *(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].UChar *= *b.UCharPtr;
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].UShort *= *b.UShortPtr;
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].UInt32 *= *b.UInt32Ptr;
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].UInt64 *= *b.UInt64Ptr;
			}
			continue;
		}
		exec_umulm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) *= b.UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) *= b.UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) *= b.UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) *= b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) *= script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) *= script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) *= script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) *= script->m_Regs[b.UInt64].UInt64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*a.UCharPtr *= b.UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr *= b.UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr *= b.UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr *= b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*a.UCharPtr *= script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr *= script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr *= script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr *= script->m_Regs[b.UInt64].UInt64;
				}
			}
			continue;
		}
		
		exec_divr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate ) {
				if( !b.Int64 )
					b.Int64 = 1;
				script->m_Regs[a.UInt64].Int64 /= b.Int64;
			}
			else if( addrmode & Register ) {
				if( !script->m_Regs[b.UInt64].Int64 )
					script->m_Regs[b.UInt64].Int64 = 1;
				script->m_Regs[a.UInt64].Int64 /= script->m_Regs[b.UInt64].Int64;
			}
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte ) {
					if( !*(script->m_Regs[b.UInt64].CharPtr+offset) )
						*(script->m_Regs[b.UInt64].CharPtr+offset) = 1;
					script->m_Regs[a.UInt64].Char /= *(script->m_Regs[b.UInt64].CharPtr+offset);
				}
				else if( addrmode & TwoBytes ) {
					if( !*(int16_t *)(script->m_Regs[b.UInt64].CharPtr+offset) )
						*(int16_t *)(script->m_Regs[b.UInt64].CharPtr+offset) = 1;
					script->m_Regs[a.UInt64].Short /= *(int16_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				}
				else if( addrmode & FourBytes ) {
					if( !*(int32_t *)(script->m_Regs[b.UInt64].CharPtr+offset) )
						*(int32_t *)(script->m_Regs[b.UInt64].CharPtr+offset) = 1;
					script->m_Regs[a.UInt64].Int32 /= *(int32_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				}
				else if( addrmode & EightBytes ) {
					if( !*(int64_t *)(script->m_Regs[b.UInt64].CharPtr+offset) )
						*(int64_t *)(script->m_Regs[b.UInt64].CharPtr+offset) = 1;
					script->m_Regs[a.UInt64].Int64 /= *(int64_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte ) {
					if( !*b.CharPtr )
						*b.CharPtr = 1;
					script->m_Regs[a.UInt64].Char /= *b.CharPtr;
				}
				else if( addrmode & TwoBytes ) {
					if( !*b.ShortPtr )
						*b.ShortPtr = 1;
					script->m_Regs[a.UInt64].Short /= *b.ShortPtr;
				}
				else if( addrmode & FourBytes ) {
					if( !*b.Int32Ptr )
						*b.Int32Ptr = 1;
					script->m_Regs[a.UInt64].Int32 /= *b.Int32Ptr;
				}
				else if( addrmode & EightBytes ) {
					if( !*b.Int64Ptr )
						*b.Int64Ptr = 1;
					script->m_Regs[a.UInt64].Int64 /= *b.Int64Ptr;
				}
			}
			continue;
		}
		exec_divm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( !b.UInt64 )
						b.UInt64 = 1;
					
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].CharPtr+offset) /= b.Char;
					else if( addrmode & TwoBytes )
						*(int16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) /= b.Short;
					else if( addrmode & FourBytes )
						*(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) /= b.Int32;
					else if( addrmode & EightBytes )
						*(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) /= b.Int64;
				}
				else if( addrmode & Register ) {
					if( !script->m_Regs[b.UInt64].UInt64 )
						script->m_Regs[b.UInt64].UInt64 = 1;
					
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].CharPtr+offset) /= script->m_Regs[b.UInt64].Char;
					else if( addrmode & TwoBytes )
						*(int16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) /= script->m_Regs[b.UInt64].Short;
					else if( addrmode & FourBytes )
						*(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) /= script->m_Regs[b.UInt64].Int32;
					else if( addrmode & EightBytes )
						*(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) /= script->m_Regs[b.UInt64].Int64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( !b.UInt64 )
						b.UInt64 = 1;
					
					if( addrmode & Byte )
						*a.CharPtr /= b.Char;
					else if( addrmode & TwoBytes )
						*a.ShortPtr /= b.Short;
					else if( addrmode & FourBytes )
						*a.Int32Ptr /= b.Int32;
					else if( addrmode & EightBytes )
						*a.Int64Ptr /= b.Int64;
				}
				else if( addrmode & Register ) {
					if( !script->m_Regs[b.UInt64].UInt64 )
						script->m_Regs[b.UInt64].UInt64 = 1;
					
					if( addrmode & Byte )
						*a.CharPtr /= script->m_Regs[b.UInt64].Char;
					else if( addrmode & TwoBytes )
						*a.ShortPtr /= script->m_Regs[b.UInt64].Short;
					else if( addrmode & FourBytes )
						*a.Int32Ptr /= script->m_Regs[b.UInt64].Int32;
					else if( addrmode & EightBytes )
						*a.Int64Ptr /= script->m_Regs[b.UInt64].Int64;
				}
			}
			continue;
		}
		exec_udivr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate ) {
				if( !b.UInt64 )
					b.UInt64 = 1;
				script->m_Regs[a.UInt64].UInt64 /= b.UInt64;
			}
			else if( addrmode & Register ) {
				if( !script->m_Regs[b.UInt64].UInt64 )
					script->m_Regs[b.UInt64].UInt64 = 1;
				script->m_Regs[a.UInt64].UInt64 /= script->m_Regs[b.UInt64].UInt64;
			}
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte ) {
					if( !*(script->m_Regs[b.UInt64].UCharPtr+offset) )
						*(script->m_Regs[b.UInt64].UCharPtr+offset) = 1;
					script->m_Regs[a.UInt64].UChar /= *(script->m_Regs[b.UInt64].UCharPtr+offset);
				}
				else if( addrmode & TwoBytes ) {
					if( !*(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset) )
						*(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset) = 1;
					script->m_Regs[a.UInt64].UShort /= *(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				}
				else if( addrmode & FourBytes ) {
					if( !*(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset) )
						*(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset) = 1;
					script->m_Regs[a.UInt64].UInt32 /= *(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				}
				else if( addrmode & EightBytes ) {
					if( !*(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset) )
						*(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset) = 1;
					script->m_Regs[a.UInt64].UInt64 /= *(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte ) {
					if( !*b.UCharPtr )
						*b.UCharPtr = 1;
					script->m_Regs[a.UInt64].UChar /= *b.UCharPtr;
				}
				else if( addrmode & TwoBytes ) {
					if( !*b.UShortPtr )
						*b.UShortPtr = 1;
					script->m_Regs[a.UInt64].UShort /= *b.UShortPtr;
				}
				else if( addrmode & FourBytes ) {
					if( !*b.UInt32Ptr )
						*b.UInt32Ptr = 1;
					script->m_Regs[a.UInt64].UInt32 /= *b.UInt32Ptr;
				}
				else if( addrmode & EightBytes ) {
					if( !*b.UInt64Ptr )
						*b.UInt64Ptr = 1;
					script->m_Regs[a.UInt64].UInt64 /= *b.UInt64Ptr;
				}
			}
			continue;
		}
		exec_udivm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( !b.UInt64 )
						b.UInt64 = 1;
					
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) /= b.UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) /= b.UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) /= b.UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) /= b.UInt64;
				}
				else if( addrmode & Register ) {
					if( !script->m_Regs[b.UInt64].UInt64 )
						script->m_Regs[b.UInt64].UInt64 = 1;
					
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) /= script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) /= script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) /= script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) /= script->m_Regs[b.UInt64].UInt64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( !b.UInt64 )
						b.UInt64 = 1;
					
					if( addrmode & Byte )
						*a.UCharPtr /= b.UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr /= b.UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr /= b.UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr /= b.UInt64;
				}
				else if( addrmode & Register ) {
					if( !script->m_Regs[b.UInt64].UInt64 )
						script->m_Regs[b.UInt64].UInt64 = 1;
					
					if( addrmode & Byte )
						*a.UCharPtr /= script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr /= script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr /= script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr /= script->m_Regs[b.UInt64].UInt64;
				}
			}
			continue;
		}
		
		exec_modr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate ) {
				if( !b.Int64 )
					b.Int64 = 1;
				script->m_Regs[a.UInt64].Int64 %= b.Int64;
			}
			if( addrmode & Register ) {
				if( !script->m_Regs[b.UInt64].Int64 )
					script->m_Regs[b.UInt64].Int64 = 1;
				script->m_Regs[a.UInt64].Int64 %= script->m_Regs[b.UInt64].Int64;
			}
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte ) {
					if( !*(script->m_Regs[b.UInt64].CharPtr+offset) )
						*(script->m_Regs[b.UInt64].CharPtr+offset) = 1;
					script->m_Regs[a.UInt64].Char %= *(script->m_Regs[b.UInt64].CharPtr+offset);
				}
				else if( addrmode & TwoBytes ) {
					if( !*(int16_t *)(script->m_Regs[b.UInt64].CharPtr+offset) )
						*(int16_t *)(script->m_Regs[b.UInt64].CharPtr+offset) = 1;
					script->m_Regs[a.UInt64].Short %= *(int16_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				}
				else if( addrmode & FourBytes ) {
					if( !*(int32_t *)(script->m_Regs[b.UInt64].CharPtr+offset) )
						*(int32_t *)(script->m_Regs[b.UInt64].CharPtr+offset) = 1;
					script->m_Regs[a.UInt64].Int32 %= *(int32_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				}
				else if( addrmode & EightBytes ) {
					if( !*(int64_t *)(script->m_Regs[b.UInt64].CharPtr+offset) )
						*(int64_t *)(script->m_Regs[b.UInt64].CharPtr+offset) = 1;
					script->m_Regs[a.UInt64].Int64 %= *(int64_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				}
			}
			else if( addrmode & (Direct|Immediate) ) {
				if( addrmode & Byte ) {
					if( !*b.CharPtr )
						*b.CharPtr = 1;
					script->m_Regs[a.UInt64].Char %= *b.CharPtr;
				}
				else if( addrmode & TwoBytes ) {
					if( !*b.ShortPtr )
						*b.ShortPtr = 1;
					script->m_Regs[a.UInt64].Short %= *b.ShortPtr;
				}
				else if( addrmode & FourBytes ) {
					if( !*b.Int32Ptr )
						*b.Int32Ptr = 1;
					script->m_Regs[a.UInt64].Int32 %= *b.Int32Ptr;
				}
				else if( addrmode & EightBytes ) {
					if( !*b.Int64Ptr )
						*b.Int64Ptr = 1;
					script->m_Regs[a.UInt64].Int64 %= *b.Int64Ptr;
				}
			}
			continue;
		}
		exec_modm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( !b.UInt64 )
						b.UInt64 = 1;
					
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].CharPtr+offset) %= b.Char;
					else if( addrmode & TwoBytes )
						*(int16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) %= b.Short;
					else if( addrmode & FourBytes )
						*(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) %= b.Int32;
					else if( addrmode & EightBytes )
						*(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) %= b.Int64;
				}
				else if( addrmode & Register ) {
					if( !script->m_Regs[b.UInt64].UInt64 )
						script->m_Regs[b.UInt64].UInt64 = 1;
					
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].CharPtr+offset) %= script->m_Regs[b.UInt64].Char;
					else if( addrmode & TwoBytes )
						*(int16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) %= script->m_Regs[b.UInt64].Short;
					else if( addrmode & FourBytes )
						*(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) %= script->m_Regs[b.UInt64].Int32;
					else if( addrmode & EightBytes )
						*(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) %= script->m_Regs[b.UInt64].Int64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( !b.UInt64 )
						b.UInt64 = 1;
					
					if( addrmode & Byte )
						*a.CharPtr %= b.Char;
					else if( addrmode & TwoBytes )
						*a.ShortPtr %= b.Short;
					else if( addrmode & FourBytes )
						*a.Int32Ptr %= b.Int32;
					else if( addrmode & EightBytes )
						*a.Int64Ptr %= b.Int64;
				}
				else if( addrmode & Register ) {
					if( !script->m_Regs[b.UInt64].UInt64 )
						script->m_Regs[b.UInt64].UInt64 = 1;
					
					if( addrmode & Byte )
						*a.CharPtr %= script->m_Regs[b.UInt64].Char;
					else if( addrmode & TwoBytes )
						*a.ShortPtr %= script->m_Regs[b.UInt64].Short;
					else if( addrmode & FourBytes )
						*a.Int32Ptr %= script->m_Regs[b.UInt64].Int32;
					else if( addrmode & EightBytes )
						*a.Int64Ptr %= script->m_Regs[b.UInt64].Int64;
				}
			}
			continue;
		}
		exec_umodr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate ) {
				if( !b.UInt64 )
					b.UInt64 = 1;
				script->m_Regs[a.UInt64].UInt64 %= b.UInt64;
			}
			else if( addrmode & Register ) {
				if( !script->m_Regs[b.UInt64].UInt64 )
					script->m_Regs[b.UInt64].UInt64 = 1;
				script->m_Regs[a.UInt64].UInt64 %= script->m_Regs[b.UInt64].UInt64;
			}
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte ) {
					if( !*(script->m_Regs[b.UInt64].UCharPtr+offset) )
						*(script->m_Regs[b.UInt64].UCharPtr+offset) = 1;
					script->m_Regs[a.UInt64].UChar %= *(script->m_Regs[b.UInt64].UCharPtr+offset);
				}
				else if( addrmode & TwoBytes ) {
					if( !*(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset) )
						*(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset) = 1;
					script->m_Regs[a.UInt64].UShort %= *(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				}
				else if( addrmode & FourBytes ) {
					if( !*(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset) )
						*(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset) = 1;
					script->m_Regs[a.UInt64].UInt32 %= *(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				}
				else if( addrmode & EightBytes ) {
					if( !*(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset) )
						*(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset) = 1;
					script->m_Regs[a.UInt64].UInt64 %= *(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte ) {
					if( !*b.UCharPtr )
						*b.UCharPtr = 1;
					script->m_Regs[a.UInt64].UChar %= *b.UCharPtr;
				}
				else if( addrmode & TwoBytes ) {
					if( !*b.UShortPtr )
						*b.UShortPtr = 1;
					script->m_Regs[a.UInt64].UShort %= *b.UShortPtr;
				}
				else if( addrmode & FourBytes ) {
					if( !*b.UInt32Ptr )
						*b.UInt32Ptr = 1;
					script->m_Regs[a.UInt64].UInt32 %= *b.UInt32Ptr;
				}
				else if( addrmode & EightBytes ) {
					if( !*b.UInt64Ptr )
						*b.UInt64Ptr = 1;
					script->m_Regs[a.UInt64].UInt64 %= *b.UInt64Ptr;
				}
			}
			continue;
		}
		exec_umodm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( !b.UInt64 )
						b.UInt64 = 1;
					
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) %= b.UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) %= b.UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) %= b.UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) %= b.UInt64;
				}
				else if( addrmode & Register ) {
					if( !script->m_Regs[b.UInt64].UInt64 )
						script->m_Regs[b.UInt64].UInt64 = 1;
					
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) %= script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) %= script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) %= script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) %= script->m_Regs[b.UInt64].UInt64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( !b.UInt64 )
						b.UInt64 = 1;
					
					if( addrmode & Byte )
						*a.UCharPtr %= b.UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr %= b.UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr %= b.UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr %= b.UInt64;
				}
				else if( addrmode & Register ) {
					if( !script->m_Regs[b.UInt64].UInt64 )
						script->m_Regs[b.UInt64].UInt64 = 1;
					
					if( addrmode & Byte )
						*a.UCharPtr %= script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr %= script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr %= script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr %= script->m_Regs[b.UInt64].UInt64;
				}
			}
			continue;
		}
		
		exec_shrr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_Regs[a.UInt64].UInt64 >>= b.UInt64;
			else if( addrmode & Register )
				script->m_Regs[a.UInt64].UInt64 >>= script->m_Regs[b.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].UChar >>= *(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].UShort >>= *(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].UInt32 >>= *(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].UInt64 >>= *(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].UChar >>= *b.UCharPtr;
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].UShort >>= *b.UShortPtr;
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].UInt32 >>= *b.UInt32Ptr;
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].UInt64 >>= *b.UInt64Ptr;
			}
			continue;
		}
		exec_shrm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) >>= b.UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) >>= b.UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) >>= b.UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) >>= b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) >>= script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) >>= script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) >>= script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) >>= script->m_Regs[b.UInt64].UInt64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*a.UCharPtr >>= b.UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr >>= b.UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr >>= b.UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr >>= b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*a.UCharPtr >>= script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr >>= script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr >>= script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr >>= script->m_Regs[b.UInt64].UInt64;
				}
			}
			continue;
		}
		
		exec_shlr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_Regs[a.UInt64].UInt64 <<= b.UInt64;
			else if( addrmode & Register )
				script->m_Regs[a.UInt64].UInt64 <<= script->m_Regs[b.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].UChar <<= *(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].UShort <<= *(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].UInt32 <<= *(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].UInt64 <<= *(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].UChar <<= *b.UCharPtr;
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].UShort <<= *b.UShortPtr;
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].UInt32 <<= *b.UInt32Ptr;
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].UInt64 <<= *b.UInt64Ptr;
			}
			continue;
		}
		exec_shlm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) <<= b.UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) <<= b.UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) <<= b.UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) <<= b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) <<= script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) <<= script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) <<= script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) <<= script->m_Regs[b.UInt64].UInt64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*a.UCharPtr <<= b.UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr <<= b.UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr <<= b.UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr <<= b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*a.UCharPtr <<= script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr <<= script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr <<= script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr <<= script->m_Regs[b.UInt64].UInt64;
				}
			}
			continue;
		}
		
		exec_andr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_Regs[a.UInt64].UInt64 &= b.UInt64;
			else if( addrmode & Register )
				script->m_Regs[a.UInt64].UInt64 &= script->m_Regs[b.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].UChar &= *(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].UShort &= *(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].UInt32 &= *(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].UInt64 &= *(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].UChar &= *b.UCharPtr;
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].UShort &= *b.UShortPtr;
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].UInt32 &= *b.UInt32Ptr;
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].UInt64 &= *b.UInt64Ptr;
			}
			continue;
		}
		exec_andm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) &= b.UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) &= b.UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) &= b.UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) &= b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) &= script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) &= script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) &= script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) &= script->m_Regs[b.UInt64].UInt64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*a.UCharPtr &= b.UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr &= b.UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr &= b.UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr &= b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*a.UCharPtr &= script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr &= script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr &= script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr &= script->m_Regs[b.UInt64].UInt64;
				}
			}
			continue;
		}
		
		exec_orr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_Regs[a.UInt64].UInt64 |= b.UInt64;
			else if( addrmode & Register )
				script->m_Regs[a.UInt64].UInt64 |= script->m_Regs[b.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].UChar |= *(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].UShort |= *(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].UInt32 |= *(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].UInt64 |= *(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].UChar |= *b.UCharPtr;
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].UShort |= *b.UShortPtr;
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].UInt32 |= *b.UInt32Ptr;
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].UInt64 |= *b.UInt64Ptr;
			}
			continue;
		}
		exec_orm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) |= b.UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) |= b.UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) |= b.UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) |= b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) |= script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) |= script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) |= script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) |= script->m_Regs[b.UInt64].UInt64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*a.UCharPtr |= b.UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr |= b.UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr |= b.UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr |= b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*a.UCharPtr |= script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr |= script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr |= script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr |= script->m_Regs[b.UInt64].UInt64;
				}
			}
			continue;
		}
		
		exec_xorr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_Regs[a.UInt64].UInt64 ^= b.UInt64;
			else if( addrmode & Register )
				script->m_Regs[a.UInt64].UInt64 ^= script->m_Regs[b.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].UChar ^= *(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].UShort ^= *(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].UInt32 ^= *(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].UInt64 ^= *(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_Regs[a.UInt64].UChar ^= *b.UCharPtr;
				else if( addrmode & TwoBytes )
					script->m_Regs[a.UInt64].UShort ^= *b.UShortPtr;
				else if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].UInt32 ^= *b.UInt32Ptr;
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].UInt64 ^= *b.UInt64Ptr;
			}
			continue;
		}
		exec_xorm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) ^= b.UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) ^= b.UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) ^= b.UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) ^= b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*(script->m_Regs[a.UInt64].UCharPtr+offset) ^= script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) ^= script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) ^= script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) ^= script->m_Regs[b.UInt64].UInt64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						*a.UCharPtr ^= b.UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr ^= b.UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr ^= b.UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr ^= b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						*a.UCharPtr ^= script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						*a.UShortPtr ^= script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						*a.UInt32Ptr ^= script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						*a.UInt64Ptr ^= script->m_Regs[b.UInt64].UInt64;
				}
			}
			continue;
		}
		
		exec_ltr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].Int64 < b.Int64;
			else if( addrmode & Register )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].Int64 < script->m_Regs[b.UInt64].Int64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Char < *(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Short < *(int16_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Int32 < *(int32_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Int64 < *(int64_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Char < *b.CharPtr;
				else if( addrmode & TwoBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Short < *b.ShortPtr;
				else if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Int32 < *b.Int32Ptr;
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Int64 < *b.Int64Ptr;
			}
			continue;
		}
		exec_ltm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *(script->m_Regs[a.UInt64].CharPtr+offset) < b.Char;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *(int16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) < b.Short;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) < b.Int32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) < b.Int64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *(script->m_Regs[a.UInt64].UCharPtr+offset) < script->m_Regs[b.UInt64].Char;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *(int16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) < script->m_Regs[b.UInt64].Short;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) < script->m_Regs[b.UInt64].Int32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) < script->m_Regs[b.UInt64].Int64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *a.CharPtr < b.Char;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *a.ShortPtr < b.Short;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.Int32Ptr < b.Int32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.Int64Ptr < b.Int64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *a.CharPtr < script->m_Regs[b.UInt64].Char;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *a.ShortPtr < script->m_Regs[b.UInt64].Short;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.Int32Ptr < script->m_Regs[b.UInt64].Int32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.Int64Ptr < script->m_Regs[b.UInt64].Int64;
				}
			}
			continue;
		}
		exec_ultr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt64 < b.UInt64;
			else if( addrmode & Register )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt64 < script->m_Regs[b.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UChar < *(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UShort < *(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt32 < *(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt64 < *(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UChar < *b.UCharPtr;
				else if( addrmode & TwoBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UShort < *b.UShortPtr;
				else if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt32 < *b.UInt32Ptr;
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt64 < *b.UInt64Ptr;
			}
			continue;
		}
		exec_ultm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *(script->m_Regs[a.UInt64].UCharPtr+offset) < b.UChar;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) < b.UShort;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) < b.UInt32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) < b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *(script->m_Regs[a.UInt64].UCharPtr+offset) < script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) < script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) < script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) < script->m_Regs[b.UInt64].UInt64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *a.UCharPtr < b.UChar;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *a.UShortPtr < b.UShort;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.UInt32Ptr < b.UInt32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.UInt64Ptr < b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *a.UCharPtr < script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *a.UShortPtr < script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.UInt32Ptr < script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.UInt64Ptr < script->m_Regs[b.UInt64].UInt64;
				}
			}
			continue;
		}
		
		exec_gtr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].Int64 > b.Int64;
			else if( addrmode & Register )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].Int64 > script->m_Regs[b.UInt64].Int64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Char > *(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Short > *(int16_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Int32 > *(int32_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Int64 > *(int64_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Char > *b.CharPtr;
				else if( addrmode & TwoBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Short > *b.ShortPtr;
				else if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Int32 > *b.Int32Ptr;
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Int64 > *b.Int64Ptr;
			}
			continue;
		}
		exec_gtm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *(script->m_Regs[a.UInt64].CharPtr+offset) > b.Char;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *(int16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) > b.Short;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) > b.Int32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) > b.Int64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *(script->m_Regs[a.UInt64].UCharPtr+offset) > script->m_Regs[b.UInt64].Char;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *(int16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) > script->m_Regs[b.UInt64].Short;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) > script->m_Regs[b.UInt64].Int32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) > script->m_Regs[b.UInt64].Int64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *a.CharPtr > b.Char;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *a.ShortPtr > b.Short;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.Int32Ptr > b.Int32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.Int64Ptr > b.Int64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *a.CharPtr > script->m_Regs[b.UInt64].Char;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *a.ShortPtr > script->m_Regs[b.UInt64].Short;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.Int32Ptr > script->m_Regs[b.UInt64].Int32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.Int64Ptr > script->m_Regs[b.UInt64].Int64;
				}
			}
			continue;
		}
		exec_ugtr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt64 > b.UInt64;
			else if( addrmode & Register )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt64 > script->m_Regs[b.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UChar > *(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UShort > *(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt32 > *(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt64 > *(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UChar > *b.UCharPtr;
				else if( addrmode & TwoBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UShort > *b.UShortPtr;
				else if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt32 > *b.UInt32Ptr;
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt64 > *b.UInt64Ptr;
			}
			break;
		}
		exec_ugtm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *(script->m_Regs[a.UInt64].UCharPtr+offset) > b.UChar;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) > b.UShort;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) > b.UInt32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) > b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *(script->m_Regs[a.UInt64].UCharPtr+offset) > script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) > script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) > script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) > script->m_Regs[b.UInt64].UInt64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *a.UCharPtr > b.UChar;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *a.UShortPtr > b.UShort;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.UInt32Ptr > b.UInt32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.UInt64Ptr > b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *a.UCharPtr > script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *a.UShortPtr > script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.UInt32Ptr > script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.UInt64Ptr > script->m_Regs[b.UInt64].UInt64;
				}
			}
			continue;
		}
		
		exec_cmpr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].Int64 == b.Int64;
			else if( addrmode & Register )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].Int64 == script->m_Regs[b.UInt64].Int64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Char == *(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Short == *(int16_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Int32 == *(int32_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Int64 == *(int64_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Char == *b.CharPtr;
				else if( addrmode & TwoBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Short == *b.ShortPtr;
				else if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Int32 == *b.Int32Ptr;
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Int64 == *b.Int64Ptr;
			}
			continue;
		}
		exec_cmpm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *(script->m_Regs[a.UInt64].CharPtr+offset) == b.Char;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *(int16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) == b.Short;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) == b.Int32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) == b.Int64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *(script->m_Regs[a.UInt64].UCharPtr+offset) == script->m_Regs[b.UInt64].Char;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *(int16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) == script->m_Regs[b.UInt64].Short;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) == script->m_Regs[b.UInt64].Int32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) == script->m_Regs[b.UInt64].Int64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *a.CharPtr == b.Char;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *a.ShortPtr == b.Short;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.Int32Ptr == b.Int32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.Int64Ptr == b.Int64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *a.CharPtr == script->m_Regs[b.UInt64].Char;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *a.ShortPtr == script->m_Regs[b.UInt64].Short;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.Int32Ptr == script->m_Regs[b.UInt64].Int32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.Int64Ptr == script->m_Regs[b.UInt64].Int64;
				}
			}
			continue;
		}
		exec_ucmpr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt64 == b.UInt64;
			else if( addrmode & Register )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt64 == script->m_Regs[b.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UChar == *(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UShort == *(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt32 == *(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt64 == *(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UChar == *b.UCharPtr;
				else if( addrmode & TwoBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UShort == *b.UShortPtr;
				else if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt32 == *b.UInt32Ptr;
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt64 == *b.UInt64Ptr;
			}
			continue;
		}
		exec_ucmpm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *(script->m_Regs[a.UInt64].UCharPtr+offset) == b.UChar;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) == b.UShort;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) == b.UInt32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) == b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *(script->m_Regs[a.UInt64].UCharPtr+offset) == script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) == script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) == script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) == script->m_Regs[b.UInt64].UInt64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *a.UCharPtr == b.UChar;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *a.UShortPtr == b.UShort;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.UInt32Ptr == b.UInt32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.UInt64Ptr == b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *a.UCharPtr == script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *a.UShortPtr == script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.UInt32Ptr == script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.UInt64Ptr == script->m_Regs[b.UInt64].UInt64;
				}
			}
			continue;
		}
		
		exec_neqr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].Int64 != b.Int64;
			else if( addrmode & Register )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].Int64 != script->m_Regs[b.UInt64].Int64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Char != *(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Short != *(int16_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Int32 != *(int32_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Int64 != *(int64_t *)(script->m_Regs[b.UInt64].CharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Char != *b.CharPtr;
				else if( addrmode & TwoBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Short != *b.ShortPtr;
				else if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Int32 != *b.Int32Ptr;
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Int64 != *b.Int64Ptr;
			}
			continue;
		}
		exec_neqm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *(script->m_Regs[a.UInt64].CharPtr+offset) != b.Char;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *(int16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) != b.Short;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) != b.Int32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) != b.Int64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *(script->m_Regs[a.UInt64].UCharPtr+offset) != script->m_Regs[b.UInt64].Char;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *(int16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) != script->m_Regs[b.UInt64].Short;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) != script->m_Regs[b.UInt64].Int32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) != script->m_Regs[b.UInt64].Int64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *a.CharPtr != b.Char;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *a.ShortPtr != b.Short;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.Int32Ptr != b.Int32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.Int64Ptr != b.Int64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *a.CharPtr != script->m_Regs[b.UInt64].Char;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *a.ShortPtr != script->m_Regs[b.UInt64].Short;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.Int32Ptr != script->m_Regs[b.UInt64].Int32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.Int64Ptr != script->m_Regs[b.UInt64].Int64;
				}
			}
			continue;
		}
		exec_uneqr:; {	// dest is reg, src is reg or memory
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt64 != b.UInt64;
			else if( addrmode & Register )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt64 != script->m_Regs[b.UInt64].UInt64;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Byte )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UChar != *(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & TwoBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UShort != *(uint16_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt32 != *(uint32_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt64 != *(uint64_t *)(script->m_Regs[b.UInt64].UCharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & Byte )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UChar != *b.UCharPtr;
				else if( addrmode & TwoBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UShort != *b.UShortPtr;
				else if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt32 != *b.UInt32Ptr;
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].UInt64 != *b.UInt64Ptr;
			}
			continue;
		}
		exec_uneqm:; {	// dest is memory and src is reg
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *(script->m_Regs[a.UInt64].UCharPtr+offset) != b.UChar;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) != b.UShort;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) != b.UInt32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) != b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *(script->m_Regs[a.UInt64].UCharPtr+offset) != script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *(uint16_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) != script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *(uint32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) != script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(uint64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset) != script->m_Regs[b.UInt64].UInt64;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *a.UCharPtr != b.UChar;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *a.UShortPtr != b.UShort;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.UInt32Ptr != b.UInt32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.UInt64Ptr != b.UInt64;
				}
				else if( addrmode & Register ) {
					if( addrmode & Byte )
						script->m_bZeroFlag = *a.UCharPtr != script->m_Regs[b.UInt64].UChar;
					else if( addrmode & TwoBytes )
						script->m_bZeroFlag = *a.UShortPtr != script->m_Regs[b.UInt64].UShort;
					else if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.UInt32Ptr != script->m_Regs[b.UInt64].UInt32;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.UInt64Ptr != script->m_Regs[b.UInt64].UInt64;
				}
			}
			continue;
		}
		exec_reset:; {	// reset ALL registers except rip, rsp, and rbp to 0.
			TaghaScript_Reset(script);
			continue;
		}
		
		exec_int2float:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate ) {
				b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
				script->m_Regs[a.UInt64].UInt64 = 0;
				script->m_Regs[a.UInt64].Float = (float) b.Int64;
			}
			else if( addrmode & Register ) {
				float temp = (float) script->m_Regs[a.UInt64].Int64;
				script->m_Regs[a.UInt64].UInt64 = 0;
				script->m_Regs[a.UInt64].Float = temp;
			}
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				float temp = (float) *(int32_t *)(script->m_Regs[a.UInt64].UCharPtr+offset);
				*(float *)(script->m_Regs[a.UInt64].UCharPtr+offset) = temp;
			}
			else if( addrmode & Direct ) {
				float temp = (float) *a.Int32Ptr;
				*a.FloatPtr = temp;
			}
			continue;
		}
		exec_int2dbl:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate ) {
				b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
				script->m_Regs[a.UInt64].UInt64 = 0;
				script->m_Regs[a.UInt64].Double = (double) b.UInt64;
			}
			else if( addrmode & Register ) {
				double temp = (double) script->m_Regs[a.UInt64].UInt64;
				script->m_Regs[a.UInt64].UInt64 = 0;
				script->m_Regs[a.UInt64].Double = temp;
			}
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				double temp = (double) *(int64_t *)(script->m_Regs[a.UInt64].UCharPtr+offset);
				*(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) = temp;
			}
			else if( addrmode & Direct ) {
				double temp = (double) *a.Int64Ptr;
				*a.DoublePtr = temp;
			}
			continue;
		}
		
		exec_float2dbl:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			double temp;
			if( addrmode & Immediate ) {
				b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
				temp = (double) b.Float;
				script->m_Regs[a.UInt64].UInt64 = 0;
				script->m_Regs[a.UInt64].Double = temp;
			}
			else if( addrmode & Register ) {
				temp = (double) script->m_Regs[a.UInt64].Float;
				script->m_Regs[a.UInt64].UInt64 = 0;
				script->m_Regs[a.UInt64].Double = temp;
			}
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				temp = (double) *(float *)(script->m_Regs[a.UInt64].UCharPtr+offset);
				*(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) = temp;
			}
			else if( addrmode & Direct ) {
				temp = (double) *a.FloatPtr;
				*a.DoublePtr = 0;
				*a.DoublePtr = temp;
			}
			continue;
		}
		exec_dbl2float:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			float temp;
			if( addrmode & Immediate ) {
				b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
				temp = (float) b.Double;
				script->m_Regs[a.UInt64].UInt64 = 0;
				script->m_Regs[a.UInt64].Float = temp;
			}
			else if( addrmode & Register ) {
				temp = (float) script->m_Regs[a.UInt64].Double;
				script->m_Regs[a.UInt64].UInt64 = 0;
				script->m_Regs[a.UInt64].Float = temp;
			}
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				temp = (float) *(double *)(script->m_Regs[a.UInt64].UCharPtr+offset);
				*(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) = 0;
				*(float *)(script->m_Regs[a.UInt64].UCharPtr+offset) = temp;
			}
			else if( addrmode & Direct ) {
				temp = (float) *a.DoublePtr;
				*a.FloatPtr = 0;
				*a.FloatPtr = temp;
			}
			continue;
		}
		
		exec_faddr:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_Regs[a.UInt64].Double += b.Double;
			else if( addrmode & Register )
				script->m_Regs[a.UInt64].Double += script->m_Regs[b.UInt64].Double;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].Float += *(float *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].Double += *(double *)(script->m_Regs[b.UInt64].CharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].Float += *b.FloatPtr;
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].Double += *b.DoublePtr;
			}
			continue;
		}
		exec_faddm:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & FourBytes ) {
						*(float *)(script->m_Regs[a.UInt64].UCharPtr+offset) += b.Float;
					}
					else if( addrmode & EightBytes ) {
						*(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) += b.Double;
					}
				}
				else if( addrmode & Register ) {
					if( addrmode & FourBytes )
						*(float *)(script->m_Regs[a.UInt64].UCharPtr+offset) += script->m_Regs[b.UInt64].Float;
					else if( addrmode & EightBytes )
						*(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) += script->m_Regs[b.UInt64].Double;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & FourBytes )
						*a.FloatPtr += b.Float;
					else if( addrmode & EightBytes )
						*a.DoublePtr += b.Double;
				}
				else if( addrmode & Register ) {
					if( addrmode & FourBytes )
						*a.FloatPtr += script->m_Regs[b.UInt64].Float;
					else if( addrmode & EightBytes )
						*a.DoublePtr += script->m_Regs[b.UInt64].Double;
				}
			}
			continue;
		}
		exec_fsubr:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_Regs[a.UInt64].Double -= b.Double;
			else if( addrmode & Register )
				script->m_Regs[a.UInt64].Double -= script->m_Regs[b.UInt64].Double;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].Float -= *(float *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].Double -= *(double *)(script->m_Regs[b.UInt64].CharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].Float -= *b.FloatPtr;
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].Double -= *b.DoublePtr;
			}
			continue;
		}
		exec_fsubm:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & FourBytes )
						*(float *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= b.Float;
					else if( addrmode & EightBytes )
						*(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= b.Double;
				}
				else if( addrmode & Register ) {
					if( addrmode & FourBytes )
						*(float *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= script->m_Regs[b.UInt64].Float;
					else if( addrmode & EightBytes )
						*(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) -= script->m_Regs[b.UInt64].Double;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & FourBytes )
						*a.FloatPtr -= b.Float;
					else if( addrmode & EightBytes )
						*a.DoublePtr -= b.Double;
				}
				else if( addrmode & Register ) {
					if( addrmode & FourBytes )
						*a.FloatPtr -= script->m_Regs[b.UInt64].Float;
					else if( addrmode & EightBytes )
						*a.DoublePtr -= script->m_Regs[b.UInt64].Double;
				}
			}
			continue;
		}
		exec_fmulr:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_Regs[a.UInt64].Double *= b.Double;
			else if( addrmode & Register )
				script->m_Regs[a.UInt64].Double *= script->m_Regs[b.UInt64].Double;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].Float *= *(float *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].Double *= *(double *)(script->m_Regs[b.UInt64].CharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & FourBytes )
					script->m_Regs[a.UInt64].Float *= *b.FloatPtr;
				else if( addrmode & EightBytes )
					script->m_Regs[a.UInt64].Double *= *b.DoublePtr;
			}
			continue;
		}
		exec_fmulm:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & FourBytes )
						*(float *)(script->m_Regs[a.UInt64].UCharPtr+offset) *= b.Float;
					else if( addrmode & EightBytes )
						*(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) *= b.Double;
				}
				else if( addrmode & Register ) {
					if( addrmode & FourBytes )
						*(float *)(script->m_Regs[a.UInt64].UCharPtr+offset) *= script->m_Regs[b.UInt64].Float;
					else if( addrmode & EightBytes )
						*(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) *= script->m_Regs[b.UInt64].Double;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & FourBytes )
						*a.FloatPtr *= b.Float;
					else if( addrmode & EightBytes )
						*a.DoublePtr *= b.Double;
				}
				else if( addrmode & Register ) {
					if( addrmode & FourBytes )
						*a.FloatPtr *= script->m_Regs[b.UInt64].Float;
					else if( addrmode & EightBytes )
						*a.DoublePtr *= script->m_Regs[b.UInt64].Double;
				}
			}
			continue;
		}
		exec_fdivr:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate ) {
				if( !b.Double )
					b.Double=1.0;
				script->m_Regs[a.UInt64].Double /= b.Double;
			}
			else if( addrmode & Register ) {
				if( !script->m_Regs[b.UInt64].Double )
					script->m_Regs[b.UInt64].Double=1.0;
				script->m_Regs[a.UInt64].Double /= script->m_Regs[b.UInt64].Double;
			}
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & FourBytes ) {
					if( !*(float *)(script->m_Regs[b.UInt64].CharPtr+offset) )
						*(float *)(script->m_Regs[b.UInt64].CharPtr+offset) = 1.f;
					script->m_Regs[a.UInt64].Float /= *(float *)(script->m_Regs[b.UInt64].CharPtr+offset);
				}
				else if( addrmode & EightBytes ) {
					if( !*(double *)(script->m_Regs[b.UInt64].CharPtr+offset) )
						*(double *)(script->m_Regs[b.UInt64].CharPtr+offset) = 1.0;
					script->m_Regs[a.UInt64].Double /= *(double *)(script->m_Regs[b.UInt64].CharPtr+offset);
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & FourBytes ) {
					if( !*b.FloatPtr )
						*b.FloatPtr = 1.f;
					script->m_Regs[a.UInt64].Float /= *b.FloatPtr;
				}
				else if( addrmode & EightBytes ) {
					if( !*b.DoublePtr )
						*b.DoublePtr = 1.0;
					script->m_Regs[a.UInt64].Double /= *b.DoublePtr;
				}
			}
			continue;
		}
		exec_fdivm:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & FourBytes ) {
						if( !b.Float )
							b.Float = 1.f;
						*(float *)(script->m_Regs[a.UInt64].UCharPtr+offset) /= b.Float;
					}
					else if( addrmode & EightBytes ) {
						if( !b.Double )
							b.Double = 1.0;
						*(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) /= b.Double;
					}
				}
				else if( addrmode & Register ) {
					if( addrmode & FourBytes ) {
						if( !b.Float )
							b.Float = 1.f;
						*(float *)(script->m_Regs[a.UInt64].UCharPtr+offset) /= script->m_Regs[b.UInt64].Float;
					}
					else if( addrmode & EightBytes ) {
						if( !b.Double )
							b.Double = 1.f;
						*(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) /= script->m_Regs[b.UInt64].Double;
					}
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & FourBytes ) {
						if( !b.Float )
							b.Float = 1.f;
						*a.FloatPtr /= b.Float;
					}
					else if( addrmode & EightBytes ) {
						if( !b.Double )
							b.Double = 1.0;
						*a.DoublePtr /= b.Double;
					}
				}
				else if( addrmode & Register ) {
					if( addrmode & FourBytes ) {
						if( !script->m_Regs[b.UInt64].Float )
							script->m_Regs[b.UInt64].Float = 1.f;
						*a.FloatPtr /= script->m_Regs[b.UInt64].Float;
					}
					else if( addrmode & EightBytes ) {
						if( !script->m_Regs[b.UInt64].Double )
							script->m_Regs[b.UInt64].Double = 1.0;
						*a.DoublePtr /= script->m_Regs[b.UInt64].Double;
					}
				}
			}
			continue;
		}
		exec_fneg:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Register )
				script->m_Regs[a.UInt64].Double = -script->m_Regs[a.UInt64].Double;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & FourBytes )
					*(float *)(script->m_Regs[a.UInt64].CharPtr+offset) = -*(float *)(script->m_Regs[a.UInt64].CharPtr+offset);
				else if( addrmode & EightBytes )
					*(double *)(script->m_Regs[a.UInt64].CharPtr+offset) = -*(double *)(script->m_Regs[a.UInt64].CharPtr+offset);
			}
			else if( addrmode & (Direct|Immediate) ) {
				if( addrmode & FourBytes )
					*a.FloatPtr = -*a.FloatPtr;
				else if( addrmode & EightBytes )
					*a.DoublePtr = -*a.DoublePtr;
			}
			continue;
		}
		exec_fltr:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].Double < b.Double;
			else if( addrmode & Register )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].Double < script->m_Regs[b.UInt64].Double;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Float < *(float *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Double < *(double *)(script->m_Regs[b.UInt64].CharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Float < *b.FloatPtr;
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Double < *b.DoublePtr;
			}
			continue;
		}
		exec_fltm:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & FourBytes )
						script->m_bZeroFlag = *(float *)(script->m_Regs[a.UInt64].UCharPtr+offset) < b.Float;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) < b.Double;
				}
				else if( addrmode & Register ) {
					if( addrmode & FourBytes )
						script->m_bZeroFlag = *(float *)(script->m_Regs[a.UInt64].UCharPtr+offset) < script->m_Regs[b.UInt64].Float;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) < script->m_Regs[b.UInt64].Double;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.FloatPtr < b.Float;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.DoublePtr < b.Double;
				}
				else if( addrmode & Register ) {
					if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.FloatPtr < script->m_Regs[b.UInt64].Float;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.DoublePtr < script->m_Regs[b.UInt64].Double;
				}
			}
			continue;
		}
		exec_fgtr:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].Double > b.Double;
			else if( addrmode & Register )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].Double > script->m_Regs[b.UInt64].Double;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Float > *(float *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Double > *(double *)(script->m_Regs[b.UInt64].CharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Float > *b.FloatPtr;
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Double > *b.DoublePtr;
			}
			continue;
		}
		exec_fgtm:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & FourBytes )
						script->m_bZeroFlag = *(float *)(script->m_Regs[a.UInt64].UCharPtr+offset) > b.Float;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) > b.Double;
				}
				else if( addrmode & Register ) {
					if( addrmode & FourBytes )
						script->m_bZeroFlag = *(float *)(script->m_Regs[a.UInt64].UCharPtr+offset) > script->m_Regs[b.UInt64].Float;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) > script->m_Regs[b.UInt64].Double;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.FloatPtr > b.Float;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.DoublePtr > b.Double;
				}
				else if( addrmode & Register ) {
					if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.FloatPtr > script->m_Regs[b.UInt64].Float;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.DoublePtr > script->m_Regs[b.UInt64].Double;
				}
			}
			continue;
		}
		exec_fcmpr:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].Double == b.Double;
			else if( addrmode & Register )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].Double == script->m_Regs[b.UInt64].Double;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Float == *(float *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Double == *(double *)(script->m_Regs[b.UInt64].CharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Float == *b.FloatPtr;
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Double == *b.DoublePtr;
			}
			continue;
		}
		exec_fcmpm:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & FourBytes )
						script->m_bZeroFlag = *(float *)(script->m_Regs[a.UInt64].UCharPtr+offset) == b.Float;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) == b.Double;
				}
				else if( addrmode & Register ) {
					if( addrmode & FourBytes )
						script->m_bZeroFlag = *(float *)(script->m_Regs[a.UInt64].UCharPtr+offset) == script->m_Regs[b.UInt64].Float;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) == script->m_Regs[b.UInt64].Double;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.FloatPtr == b.Float;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.DoublePtr == b.Double;
				}
				else if( addrmode & Register ) {
					if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.FloatPtr == script->m_Regs[b.UInt64].Float;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.DoublePtr == script->m_Regs[b.UInt64].Double;
				}
			}
			continue;
		}
		exec_fneqr:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & Immediate )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].Double != b.Double;
			else if( addrmode & Register )
				script->m_bZeroFlag = script->m_Regs[a.UInt64].Double != script->m_Regs[b.UInt64].Double;
			else if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Float != *(float *)(script->m_Regs[b.UInt64].CharPtr+offset);
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Double != *(double *)(script->m_Regs[b.UInt64].CharPtr+offset);
			}
			else if( addrmode & Direct ) {
				if( addrmode & FourBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Float != *b.FloatPtr;
				else if( addrmode & EightBytes )
					script->m_bZeroFlag = script->m_Regs[a.UInt64].Double != *b.DoublePtr;
			}
			continue;
		}
		exec_fneqm:; {
			a.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			b.UInt64 = *script->m_Regs[rip].UInt64Ptr++;
			if( addrmode & RegIndirect ) {
				offset = *script->m_Regs[rip].Int32Ptr++;
				if( addrmode & Immediate ) {
					if( addrmode & FourBytes )
						script->m_bZeroFlag = *(float *)(script->m_Regs[a.UInt64].UCharPtr+offset) != b.Float;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) != b.Double;
				}
				else if( addrmode & Register ) {
					if( addrmode & FourBytes )
						script->m_bZeroFlag = *(float *)(script->m_Regs[a.UInt64].UCharPtr+offset) != script->m_Regs[b.UInt64].Float;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *(double *)(script->m_Regs[a.UInt64].UCharPtr+offset) != script->m_Regs[b.UInt64].Double;
				}
			}
			else if( addrmode & Direct ) {
				if( addrmode & Immediate ) {
					if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.FloatPtr != b.Float;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.DoublePtr != b.Double;
				}
				else if( addrmode & Register ) {
					if( addrmode & FourBytes )
						script->m_bZeroFlag = *a.FloatPtr != script->m_Regs[b.UInt64].Float;
					else if( addrmode & EightBytes )
						script->m_bZeroFlag = *a.DoublePtr != script->m_Regs[b.UInt64].Double;
				}
			}
			continue;
		}
	}
	return 0;
}
