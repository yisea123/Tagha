
#ifndef TAGHA_HPP_INCLUDED
#define TAGHA_HPP_INCLUDED

#include "tagha.h"


struct TaghaScript_;
struct TaghaVM_;
struct NativeInfo_;

struct TaghaScript_ {
	char m_strName[64];	// script's name
	union CValue m_Regs[regsize];
	uint8_t
		*m_pMemory,	// stack and data stream. Used for stack and data segment
		*m_pText	// instruction stream.
	;
	char	**m_pstrNativeCalls;	// natives table as stored strings.
	struct hashmap
		*m_pmapFuncs,	// stores the functions compiled to script.
		*m_pmapGlobals	// stores global vars like string literals or variables.
	;
	uint32_t
		m_uiMemsize,	// size of m_pMemory
		m_uiInstrSize,	// size of m_pText
		m_uiMaxInstrs,	// max amount of instrs a script can execute.
		m_uiNatives,	// amount of natives the script uses.
		m_uiFuncs,	// how many functions the script has.
		m_uiGlobals	// how many globals variables the script has.
	;
	bool	m_bSafeMode : 1;	// does the script want bounds checking?
	bool	m_bDebugMode : 1;	// print debug info.
	bool	m_bZeroFlag : 1;	// conditional zero flag.
	
	void Delete();
	void PrintPtrs();
	void PrintMem();
	void PrintInstrs();
	void Reset();
	void *GetGlobalByName(const char *strGlobalName);
	bool BindGlobalPtr(const char *strGlobalName, void *pVar);
	void PushValue(const CValue value);
	CValue PopValue();
	uint32_t GetMemSize();
	uint32_t GetInstrSize();
	uint32_t GetMaxInstrs();
	uint32_t GetNativeCount();
	uint32_t GetFuncCount();
	uint32_t GetGlobalsCount();
	bool IsSafemodeActive();
	bool IsDebugActive();
};

struct TaghaVM_ {
	struct TaghaScript	*m_pScript;
	struct hashmap		*m_pmapNatives;
	
	TaghaVM_(void);
	void Delete();
	void LoadScriptByName(char *filename);
	bool RegisterNatives(NativeInfo_ arrNatives[]);
	int32_t CallScriptFunc(const char *strFunc);
	TaghaScript_ *GetScript();
	void SetScript(TaghaScript_ *script);
	void Exec(int argc, CValue argv[]);
	void LoadLibCNatives();
	void LoadSelfNatives();
};


typedef		void (*fnNative_)(struct TaghaScript_ *, union CValue [], union CValue *, const uint32_t, struct TaghaVM_ *);

struct NativeInfo_ {
	const char	*strName;	// use as string literals
	fnNative_	pFunc;
};


#endif	// TAGHA_HPP_INCLUDED

