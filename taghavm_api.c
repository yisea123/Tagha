
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "tagha.h"

void Tagha_Init(struct TaghaVM *pVM)
{
	if( !pVM )
		return;
	
	pVM->m_pScript = NULL;
	
	pVM->m_pmapNatives = calloc(1, sizeof(struct hashmap));
	if( !pVM->m_pmapNatives )
		printf("[%sTagha Init Error%s]: **** %sUnable to initialize Native Map%s ****\n", KRED, RESET, KGRN, RESET);
	else map_init(pVM->m_pmapNatives);
}

static bool is_c_file(const char *filename)
{
	if( !filename )
		return false;
	
	const char *p = filename;
	// iterate to end of string and then check backwards.
	while( *p )
		p++;
	return( (*(p-1)=='c' or *(p-1)=='C') and (*(p-2)=='.') );
}
static bool is_tbc_file(const char *filename)
{
	if( !filename )
		return false;
	
	const char *p = filename;
	// iterate to end of string and then check backwards.
	while( *p )
		p++;
	return( (*(p-3)=='t' and *(p-2)=='b' and *(p-1)=='c') or (*(p-3)=='T' and *(p-2)=='B' and *(p-1)=='C') and *(p-4)=='.' );
}


void Tagha_LoadScriptByName(struct TaghaVM *restrict pVM, char *restrict strFilename)
{
	if( !pVM )
		return;
	
	// allocate our pScript.
	pVM->m_pScript = TaghaScript_BuildFromFile(strFilename);
	
	struct TaghaScript *pScript = pVM->m_pScript;
	
	// do some initial libc setup for our pScript.
	// set up our standard I/O streams
	// and global self-referencing pScript pointer
	// Downside is that the pScript side host var MUST be a pointer.
	if( pScript and pScript->m_pmapGlobals ) {
		TaghaScript_BindGlobalPtr(pScript, "stdin", stdin);
		TaghaScript_BindGlobalPtr(pScript, "stderr", stderr);
		TaghaScript_BindGlobalPtr(pScript, "stdout", stdout);
		TaghaScript_BindGlobalPtr(pScript, "myself", pScript);
	}
}


void Tagha_Free(struct TaghaVM *pVM)
{
	if( !pVM )
		return;
	
	if( pVM->m_pScript )
		TaghaScript_Free(pVM->m_pScript), pVM->m_pScript=NULL;
	
	// since the VMs native hashmap has nothing allocated,
	// we just free the hashmap's internal data and then the hashmap itself.
	if( pVM->m_pmapNatives ) {
		map_free(pVM->m_pmapNatives);
		gfree((void **)&pVM->m_pmapNatives);
	}
}


bool Tagha_RegisterNatives(struct TaghaVM *pVM, struct NativeInfo arrNatives[])
{
	if( !pVM or !arrNatives )
		return false;
	else if( !pVM->m_pmapNatives )
		return false;
	
	for( NativeInfo *natives = arrNatives ; natives->pFunc and natives->strName ; natives++ )
		map_insert(pVM->m_pmapNatives, natives->strName, (uintptr_t)natives->pFunc);
	return true;
}

int32_t Tagha_CallScriptFunc(struct TaghaVM *restrict pVM, const char *restrict strFunc)
{
	// We need the VM system in order to call pScripts, why?
	// Because the exec function needs the VM system in order to check for native functions.
	if( !pVM or !pVM->m_pScript or !strFunc )
		return -1;
	
	struct TaghaScript *pScript = pVM->m_pScript;
	if( !pScript->m_pmapFuncs ) {
		TaghaScript_PrintErr(pScript, __func__, "Cannot call functions from host using a NULL function table!");
		return -1;
	}
	else if( ((pScript->m_Regs[rsp].UCharPtr-pScript->m_pMemory)-16) >= pScript->m_uiMemsize ) {
		TaghaScript_PrintErr(pScript, __func__, "stack overflow!");
		return -1;
	}
	
	struct FuncTable *pFuncTable = (struct FuncTable *)(uintptr_t) map_find(pScript->m_pmapFuncs, strFunc);
	if( !pFuncTable ) {
		TaghaScript_PrintErr(pScript, __func__, "Function \'%s\' doesn't exist!", strFunc);
		return -1;
	}
	
	// save return address.
	(--pScript->m_Regs[rsp].SelfPtr)->UInt64 = (uintptr_t)pScript->m_Regs[rip].UCharPtr+1;
	
	// jump to the function entry address.
	pScript->m_Regs[rip].UCharPtr = pScript->m_pMemory + pFuncTable->m_uiEntry;
	pFuncTable = NULL;
	
	// push bp and copy sp to bp.
	(--pScript->m_Regs[rsp].SelfPtr)->UInt64 = (uintptr_t)pScript->m_Regs[rbp].UCharPtr;
	pScript->m_Regs[rbp] = pScript->m_Regs[rsp];
	
	return Tagha_Exec(pVM, 0, NULL);
}

