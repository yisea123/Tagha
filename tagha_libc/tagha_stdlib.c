
#include <stdlib.h>

/* void *malloc(size_t size); */
static void native_malloc(struct TaghaScript *pScript, union CValue params[], union CValue *restrict pRetval, const uint32_t argc, struct TaghaVM *pEnv)
{
	// size_t is 8 bytes on 64-bit systems
	const uint64_t ptrsize = params[0].UInt64;
	
	printf("native_malloc:: allocating size: %" PRIu64 "\n", ptrsize);
	void *p = malloc(ptrsize);
	if( p )
		puts("native_malloc:: pointer is VALID.\n");
	else puts("native_malloc:: returned \'p\' is NULL\n");
	pRetval->Ptr = p;
}

/* void free(void *ptr); */
static void native_free(struct TaghaScript *pScript, union CValue params[], union CValue *restrict pRetval, const uint32_t argc, struct TaghaVM *pEnv)
{
	void *ptr = params[0].Ptr;
	if( ptr ) {
		puts("native_free :: ptr is VALID, freeing...\n");
		free(ptr), ptr=NULL;
	}
}

/* void *calloc(size_t num, size_t size); */
static void native_calloc(struct TaghaScript *pScript, union CValue params[], union CValue *restrict pRetval, const uint32_t argc, struct TaghaVM *pEnv)
{
	const uint64_t num = params[0].UInt64;
	const uint64_t size = params[1].UInt64;
	
	pRetval->Ptr = calloc(num, size);
}

/* void *realloc(void *ptr, size_t size); */
static void native_realloc(struct TaghaScript *pScript, union CValue params[], union CValue *restrict pRetval, const uint32_t argc, struct TaghaVM *pEnv)
{
	void *ptr = params[0].Ptr;
	const uint64_t size = params[1].UInt64;
	
	pRetval->Ptr = realloc(ptr, size);
}

/* void srand(unsigned int seed); */
static void native_srand(struct TaghaScript *pScript, union CValue params[], union CValue *restrict pRetval, const uint32_t argc, struct TaghaVM *pEnv)
{
	srand(params[0].UInt32);
}

/* int rand(void); */
static void native_rand(struct TaghaScript *pScript, union CValue params[], union CValue *restrict pRetval, const uint32_t argc, struct TaghaVM *pEnv)
{
	pRetval->Int32 = rand();
}

/* double strtod(const char *str, char **endptr); */
static void native_strtod(struct TaghaScript *pScript, union CValue params[], union CValue *restrict pRetval, const uint32_t argc, struct TaghaVM *pEnv)
{
	const char *str = params[0].String;
	if( !str ) {
		puts("strtod reported :: **** param 'str' is NULL ****\n");
		return;
	}
	pRetval->Double = strtod(str, params[1].Ptr);
}

/* float strtof(const char *str, char **endptr); */
static void native_strtof(struct TaghaScript *pScript, union CValue params[], union CValue *restrict pRetval, const uint32_t argc, struct TaghaVM *pEnv)
{
	const char *str = params[0].String;
	if( !str ) {
		puts("strtof reported :: **** param 'str' is NULL ****\n");
		return;
	}
	pRetval->Float = strtof(str, params[1].Ptr);
}

/* long int strtol(const char *str, char **endptr, int base); */
static void native_strtol(struct TaghaScript *pScript, union CValue params[], union CValue *restrict pRetval, const uint32_t argc, struct TaghaVM *pEnv)
{
	const char *str = params[0].String;
	if( !str ) {
		puts("strtol reported :: **** param 'str' is NULL ****\n");
		return;
	}
	pRetval->Int32 = strtol(str, params[1].Ptr, params[2].Int32);
}

/* long long int strtoll(const char *str, char **endptr, int base); */
static void native_strtoll(struct TaghaScript *pScript, union CValue params[], union CValue *restrict pRetval, const uint32_t argc, struct TaghaVM *pEnv)
{
	const char *str = params[0].String;
	if( !str ) {
		puts("strtoll reported :: **** param 'str' is NULL ****\n");
		return;
	}
	pRetval->Int64 = strtoll(str, params[1].Ptr, params[2].Int32);
}

/* unsigned long int strtoul(const char *str, char **endptr, int base); */
static void native_strtoul(struct TaghaScript *pScript, union CValue params[], union CValue *restrict pRetval, const uint32_t argc, struct TaghaVM *pEnv)
{
	const char *str = params[0].String;
	if( !str ) {
		puts("strtoul reported :: **** param 'str' is NULL ****\n");
		return;
	}
	pRetval->UInt32 = strtoul(str, params[1].Ptr, params[2].Int32);
}

/* unsigned long long int strtoull(const char *str, char **endptr, int base); */
static void native_strtoull(struct TaghaScript *pScript, union CValue params[], union CValue *restrict pRetval, const uint32_t argc, struct TaghaVM *pEnv)
{
	const char *str = params[0].String;
	if( !str ) {
		puts("strtoull reported :: **** param 'str' is NULL ****\n");
		return;
	}
	pRetval->UInt64 = strtoll(str, params[1].Ptr, params[2].Int32);
}

/* void abort(void); */
static void native_abort(struct TaghaScript *pScript, union CValue params[], union CValue *restrict pRetval, const uint32_t argc, struct TaghaVM *pEnv)
{
	*pScript->m_Regs[rip].UCharPtr = halt;
}

/* void exit(int status); */
static void native_exit(struct TaghaScript *pScript, union CValue params[], union CValue *restrict pRetval, const uint32_t argc, struct TaghaVM *pEnv)
{
	*pScript->m_Regs[rip].UCharPtr = halt;
	printf("Exiting with status: %i\n", params[0].Int32);
}

/* int system(const char *command); */
static void native_system(struct TaghaScript *pScript, union CValue params[], union CValue *restrict pRetval, const uint32_t argc, struct TaghaVM *pEnv)
{
	const char *command = params[0].String;
	if( !command ) {
		puts("system reported :: **** param 'command' is NULL ****\n");
		pRetval->Int32 = -1;
		return;
	}
	pRetval->Int32 = system(command);
}



void Tagha_load_stdlib_natives(struct TaghaVM *pVM)
{
	if( !pVM )
		return;
	
	NativeInfo libc_stdlib_natives[] = {
		{"malloc", native_malloc},
		{"free", native_free},
		{"calloc", native_calloc},
		{"realloc", native_realloc},
		{"srand", native_srand},
		{"rand", native_rand},
		{"strtod", native_strtod},
		{"strtof", native_strtof},
		{"strtol", native_strtol},
		{"strtoll", native_strtoll},
		{"strtoul", native_strtoul},
		{"strtoull", native_strtoull},
		{"abort", native_abort},
		{"exit", native_exit},
		{"system", native_system},
		{NULL, NULL}
	};
	Tagha_RegisterNatives(pVM, libc_stdlib_natives);
}