TaghaScript *Tagha_GetScript(const struct TaghaVM *pVM)
{
	return !pVM ? NULL : pVM->m_pScript;
}

void Tagha_SetScript(struct TaghaVM *pVM, struct TaghaScript *pScript)
{
	if( !pVM )
		return;
	pVM->m_pScript = pScript;
}

void gfree(void **ppPtr)
{
	if( *ppPtr != NULL ) {
		free(*ppPtr);
		*ppPtr = NULL;
	}
}

// Code from SourceMod
// Copyright (C) 2004-2015 AlliedModders LLC.  All rights reserved.
// Under GNU General Public License, version 3.0
#define LADJUST			0x00000001		/* left adjustment */
#define ZEROPAD			0x00000002		/* zero (as opposed to blank) pad */
#define UPPERDIGITS		0x00000004		/* make alpha digits uppercase */
#define NOESCAPE		0x00000008		/* do not escape strings (they are only escaped if a database connection is provided) */
#define LONGADJ			0x00000010		/* adjusting for longer values like "lli", etc. Added by Assyrianic */

#define to_digit(c)		((c) - '0')
#define is_digit(c)		((unsigned)to_digit(c) <= 9)

// minor edits is removing database string AND 'maxlen' is changed from a reference to a pointer.
static bool AddString(char **buf_p, size_t *maxlen, const char *string, int width, int prec, int flags)
{
	int size = 0;
	char *buf;
	static char nlstr[] = {'(','n','u','l','l',')','\0'};

	buf = *buf_p;

	if (string == NULL)
	{
		string = nlstr;
		prec = -1;
		flags |= NOESCAPE;
	}

	if (prec >= 0)
	{
		for (size = 0; size < prec; size++) 
		{
			if (string[size] == '\0')
			{
				break;
			}
		}
	}
	else
	{
		while (string[size++]);
		size--;
	}

	if (size > (int)*maxlen)
	{
		size = *maxlen;
	}

	width -= size;
	*maxlen -= size;

	while (size--)
	{
		*buf++ = *string++;
	}

	while ((width-- > 0) && *maxlen)
	{
		*buf++ = ' ';
		--*maxlen;
	}

	*buf_p = buf;
	return true;
}

#include <float.h>
#include <math.h>
void AddFloat(char **buf_p, size_t *maxlen, double fval, int width, int prec, int flags)
{
	int digits;					// non-fraction part digits
	double tmp;					// temporary
	char *buf = *buf_p;			// output buffer pointer
	int64_t val;				// temporary
	int sign = 0;				// 0: positive, 1: negative
	int fieldlength;			// for padding
	int significant_digits = 0;	// number of significant digits written
	const int MAX_SIGNIFICANT_DIGITS = 16;

	if (fval != fval)
	{
		AddString(buf_p, maxlen, "NaN", width, prec, flags | NOESCAPE);
		return;
	}

	// default precision
	if (prec < 0)
	{
		prec = 6;
	}

	// get the sign
	if (fval < 0)
	{
		fval = -fval;
		sign = 1;
	}

	// compute whole-part digits count
	digits = (int)log10(fval) + 1;

	// Only print 0.something if 0 < fval < 1
	if (digits < 1)
	{
		digits = 1;
	}

	// compute the field length
	fieldlength = digits + prec + ((prec > 0) ? 1 : 0) + sign;

	// minus sign BEFORE left padding if padding with zeros
	if (sign && *maxlen && (flags & ZEROPAD))
	{
		*buf++ = '-';
		--*maxlen;
	}

	// right justify if required
	if ((flags & LADJUST) == 0)
	{
		while ((fieldlength < width) && *maxlen)
		{
			*buf++ = (flags & ZEROPAD) ? '0' : ' ';
			width--;
			--*maxlen;
		}
	}

	// minus sign AFTER left padding if padding with spaces
	if (sign && maxlen && !(flags & ZEROPAD))
	{
		*buf++ = '-';
		--*maxlen;
	}

	// write the whole part
	tmp = pow(10.0, digits-1);
	while ((digits--) && *maxlen)
	{
		if (++significant_digits > MAX_SIGNIFICANT_DIGITS)
		{
			*buf++ = '0';
		}
		else
		{
			val = (int64_t)(fval / tmp);
			*buf++ = '0' + val;
			fval -= val * tmp;
			tmp *= 0.1;
		}
		--*maxlen;
	}

	// write the fraction part
	if (*maxlen && prec)
	{
		*buf++ = '.';
		--*maxlen;
	}

	tmp = pow(10.0, prec);

	fval *= tmp;
	while (prec-- && *maxlen)
	{
		if (++significant_digits > MAX_SIGNIFICANT_DIGITS)
		{
			*buf++ = '0';
		}
		else
		{
			tmp *= 0.1;
			val = (int64_t)(fval / tmp);
			*buf++ = '0' + val;
			fval -= val * tmp;
		}
		--*maxlen;
	}

	// left justify if required
	if (flags & LADJUST)
	{
		while ((fieldlength < width) && *maxlen)
		{
			// right-padding only with spaces, ZEROPAD is ignored
			*buf++ = ' ';
			width--;
			--*maxlen;
		}
	}

	// update parent's buffer pointer
	*buf_p = buf;
}

void AddBinary(char **buf_p, size_t *maxlen, uint64_t val, int width, int flags)
{
	char text[64];
	int digits;
	char *buf;

	digits = 0;
	do
	{
		if (val & 1)
		{
			text[digits++] = '1';
		}
		else
		{
			text[digits++] = '0';
		}
		val >>= 1;
	} while (val);

	buf = *buf_p;

	if (!(flags & LADJUST))
	{
		while (digits < width && *maxlen)
		{
			*buf++ = (flags & ZEROPAD) ? '0' : ' ';
			width--;
			--*maxlen;
		}
	}

	while (digits-- && *maxlen)
	{
		*buf++ = text[digits];
		width--;
		--*maxlen;
	}

	if (flags & LADJUST)
	{
		while (width-- && *maxlen)
		{
			*buf++ = (flags & ZEROPAD) ? '0' : ' ';
			--*maxlen;
		}
	}

	*buf_p = buf;
}

void AddUInt(char **buf_p, size_t *maxlen, uint64_t val, int width, int flags)
{
	char text[64];
	int digits;
	char *buf;

	digits = 0;
	do
	{
		text[digits++] = '0' + val % 10;
		val /= 10;
	} while (val);

	buf = *buf_p;

	if (!(flags & LADJUST))
	{
		while (digits < width && *maxlen)
		{
			*buf++ = (flags & ZEROPAD) ? '0' : ' ';
			width--;
			--*maxlen;
		}
	}

	while (digits-- && *maxlen)
	{
		*buf++ = text[digits];
		width--;
		--*maxlen;
	}

	if (flags & LADJUST)
	{
		while (width-- && *maxlen)
		{
			*buf++ = (flags & ZEROPAD) ? '0' : ' ';
			--*maxlen;
		}
	}

	*buf_p = buf;
}

void AddInt(char **buf_p, size_t *maxlen, int64_t val, int width, int flags)
{
	char text[64];
	int digits;
	int signedVal;
	char *buf;
	uint64_t unsignedVal;

	digits = 0;
	signedVal = val;
	if (val < 0)
	{
		/* we want the unsigned version */
		unsignedVal = llabs(val);
	}
	else
	{
		unsignedVal = val;
	}

	do
	{
		text[digits++] = '0' + unsignedVal % 10;
		unsignedVal /= 10;
	} while (unsignedVal);

	if (signedVal < 0)
	{
		text[digits++] = '-';
	}

	buf = *buf_p;

	if (!(flags & LADJUST))
	{
		while ((digits < width) && *maxlen)
		{
			*buf++ = (flags & ZEROPAD) ? '0' : ' ';
			width--;
			--*maxlen;
		}
	}

	while (digits-- && *maxlen)
	{
		*buf++ = text[digits];
		width--;
		--*maxlen;
	}

	if (flags & LADJUST)
	{
		while (width-- && *maxlen)
		{
			*buf++ = (flags & ZEROPAD) ? '0' : ' ';
			--*maxlen;
		}
	}

	*buf_p = buf;
}

void AddHex(char **buf_p, size_t *maxlen, uint64_t val, int width, int flags)
{
	char text[64];
	int digits;
	char *buf;
	char digit;
	int hexadjust;

	if (flags & UPPERDIGITS)
	{
		hexadjust = 'A' - '9' - 1;
	}
	else
	{
		hexadjust = 'a' - '9' - 1;
	}

	digits = 0;
	do 
	{
		digit = ('0' + val % 16);
		if (digit > '9')
		{
			digit += hexadjust;
		}

		text[digits++] = digit;
		val /= 16;
	} while(val);

	buf = *buf_p;

	if (!(flags & LADJUST))
	{
		while (digits < width && *maxlen)
		{
			*buf++ = (flags & ZEROPAD) ? '0' : ' ';
			width--;
			--*maxlen;
		}
	}

	while (digits-- && *maxlen)
	{
		*buf++ = text[digits];
		width--;
		--*maxlen;
	}

	if (flags & LADJUST)
	{
		while (width-- && *maxlen)
		{
			*buf++ = (flags & ZEROPAD) ? '0' : ' ';
			--*maxlen;
		}
	}

	*buf_p = buf;
}

// Modified from AddHex for printing octal values.
void AddOctal(char **buf_p, size_t *maxlen, uint64_t val, int width, int flags)
{
	char text[64];
	int digits;
	char *buf;
	char digit;
	int octadjust;

	digits = 0;
	do {
		text[digits++] = ('0' + val % 8);
		val /= 8;
	} while(val);

	buf = *buf_p;

	if (!(flags & LADJUST))
	{
		while (digits < width && *maxlen)
		{
			*buf++ = (flags & ZEROPAD) ? '0' : ' ';
			width--;
			--*maxlen;
		}
	}

	while (digits-- && *maxlen)
	{
		*buf++ = text[digits];
		width--;
		--*maxlen;
	}

	if (flags & LADJUST)
	{
		while (width-- && *maxlen)
		{
			*buf++ = (flags & ZEROPAD) ? '0' : ' ';
			--*maxlen;
		}
	}

	*buf_p = buf;
}

//		Edits done:
// void* array is changed to CValue.
// removed pPhrases, pOutPutLength, and pFailPhrase.
// Added extra formats for other data like '%p' for pointers.
int32_t gnprintf(char *buffer,
					size_t maxlen,
					const char *format,
					CValue params[],
					uint32_t numparams,
					uint32_t *restrict curparam)
{
	if (!buffer || !maxlen)
	{
		return -1;
	}

	int arg = 0;
	char *buf_p;
	char ch;
	int flags;
	int width;
	int prec;
	int n;
	char sign;
	const char *fmt;
	size_t llen = maxlen - 1;

	buf_p = buffer;
	fmt = format;

	while (true)
	{
		// run through the format string until we hit a '%' or '\0'
		for (ch = *fmt; llen && ((ch = *fmt) != '\0') && (ch != '%'); fmt++)
		{
			*buf_p++ = ch;
			llen--;
		}
		if ((ch == '\0') || (llen <= 0))
		{
			goto done;
		}

		// skip over the '%'
		fmt++;

		// reset formatting state
		flags = 0;
		width = 0;
		prec = -1;
		sign = '\0';

rflag:
		ch = *fmt++;
reswitch:
		switch(ch)
		{
			case '-':
			{
				flags |= LADJUST;
				goto rflag;
			}
			case '.':
			{
				n = 0;
				while(is_digit((ch = *fmt++)))
				{
					n = 10 * n + (ch - '0');
				}
				prec = (n < 0) ? -1 : n;
				goto reswitch;
			}
			case '0':
			{
				flags |= ZEROPAD;
				goto rflag;
			}
			case '1' ... '9':
			{
				n = 0;
				do
				{
					n = 10 * n + (ch - '0');
					ch = *fmt++;
				} while(is_digit(ch));
				width = n;
				goto reswitch;
			}
			case 'c': case 'C':
			{
				if (!llen)
				{
					goto done;
				}
				char c = params[*curparam].Char;
				++*curparam;
				*buf_p++ = c;
				llen--;
				arg++;
				break;
			}
			case 'b':
			{
				if( flags & LONGADJ )
					AddBinary(&buf_p, &llen, params[*curparam].Int64, width, flags);
				else AddBinary(&buf_p, &llen, params[*curparam].Int32, width, flags);
				++*curparam;
				arg++;
				break;
			}
			case 'd': case 'i':
			{
				if( flags & LONGADJ )
					AddInt(&buf_p, &llen, params[*curparam].Int64, width, flags);
				else AddInt(&buf_p, &llen, params[*curparam].Int32, width, flags);
				++*curparam;
				arg++;
				break;
			}
			case 'u':
			{
				if( flags & LONGADJ )
					AddUInt(&buf_p, &llen, params[*curparam].UInt64, width, flags);
				else AddUInt(&buf_p, &llen, params[*curparam].UInt32, width, flags);
				++*curparam;
				arg++;
				break;
			}
			case 'F': case 'E': case 'G':
			case 'f': case 'e': case 'g':
			{
				double value = params[*curparam].Double;
				++*curparam;
				AddFloat(&buf_p, &llen, value, width, prec, flags);
				arg++;
				break;
			}
			case 's':
			{
				const char *str = params[*curparam].String;
				++*curparam;
				AddString(&buf_p, &llen, str, width, prec, flags);
				arg++;
				break;
			}
			case 'X':
				flags |= UPPERDIGITS;
			case 'x':
			{
				if( flags & LONGADJ )
					AddHex(&buf_p, &llen, params[*curparam].UInt64, width, flags);
				else AddHex(&buf_p, &llen, params[*curparam].UInt32, width, flags);
				++*curparam;
				arg++;
				break;
			}
			case 'o': case 'O': {
				if( flags & LONGADJ )
					AddOctal(&buf_p, &llen, params[*curparam].UInt64, width, flags);
				else AddOctal(&buf_p, &llen, params[*curparam].UInt32, width, flags);
				++*curparam;
				arg++;
				break;
			}
			case 'P':
				flags |= UPPERDIGITS;
			case 'p': {
				uint64_t value = (uintptr_t)params[*curparam].Ptr;
				++*curparam;
				AddHex(&buf_p, &llen, value, width, flags);
				arg++;
				break;
			}
			case '%':
			{
				if (!llen)
				{
					goto done;
				}
				*buf_p++ = ch;
				llen--;
				break;
			}
			case 'L':
				flags |= UPPERDIGITS;
			case 'l':
				flags |= LONGADJ;
				goto rflag;
			
			case '\0':
			{
				if (!llen)
				{
					goto done;
				}
				*buf_p++ = '%';
				llen--;
				goto done;
			}
			default:
			{
				if (!llen)
				{
					goto done;
				}
				*buf_p++ = ch;
				llen--;
				break;
			}
		}
	}
	
done:
	*buf_p = '\0';
	
	return (maxlen - llen - 1);
}
/////////////////////////////////////////////////////////////////////////////////








