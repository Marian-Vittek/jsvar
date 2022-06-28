
// exported debug level
int jsVarDebugLevel = 0;

//#include "jsvar.c"

//////////////////////////////////////////////////////////////////////////
// jsvar version 0.7
// jsvar provides synchronization of C and Javascript (GUI) variables
// (C) Xrefactory s.r.o.
// (C) HUBECN LLC


#if _WIN32

// _WIN32
#ifdef _MSC_VER
// Remove annoying warnings
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
// No Warning on: unreferenced local variable
#pragma warning (disable: 4101)
// No Warning on: conversion from 'xxx' to 'yyy', possible loss of data
#pragma warning (disable: 4244)
// No Warning on: This function or variable may be unsafe.
#pragma warning (disable: 4996)
#endif

#include <tchar.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <malloc.h>
#include <new.h>
#include <Winsock2.h>
#include <ws2tcpip.h>
#include <errno.h>
#include <io.h>
#include <fcntl.h>
#define _SC_OPEN_MAX            1024
#define strncasecmp             _strnicmp
#define strcasecmp              _stricmp
#define va_copy(d,s)            ((d) = (s))
#define va_copy_end(a)          {}
#define snprintf                _snprintf

#define isblank(c)              ((c)==' ' || (c) =='\n' || c=='\t')
#define isprint(c)              ((c)>=0x20 && (c)!=0x7f)


#else

// !_WIN32
#include <netdb.h>
#include <unistd.h>
#include <netinet/tcp.h>
#if __GNUC__
// Remove annoying warnings
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wwrite-strings"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#define closesocket(x)                      close(x)
#define va_copy_end(a)                      va_end(a)

// if _WIN32
#endif


#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "jsvar.h"
#include "sglib.h"


/////////////////////////////////////////////////////////////////////////////////////
// Do not modify this part of code, instead define those things in jsvar.h !

// all memory allocation / freeing is going through the following macros
#ifndef JSVAR_ALLOC
#define JSVAR_ALLOC(p,t)            {(p) = (t*) malloc(sizeof(t)); if((p)==NULL) {printf("bbo: Out of memory\n"); assert(0); exit(1);}}
#define JSVAR_REALLOC(p,t)          {(p) = (t*) realloc((p), sizeof(t)); if((p)==NULL && (n)!=0) {printf("bbo: Out of memory\n"); assert(0); exit(1);}}
#define JSVAR_ALLOCC(p,n,t)         {(p) = (t*) malloc((n)*sizeof(t)); if((p)==NULL && (n)!=0) {printf("bbo: Out of memory\n"); assert(0); exit(1);}}
#define JSVAR_REALLOCC(p,n,t)       {(p) = (t*) realloc((p), (n)*sizeof(t)); if((p)==NULL && (n)!=0) {printf("bbo: Out of memory\n"); assert(0); exit(1);}}
#define JSVAR_ALLOC_SIZE(p,t,n)     {(p) = (t*) malloc(n); if((p)==NULL && (n)!=0) {printf("bbo: Out of memory\n"); assert(0); exit(1);}}
#define JSVAR_FREE(p)               {free(p); }
#endif

// Macro used to compute I/O buffer sizes when buffer needs to be expanded.
// If your memory allocator works better with specific sizes, you can specify it here.
// Next buffer size shall be approximately twice as large as previous one.
#ifndef JSVAR_NEXT_RECOMMENDED_BUFFER_SIZE
#define JSVAR_NEXT_RECOMMENDED_BUFFER_SIZE(size) (size*2+1);
#endif

// All messages written by jsVar are prefixed with the following string
#ifndef JSVAR_PRINT_PREFIX
#define JSVAR_PRINT_PREFIX()        "jsVar"
#endif

// If you want that undocumented jsVar functions have non static linkage
// define JSVAR_ALL_LIBRARIES macro
#ifdef JSVAR_ALL_LIBRARIES
#define JSVAR_STATIC
#else
#define JSVAR_STATIC                static
#endif

// initial size of dynamic strings, with string growing the size is doubled
#ifndef JSVAR_INITIAL_DYNAMIC_STRING_SIZE
#define JSVAR_INITIAL_DYNAMIC_STRING_SIZE   16
#endif

//////////////////////////////////////////////////////////////////////////////////////


#define JSVAR_PATH_MAX                      256

#define JSVAR_TMP_STRING_SIZE               256
#define JSVAR_LONG_LONG_TMP_STRING_SIZE     65536

#define JSVAR_MIN(x,y)                      ((x)<(y) ? (x) : (y))
#define JSVAR_MAX(x,y)                      ((x)<(y) ? (y) : (x))


#define JsVarInternalCheck(x) {                                             \
        if (! (x)) {                                                    \
            printf("%s: Error: Internal check %s failed at %s:%d\n", JSVAR_PRINT_PREFIX(), #x, __FILE__, __LINE__); \
            fflush(stdout); abort();									\
        }                                                               \
    }

#define JSVAR_CALLBACK_CALL(hook, command) {\
        int _i_; \
        for(_i_=(hook).i-1; _i_ >= 0; _i_--) {\
            jsVarCallBackHookFunType callBack = (hook).a[_i_]; \
            if (command) break;\
        }\
    }

//////////////////////////////////////////////////////////////////////////////////////////////////
// some auxiliary data types

struct baioOpenSocketFileData {
    int     ioDirection;
    int     sockFd;
    char    path[1];        // maybe MAX_PATH, the structure has to be allocated long enough to keep whole path
};

struct wsaioStrList {
	char				*s;
	struct wsaioStrList *next;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// forward decls
int baioMsgInProgress(struct baio *bb) ;

//////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: add prefix jsvar
static char *strDuplicate(char *s) {
    int     n;
    char    *res;
    if (s == NULL) return(NULL);
    n = strlen(s);
    JSVAR_ALLOCC(res, n+1, char);
    strcpy(res, s);
    return(res);
}

static int hexDigitCharToInt(int hx) {
    int res;
    if (hx >= '0' && hx <= '9') return(hx - '0');
    if (hx >= 'A' && hx <= 'F') return(hx - 'A' + 10);
    if (hx >= 'a' && hx <= 'f') return(hx - 'a' + 10);
    return(-1);
}

static int intDigitToHexChar(int x) {
    if (x >= 0 && x <= 9) return(x + '0');
    if (x >= 10 && x <= 15) return(x + 'A' - 10);
    return(-1);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
// base64 (Those two function are public domain software)

static unsigned char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};
static unsigned char *decoding_table = NULL;
static int mod_table[] = {0, 2, 1};


JSVAR_STATIC void jsVarBuildDecodingTable() {
    int i;
    JSVAR_ALLOCC(decoding_table, 256, unsigned char);
    for (i = 0; i < 64; i++) decoding_table[(unsigned char) encoding_table[i]] = i;
}


JSVAR_STATIC void jsVarBase64Cleanup() {
    free(decoding_table);
}


JSVAR_STATIC int jsVarBase64Encode(char *data, int input_length, char *encoded_data, int output_length) {
    int             i, j, olen;
    unsigned char   *d;

    d = (unsigned char *) data;

    olen = 4 * ((input_length + 2) / 3);
    if (olen >= output_length) return(-1);

    for (i = 0, j = 0; i < input_length;) {

        uint32_t octet_a = i < input_length ? d[i++] : 0;
        uint32_t octet_b = i < input_length ? d[i++] : 0;
        uint32_t octet_c = i < input_length ? d[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    for (i = 0; i < mod_table[input_length % 3]; i++)
        encoded_data[olen - 1 - i] = '=';

    encoded_data[olen] = 0;
    return(olen);
}


JSVAR_STATIC int jsVarBase64Decode(char *data, int input_length, char *decoded_data, int output_length) {
    int             i, j, olen;
    unsigned char   *d;

    d = (unsigned char *) data;
    if (decoding_table == NULL) jsVarBuildDecodingTable();

    if (input_length % 4 != 0) return(-1);

    olen = input_length / 4 * 3;
    if (d[input_length - 1] == '=') olen--;
    if (d[input_length - 2] == '=') olen--;

    if (olen > output_length) return(-1);

    for (i = 0, j = 0; i < input_length;) {

        uint32_t sextet_a = d[i] == '=' ? 0 & i++ : decoding_table[d[i++]];
        uint32_t sextet_b = d[i] == '=' ? 0 & i++ : decoding_table[d[i++]];
        uint32_t sextet_c = d[i] == '=' ? 0 & i++ : decoding_table[d[i++]];
        uint32_t sextet_d = d[i] == '=' ? 0 & i++ : decoding_table[d[i++]];

        uint32_t triple = (sextet_a << 3 * 6)
        + (sextet_b << 2 * 6)
        + (sextet_c << 1 * 6)
        + (sextet_d << 0 * 6);

        if (j < olen) decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < olen) decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < olen) decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
    }
    return(olen);
}


//////////////////////////////////////////////////////////////////////////////////////////////////
// simple resizeable strings

static void jsVarDstrInitContent(struct jsVarDstr *s) {
    if (s->allocationMethod == MSAM_PURE_MALLOC) {
        s->s = (char *) malloc(JSVAR_INITIAL_DYNAMIC_STRING_SIZE * sizeof(char));
    } else {
        JSVAR_ALLOCC(s->s, JSVAR_INITIAL_DYNAMIC_STRING_SIZE, char);
    }
    s->allocatedSize = JSVAR_INITIAL_DYNAMIC_STRING_SIZE;
    s->size = 0;
    s->s[0] = 0;
}

JSVAR_STATIC struct jsVarDstr *jsVarDstrCreate() {
    struct jsVarDstr *res;
    JSVAR_ALLOCC(res, 1, struct jsVarDstr);
    res->allocationMethod = MSAM_DEFAULT;
    jsVarDstrInitContent(res);
    return(res);
}

JSVAR_STATIC struct jsVarDstr *jsVarDstrCreatePureMalloc() {
    struct jsVarDstr *res;
    res = (struct jsVarDstr *) malloc(sizeof(*res));
    res->allocationMethod = MSAM_PURE_MALLOC;
    jsVarDstrInitContent(res);
    return(res);
}

JSVAR_STATIC void jsVarDstrFree(struct jsVarDstr **p) {
    if (p == NULL || *p == NULL) return;
    if ((*p)->allocationMethod == MSAM_PURE_MALLOC) {
        free((*p)->s);
        free(*p);
    } else {
        assert((*p)->allocationMethod == MSAM_DEFAULT);
        JSVAR_FREE((*p)->s);
        JSVAR_FREE(*p);
    }
    *p = NULL;
}

JSVAR_STATIC char *jsVarDstrGetStringAndReinit(struct jsVarDstr *s) {
    char *res;
    res = s->s;
    jsVarDstrInitContent(s);
    return(res);
}

JSVAR_STATIC void jsVarDstrInternalReallocateToSize(struct jsVarDstr *s) {
    if (s->allocationMethod == MSAM_PURE_MALLOC) {
        s->s = (char *) realloc(s->s, s->allocatedSize * sizeof(char));
        if (s->s == NULL) {
            printf("Out of memory\n");
            assert(0);
            exit(1);
        }
    } else {
        assert(s->allocationMethod == MSAM_DEFAULT);
        JSVAR_REALLOCC(s->s, s->allocatedSize, char);
    }
}

JSVAR_STATIC void jsVarDstrExpandToSize(struct jsVarDstr *s, int size) {
    if (size < s->allocatedSize) return;
    while (size >= s->allocatedSize) {
        // adjust allocated size
        s->allocatedSize = (s->allocatedSize * 4 + 1);
    }
    jsVarDstrInternalReallocateToSize(s);
}

JSVAR_STATIC struct jsVarDstr *jsVarDstrTruncateToSize(struct jsVarDstr *s, int size) {
    s->size = size;
    s->s[s->size] = 0;
    s->allocatedSize = size + 1;
    jsVarDstrInternalReallocateToSize(s);
    return(s);
}

JSVAR_STATIC struct jsVarDstr *jsVarDstrTruncate(struct jsVarDstr *s) {
    jsVarDstrTruncateToSize(s, s->size);
    return(s);
}

JSVAR_STATIC int jsVarDstrAddCharacter(struct jsVarDstr *s, int c) {
    jsVarDstrExpandToSize(s, s->size+1);
    s->s[(s->size)++] = c;
    s->s[s->size] = 0;
    return(1);
}

JSVAR_STATIC int jsVarDstrDeleteLastChar(struct jsVarDstr *s) {
    if (s->size == 0) return(0);
    s->size --;
    s->s[s->size] = 0;
    return(-1);
}

JSVAR_STATIC int jsVarDstrAppendData(struct jsVarDstr *s, char *appendix, int len) {
    if (len < 0) return(len);
    jsVarDstrExpandToSize(s, s->size+len+1);
    memcpy(s->s+s->size, appendix, len*sizeof(char));
    s->size += len;
    s->s[s->size] = 0;
    return(len);
}

JSVAR_STATIC int jsVarDstrAppend(struct jsVarDstr *s, char *appendix) {
    int len;
    len = strlen(appendix);
    jsVarDstrAppendData(s, appendix, len);
    return(len);
}

JSVAR_STATIC int jsVarDstrAppendVPrintf(struct jsVarDstr *s, char *fmt, va_list arg_ptr) {
    int         n, dsize;
    char        *d;
    va_list     arg_ptr_copy;

    n = 0;
    for(;;) {
        d = s->s+s->size;
        dsize = s->allocatedSize - s->size;
        if (dsize > 0) {
            va_copy(arg_ptr_copy, arg_ptr);
            n = vsnprintf(d, dsize, fmt, arg_ptr_copy);
#if _WIN32
            // TODO: Review this stuff
            while (n < 0) {
                jsVarDstrExpandToSize(s, s->allocatedSize * 2 + 1024);
                d = s->s+s->size;
                dsize = s->allocatedSize - s->size;
                n = vsnprintf(d, dsize, fmt, arg_ptr_copy);
            }
#endif
            JsVarInternalCheck(n >= 0);
            va_copy_end(arg_ptr_copy);
            if (n < dsize) break;                   // success
        }
        jsVarDstrExpandToSize(s, s->size+n+1);
    }
    s->size += n;
    return(n);
}

JSVAR_STATIC int jsVarDstrAppendPrintf(struct jsVarDstr *s, char *fmt, ...) {
    int             res;
    va_list         arg_ptr;

    va_start(arg_ptr, fmt);
    res = jsVarDstrAppendVPrintf(s, fmt, arg_ptr);
    va_end(arg_ptr);
    return(res);
}

JSVAR_STATIC int jsVarDstrAppendEscapedStringUsableInJavascriptEval(struct jsVarDstr *ss, char *s, int slen) {
    int                         i, c;
    
    for(i=0; i<slen; i++) {
        c = ((unsigned char*)s)[i];
        jsVarDstrAddCharacter(ss, '\\');
        jsVarDstrAddCharacter(ss, 'x');
        jsVarDstrAddCharacter(ss, intDigitToHexChar(c/16));
        jsVarDstrAddCharacter(ss, intDigitToHexChar(c%16));
    }
    return(slen*4);
}

JSVAR_STATIC int jsVarDstrAppendBase64EncodedData(struct jsVarDstr *ss, char *s, int slen) {
    int                         i, c, r;

    if (slen < 0) return(-1);
    if (slen == 0) return(0);

    jsVarDstrExpandToSize(ss, ss->size + slen*4/3 + 2);
    r = jsVarBase64Encode(s, slen, ss->s+ss->size, ss->allocatedSize-ss->size);
    if (r < 0) {
        printf("%s: %s:%d: Can't encode string\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        return(-1);
    }
    ss->size += r;
    assert(ss->size < ss->allocatedSize);
    ss->s[ss->size] = 0;
    return(r);
}

JSVAR_STATIC int jsVarDstrAppendBase64DecodedData(struct jsVarDstr *ss, char *s, int slen) {
    int                         i, c, r;

    if (slen < 0) return(-1);
    if (slen == 0) return(0);

    jsVarDstrExpandToSize(ss, ss->size + slen / 4 * 3);
    r = jsVarBase64Decode(s, slen, ss->s+ss->size, ss->allocatedSize-ss->size);
    if (r < 0) {
        printf("%s: %s:%d: Can't decode string\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        return(-1);
    }
    ss->size += r;
    assert(ss->size < ss->allocatedSize);
    ss->s[ss->size] = 0;
    return(r);
}

JSVAR_STATIC void jsVarDstrClear(struct jsVarDstr *s) {
    s->size = 0;
    (s->s)[s->size] = 0;
}

// flags: 0 == ALL, otherwise single occurence
JSVAR_STATIC int jsVarDstrReplace(struct jsVarDstr *s, char *str, char *byStr, int allOccurencesFlag) {
    struct jsVarDstr    *d;
    int                 i, slen, stlen;
    char                *ss, *cc;

    stlen = strlen(str);
    slen = s->size;
    ss = jsVarDstrGetStringAndReinit(s);
    for(i=0; i<slen-stlen+1; i++) {
        if (strncmp(ss+i, str, stlen) == 0) {
            jsVarDstrAppend(s, byStr);
            i += stlen - 1;
            if (allOccurencesFlag == 0) break;
        } else {
            jsVarDstrAddCharacter(s, ss[i]);
        }
    }
    if (i<slen) {
        jsVarDstrAppendData(s, ss+i+1, slen-i-1);
    }
    if (s->allocationMethod == MSAM_PURE_MALLOC) {
        free(ss);
    } else {
        JSVAR_FREE(ss);
    }
    return(s->size);
}

JSVAR_STATIC int jsVarDstrAppendFile(struct jsVarDstr *res, FILE *ff) {
    int     n, originalSize;

    originalSize = res->size;
    n = 1;
    while (n > 0) {
        jsVarDstrExpandToSize(res, res->size + 1024);
        n = fread(res->s+res->size, 1, res->allocatedSize-res->size-1, ff);
        res->size += n;
    }
    res->s[res->size] = 0;
    return(res->size - originalSize);
}

JSVAR_STATIC struct jsVarDstr *jsVarDstrCreateByVPrintf(char *fmt, va_list arg_ptr) {
    struct jsVarDstr    *res;
    va_list             arg_ptr_copy;

    va_copy(arg_ptr_copy, arg_ptr);
    res = jsVarDstrCreate();
    jsVarDstrAppendVPrintf(res, fmt, arg_ptr_copy);
    res = jsVarDstrTruncate(res);
    va_copy_end(arg_ptr_copy);
    return(res);
}

JSVAR_STATIC struct jsVarDstr *jsVarDstrCreateByPrintf(char *fmt, ...) {
    struct jsVarDstr   *res;
    va_list            arg_ptr;

    va_start(arg_ptr, fmt);
    res = jsVarDstrCreateByVPrintf(fmt, arg_ptr);
    va_end(arg_ptr);
    return(res);
}

JSVAR_STATIC struct jsVarDstr *jsVarDstrCreateFromCharPtr(char *s, int slen) {
    struct jsVarDstr   *res;

    if (slen < 0) return(NULL);
    res = jsVarDstrCreate();
    jsVarDstrAppendData(res, s, slen);
    return(res);
}

JSVAR_STATIC struct jsVarDstr *jsVarDstrCreateCopy(struct jsVarDstr *ss) {
    struct jsVarDstr   *res;
    res = jsVarDstrCreate();
    jsVarDstrAppendData(res, ss->s, ss->size);
    return(res);
}

JSVAR_STATIC struct jsVarDstr *jsVarDstrCreateByVFileLoad(int useCppFlag, char *fileNameFmt, va_list arg_ptr) {
    FILE                *ff;
    int                 r;
    char                fname[JSVAR_PATH_MAX];
    char                ccc[JSVAR_PATH_MAX+32];
    struct jsVarDstr    *res;

    r = vsnprintf(fname, sizeof(fname), fileNameFmt, arg_ptr);
    if (r < 0 || r >= (int) sizeof(fname)) {
        strcpy(fname+sizeof(fname)-5, "...");
        printf("%s: %s:%d: Error: File name too long %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, fname);
        return(NULL);
    }

    if (useCppFlag) {
#if _WIN32
        printf("%s: %s:%d: Error: file preprocessor is not available for Windows: %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, fname);
        return(NULL);
#else
        sprintf(ccc, "cpp -P %s", fname);
        ff = popen(ccc, "r");
#endif
    } else {
        ff = fopen(fname, "r");
    }
    if (ff == NULL) return(NULL);
    res = jsVarDstrCreate();
    jsVarDstrAppendFile(res, ff);
    fclose(ff);
    return(res);
}

JSVAR_STATIC struct jsVarDstr *jsVarDstrCreateByFileLoad(int useCppFlag, char *fileNameFmt, ...) {
    va_list             arg_ptr;
    struct jsVarDstr    *res;

    va_start(arg_ptr, fileNameFmt);
    res = jsVarDstrCreateByVFileLoad(useCppFlag, fileNameFmt, arg_ptr);
    va_end(arg_ptr);
    return(res);
}


#if JSVAR_USE_WCHAR_T
//////////////////////////////////////////////////////////////////////////////////////////////////
// simple resizeable wchar_t strings

static void jsVarWDstrInitContent(struct jsVarWDstr *s) {
    if (s->allocationMethod == MSAM_PURE_MALLOC) {
        s->s = (wchar_t *) malloc(JSVAR_INITIAL_DYNAMIC_STRING_SIZE * sizeof(wchar_t));
    } else {
        JSVAR_ALLOCC(s->s, JSVAR_INITIAL_DYNAMIC_STRING_SIZE, wchar_t);
    }
    s->allocatedSize = JSVAR_INITIAL_DYNAMIC_STRING_SIZE;
    s->size = 0;
    s->s[0] = 0;
}

JSVAR_STATIC struct jsVarWDstr *jsVarWDstrCreate() {
    struct jsVarWDstr *res;
    JSVAR_ALLOCC(res, 1, struct jsVarWDstr);
    res->allocationMethod = MSAM_DEFAULT;
    jsVarWDstrInitContent(res);
    return(res);
}

JSVAR_STATIC struct jsVarWDstr *jsVarWDstrCreatePureMalloc() {
    struct jsVarWDstr *res;
    res = (struct jsVarWDstr *) malloc(sizeof(*res));
    res->allocationMethod = MSAM_PURE_MALLOC;
    jsVarWDstrInitContent(res);
    return(res);
}

JSVAR_STATIC void jsVarWDstrFree(struct jsVarWDstr **p) {
    if (p == NULL || *p == NULL) return;
    if ((*p)->allocationMethod == MSAM_PURE_MALLOC) {
        free((*p)->s);
        free(*p);
    } else {
        assert((*p)->allocationMethod == MSAM_DEFAULT);
        JSVAR_FREE((*p)->s);
        JSVAR_FREE(*p);
    }
    *p = NULL;
}

JSVAR_STATIC wchar_t *jsVarWDstrGetStringAndReinit(struct jsVarWDstr *s) {
    wchar_t *res;
    res = s->s;
    jsVarWDstrInitContent(s);
    return(res);
}

static void jsVarWDstrInternalReallocateToSize(struct jsVarWDstr *s) {
    if (s->allocationMethod == MSAM_PURE_MALLOC) {
        s->s = (wchar_t *) realloc(s->s, s->allocatedSize * sizeof(wchar_t));
        if (s->s == NULL) {
            printf("Out of memory\n");
            assert(0);
            exit(1);
        }
    } else {
        assert(s->allocationMethod == MSAM_DEFAULT);
        JSVAR_REALLOCC(s->s, s->allocatedSize, wchar_t);
    }
}

JSVAR_STATIC void jsVarWDstrExpandToSize(struct jsVarWDstr *s, int size) {
    if (size < s->allocatedSize) return;
    while (size >= s->allocatedSize) {
        // adjust allocated size
        s->allocatedSize = (s->allocatedSize * 4 + 1);
    }
    jsVarWDstrInternalReallocateToSize(s);
}

JSVAR_STATIC struct jsVarWDstr *jsVarWDstrTruncateToSize(struct jsVarWDstr *s, int size) {
    s->size = size;
    s->s[s->size] = 0;
    s->allocatedSize = size + 1;
    jsVarWDstrInternalReallocateToSize(s);
    return(s);
}

JSVAR_STATIC struct jsVarWDstr *jsVarWDstrTruncate(struct jsVarWDstr *s) {
    jsVarWDstrTruncateToSize(s, s->size);
    return(s);
}

JSVAR_STATIC int jsVarWDstrAddCharacter(struct jsVarWDstr *s, int c) {
    jsVarWDstrExpandToSize(s, s->size+1);
    s->s[(s->size)++] = c;
    s->s[s->size] = 0;
    return(1);
}

JSVAR_STATIC int jsVarWDstrDeleteLastChar(struct jsVarWDstr *s) {
    if (s->size == 0) return(0);
    s->size --;
    s->s[s->size] = 0;
    return(-1);
}

JSVAR_STATIC int jsVarWDstrAppendData(struct jsVarWDstr *s, wchar_t *appendix, int len) {
    if (len < 0) return(len);
    jsVarWDstrExpandToSize(s, s->size+len+1);
    memcpy(s->s+s->size, appendix, len * sizeof(wchar_t));
    s->size += len;
    s->s[s->size] = 0;
    return(len);
}

JSVAR_STATIC int jsVarWDstrAppend(struct jsVarWDstr *s, wchar_t *appendix) {
    int len;
    len = wcslen(appendix);
    jsVarWDstrAppendData(s, appendix, len);
    return(len);
}

JSVAR_STATIC int jsVarWDstrAppendVPrintf(struct jsVarWDstr *s, wchar_t *fmt, va_list arg_ptr) {
    int         n, dsize;
    wchar_t        *d;
    va_list     arg_ptr_copy;

    n = 0;
	d = s->s+s->size;
	dsize = s->allocatedSize - s->size;
	va_copy(arg_ptr_copy, arg_ptr);
	n = vswprintf(d, dsize, fmt, arg_ptr_copy);
	// TODO: Review this stuff
	while (n < 0) {
		jsVarWDstrExpandToSize(s, s->allocatedSize * 2 + 1024);
		d = s->s+s->size;
		dsize = s->allocatedSize - s->size;
		va_copy_end(arg_ptr_copy);
		va_copy(arg_ptr_copy, arg_ptr);
		n = vswprintf(d, dsize, fmt, arg_ptr_copy);
	}
	JsVarInternalCheck(n >= 0);
	va_copy_end(arg_ptr_copy);
    s->size += n;
    return(n);
}

JSVAR_STATIC int jsVarWDstrAppendPrintf(struct jsVarWDstr *s, wchar_t *fmt, ...) {
    int             res;
    va_list         arg_ptr;

    va_start(arg_ptr, fmt);
    res = jsVarWDstrAppendVPrintf(s, fmt, arg_ptr);
    va_end(arg_ptr);
    return(res);
}

JSVAR_STATIC int jsVarWDstrAppendEscapedStringUsableInJavascriptEval(struct jsVarWDstr *ss, wchar_t *s, int slen) {
    int                         i, c;
    
    for(i=0; i<slen; i++) {
        c = ((wchar_t*)s)[i];
        jsVarWDstrAddCharacter(ss, '\\');
        jsVarWDstrAddCharacter(ss, 'x');
        jsVarWDstrAddCharacter(ss, intDigitToHexChar(c/4096%16));
        jsVarWDstrAddCharacter(ss, intDigitToHexChar(c/256%16));
        jsVarWDstrAddCharacter(ss, intDigitToHexChar(c/16%16));
        jsVarWDstrAddCharacter(ss, intDigitToHexChar(c%16));
    }
    return(slen*6);
}

#if 0
// not yet implemented
JSVAR_STATIC int jsVarWDstrAppendBase64EncodedData(struct jsVarWDstr *ss, wchar_t *s, int slen) {
    int                         i, c, r;

    if (slen < 0) return(-1);
    if (slen == 0) return(0);

    jsVarWDstrExpandToSize(ss, ss->size + slen*4/3 + 2);
    r = base64_encode(s, slen, ss->s+ss->size, ss->allocatedSize-ss->size);
    if (r < 0) {
        printf("%s: %s:%d: Can't encode string\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        return(-1);
    }
    ss->size += r;
    assert(ss->size < ss->allocatedSize);
    ss->s[ss->size] = 0;
    return(r);
}

JSVAR_STATIC int jsVarWDstrAppendBase64DecodedData(struct jsVarWDstr *ss, wchar_t *s, int slen) {
    int                         i, c, r;

    if (slen < 0) return(-1);
    if (slen == 0) return(0);

    jsVarWDstrExpandToSize(ss, ss->size + slen / 4 * 3);
    r = base64_decode(s, slen, ss->s+ss->size, ss->allocatedSize-ss->size);
    if (r < 0) {
        printf("%s: %s:%d: Can't decode string\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        return(-1);
    }
    ss->size += r;
    assert(ss->size < ss->allocatedSize);
    ss->s[ss->size] = 0;
    return(r);
}
#endif

JSVAR_STATIC void jsVarWDstrClear(struct jsVarWDstr *s) {
    s->size = 0;
    (s->s)[s->size] = 0;
}

// flags: 0 == ALL, otherwise single occurence
JSVAR_STATIC int jsVarWDstrReplace(struct jsVarWDstr *s, wchar_t *str, wchar_t *byStr, int allOccurencesFlag) {
    struct jsVarWDstr 	*d;
    int                 i, slen, stlen;
    wchar_t            	*ss, *cc;

    stlen = wcslen(str);
    slen = s->size;
    ss = jsVarWDstrGetStringAndReinit(s);
    for(i=0; i<slen-stlen+1; i++) {
        if (wcsncmp(ss+i, str, stlen) == 0) {
            jsVarWDstrAppend(s, byStr);
            i += stlen - 1;
            if (allOccurencesFlag == 0) break;
        } else {
            jsVarWDstrAddCharacter(s, ss[i]);
        }
    }
    if (i<slen) {
        jsVarWDstrAppendData(s, ss+i+1, slen-i-1);
    }
    if (s->allocationMethod == MSAM_PURE_MALLOC) {
        free(ss);
    } else {
        JSVAR_FREE(ss);
    }
    return(s->size);
}

JSVAR_STATIC int jsVarWDstrAppendFile(struct jsVarWDstr *res, FILE *ff) {
    int     n, originalSize;

    originalSize = res->size;
    n = 1;
    while (n > 0) {
        jsVarWDstrExpandToSize(res, res->size + 1024);
        n = fread((char*)(res->s+res->size), sizeof(wchar_t), res->allocatedSize-res->size-1, ff);
        res->size += n;
    }
    res->s[res->size] = 0;
    return(res->size - originalSize);
}

JSVAR_STATIC struct jsVarWDstr *jsVarWDstrCreateByVPrintf(wchar_t *fmt, va_list arg_ptr) {
    struct jsVarWDstr    *res;
    va_list             arg_ptr_copy;

    va_copy(arg_ptr_copy, arg_ptr);
    res = jsVarWDstrCreate();
    jsVarWDstrAppendVPrintf(res, fmt, arg_ptr_copy);
    res = jsVarWDstrTruncate(res);
    va_copy_end(arg_ptr_copy);
    return(res);
}

JSVAR_STATIC struct jsVarWDstr *jsVarWDstrCreateByPrintf(wchar_t *fmt, ...) {
    struct jsVarWDstr   *res;
    va_list            arg_ptr;

    va_start(arg_ptr, fmt);
    res = jsVarWDstrCreateByVPrintf(fmt, arg_ptr);
    va_end(arg_ptr);
    return(res);
}

JSVAR_STATIC struct jsVarWDstr *jsVarWDstrCreateFromCharPtr(wchar_t *s, int slen) {
    struct jsVarWDstr   *res;

    if (slen < 0) return(NULL);
    res = jsVarWDstrCreate();
    jsVarWDstrAppendData(res, s, slen);
    return(res);
}

JSVAR_STATIC struct jsVarWDstr *jsVarWDstrCreateCopy(struct jsVarWDstr *ss) {
    struct jsVarWDstr   *res;
    res = jsVarWDstrCreate();
    jsVarWDstrAppendData(res, ss->s, ss->size);
    return(res);
}

JSVAR_STATIC struct jsVarWDstr *jsVarWDstrCreateByVFileLoad(int useCppFlag, char *fileNameFmt, va_list arg_ptr) {
    FILE                *ff;
    int                 r;
    char                fname[JSVAR_PATH_MAX];
    char                ccc[JSVAR_PATH_MAX+32];
    struct jsVarWDstr    *res;

    r = vsnprintf(fname, sizeof(fname), fileNameFmt, arg_ptr);
    if (r < 0 || r >= (int) sizeof(fname)) {
        strcpy(fname+sizeof(fname)-5, "...");
        printf("%s: %s:%d: Error: File name too long %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, fname);
        return(NULL);
    }

    if (useCppFlag) {
#if _WIN32
        printf("%s: %s:%d: Error: file preprocessor is not available for Windows: %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, fname);
        return(NULL);
#else
        sprintf(ccc, "cpp -P %s", fname);
        ff = popen(ccc, "r");
#endif
    } else {
        ff = fopen(fname, "r");
    }
    if (ff == NULL) return(NULL);
    res = jsVarWDstrCreate();
    jsVarWDstrAppendFile(res, ff);
    fclose(ff);
    return(res);
}

JSVAR_STATIC struct jsVarWDstr *jsVarWDstrCreateByFileLoad(int useCppFlag, char *fileNameFmt, ...) {
    va_list             arg_ptr;
    struct jsVarWDstr    *res;

    va_start(arg_ptr, fileNameFmt);
    res = jsVarWDstrCreateByVFileLoad(useCppFlag, fileNameFmt, arg_ptr);
    va_end(arg_ptr);
    return(res);
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////
// callbacks

void jsVarCallBackClearHook(struct jsVarCallBackHook *h) {
    h->i = 0;
}

void jsVarCallBackFreeHook(struct jsVarCallBackHook *h) {
    h->i = 0;
    JSVAR_FREE(h->a);
    h->a = NULL;
    h->dim = 0;
}

int jsVarCallBackAddToHook(struct jsVarCallBackHook *h, void *ptr) {
    int i;

    i = h->i;
    if (i >= h->dim) {
        h->dim = h->dim * 2 + 1;
        JSVAR_REALLOCC(h->a, h->dim, jsVarCallBackHookFunType);
    }
    h->a[i] = (jsVarCallBackHookFunType) ptr;
    h->i ++;
    return(i);
}

static void jsVarCallBackRemoveIndexFromHook(struct jsVarCallBackHook *h, int i) {
    if (i < 0 || i >= h->i) return;
    for(i=i+1; i < h->i; i++) h->a[i-1] = h->a[i];
    h->i --;
}

void jsVarCallBackRemoveFromHook(struct jsVarCallBackHook *h, void *ptr) {
    int i;

    for(i=0; i<h->i && h->a[i] != ptr; i++) ;
    if (i < h->i) {
        jsVarCallBackRemoveIndexFromHook(h, i);
    }
}

// if src is NULL, allocate new copy of src itself
JSVAR_STATIC void jsVarCallBackCloneHook(struct jsVarCallBackHook *dst, struct jsVarCallBackHook *src) {
    jsVarCallBackHookFunType *a;

    if (src != NULL) *dst = *src;
    a = dst->a;
    JSVAR_ALLOCC(dst->a, dst->dim, jsVarCallBackHookFunType);
    memcpy(dst->a, a, dst->dim*sizeof(jsVarCallBackHookFunType));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// popens

#if ! _WIN32
static void closeAllFdsFrom(int fd0) {
    int maxd, i;

    maxd = sysconf(_SC_OPEN_MAX);
    if (maxd > 1024) maxd = 1024;
    for(i=fd0; i<maxd; i++) close(i);
}

static int createPipesForPopens(int *in_fd, int *out_fd, int *pin, int *pout) {

    if (out_fd != NULL) {
        if (pipe(pin) != 0) {
            printf("%s: %s:%d: Can't create output pipe\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            return(-1);
        }
        *out_fd = pin[1];
        // printf("pipe pin: %p %p: %d %d\n", pin, pin+1, pin[0], pin[1]);
    }
    if (in_fd != NULL) {
        if (pipe(pout) != 0) {
            printf("%s: %s:%d: Can't create input pipe\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            if (out_fd != NULL) {
                close(pin[0]);
                close(pin[1]);
            }
            return(-1);
        }
        *in_fd = pout[0];
        // printf("pipe pout: %p %p: %d %d\n", pout, pout+1, pout[0], pout[1]);
    }
    return(0);
}

static void closePopenPipes(int *in_fd, int *out_fd, int pin[2], int pout[2]) {
    if (out_fd != NULL) {
        close(pin[0]);
        close(pin[1]);
    }
    if (in_fd != NULL) {
        close(pout[0]);
        close(pout[1]);
    }
}

JSVAR_STATIC pid_t jsVarPopen2(char *command, int *in_fd, int *out_fd, int useBashFlag) {
    int                 pin[2], pout[2];
    pid_t               pid;
    int                 md;
    char                ccc[strlen(command)+10];

    if (createPipesForPopens(in_fd, out_fd, pin, pout) == -1) return(-1);

    pid = fork();

    if (pid < 0) {
        printf("%s:%s:%d: fork failed in popen2: %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
        closePopenPipes(in_fd, out_fd, pin, pout);
        return pid;
    }

    if (pid == 0) {

        if (out_fd != NULL) {
            close(pin[1]);
            dup2(pin[0], 0);
            close(pin[0]);
        } else {
            // do not inherit stdin, if no pipe is defined, close it.
            close(0);
        }

        // we do not want to loose completely stderr of the task. redirect it to a common file
        md = open("currentsubmsgs.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
        dup2(md, 2);
        close(md);

        if (in_fd != NULL) {
            close(pout[0]);
            dup2(pout[1], 1);
            close(pin[1]);
        } else {
            // if there is no pipe for stdout, join stderr.
            dup2(2, 1);
        }

        // close all remaining fds. This is important because otherwise files and pipes may remain open
        // until the new process terminates.
        closeAllFdsFrom(3);

        // Exec is better, because otherwise this process may be unkillable (we would kill the shell not the process itself).
        if (useBashFlag) {
            execlp("bash", "bash", "-c", command, NULL);
        } else {
            sprintf(ccc, "exec %s", command);
            execlp("sh", "sh", "-c", ccc, NULL);
        }
        // execlp(command, command, NULL);
        printf("%s:%s:%d: Exec failed in popen2: %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
        exit(1);
    }

    if (in_fd != NULL) {
        close(pout[1]);
    }
    if (out_fd != NULL) {
        close(pin[0]);
    }

    return pid;
}

JSVAR_STATIC int jsVarPopen2File(char *path, char *ioDirection) {
    char    command[strlen(path)+30];
    int     fd;
    int     r;

    if (strcmp(ioDirection, "r") == 0) {
        sprintf(command, "cat %s", path);
        r = jsVarPopen2(command, &fd, NULL, 1);
        if (r < 0) return(-1);
    } else if (strcmp(ioDirection, "w") == 0) {
        sprintf(command, "cat > %s", path);
        r = jsVarPopen2(command, NULL, &fd, 1);
        if (r < 0) return(-1);
    } else {
        printf("%s:%s:%d: Invalid ioDirection. Only r or w are acceopted values.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        return(-1);
    }
    return(fd);
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// assynchronous socket connection functions

JSVAR_STATIC char *jsVarSprintfIpAddress_st(long unsigned ip) {
    static char res[JSVAR_TMP_STRING_SIZE];
    sprintf(res, "%ld.%ld.%ld.%ld", (ip >> 0) & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff);
    return(res);
}

JSVAR_STATIC int jsVarSetFileNonBlocking(int fd) {
#if _WIN32
    return(-1);
#else
    unsigned    flg;
    // Set non-blocking 
    if((flg = fcntl(fd, F_GETFL, NULL)) < 0) { 
        printf("%s: %s:%d: Error on socket %d fcntl(..., F_GETFL) (%s)\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, fd, strerror(errno)); 
        return(-1);
    } 
    flg |= O_NONBLOCK; 
    if(fcntl(fd, F_SETFL, flg) < 0) { 
        printf("%s: %s:%d: Error on socket %d fcntl(..., F_SETFL) (%s)\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, fd, strerror(errno)); 
        return(-1);
    }
    return(0);
#endif
}

JSVAR_STATIC int jsVarSetSocketNonBlocking(int fd) {
#if _WIN32
    int             r;
    unsigned long   mode;
    mode = 1;
    r = ioctlsocket(fd, FIONBIO, &mode);
    if (r) return(-1);
    return(0);
#else
    return(jsVarSetFileNonBlocking(fd));
#endif
}

JSVAR_STATIC int jsVarSetFileBlocking(int fd) {
#if _WIN32
    return(-1);
#else
    unsigned    flg;
    // Set blocking 
    if((flg = fcntl(fd, F_GETFL, NULL)) < 0) { 
        printf("%s: %s:%d: Error on socket %d fcntl(..., F_GETFL) (%s)\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, fd, strerror(errno)); 
        return(-1);
    } 
    flg &= ~(O_NONBLOCK); 
    if(fcntl(fd, F_SETFL, flg) < 0) { 
        printf("%s: %s:%d: Error on socket %d fcntl(..., F_SETFL) (%s)\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, fd, strerror(errno)); 
        return(-1);
    }
    return(0);
#endif
}

JSVAR_STATIC int jsVarSetSocketBlocking(int fd) {
#if _WIN32
    int             r;
    unsigned long   mode;
    mode = 0; 
    r = ioctlsocket(fd, FIONBIO, &mode);
    if (r) return(-1);
    return(0);
#else
    return(jsVarSetFileBlocking(fd));
#endif
}

JSVAR_STATIC void jsVarTuneSocketForLatency(int fd) {
#if _WIN32
  DWORD one;
  one = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
#else
  int one;
  one = 1;
  setsockopt(fd, SOL_TCP, TCP_NODELAY, &one, sizeof(one));
  setsockopt(fd, SOL_TCP, TCP_QUICKACK, &one, sizeof(one));
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
// Baio stands for Basic Assynchronous I/O library

/////////////////////////////////////////////////////////////////////////////////////

#define BAIO_STATUSES_CLEARED_PER_TICK              0x0ffff0

#define BAIO_BLOCKED_FOR_READ_IN_SPECIAL_STATUS_MASK (      \
        0                                       \
        | BAIO_BLOCKED_FOR_READ_IN_TCPIP_LISTEN \
        | BAIO_BLOCKED_FOR_READ_IN_SSL_CONNECT  \
        | BAIO_BLOCKED_FOR_READ_IN_SSL_ACCEPT   \
        )
#define BAIO_BLOCKED_FOR_READ_STATUS_MASK (     \
        0                                       \
        | BAIO_BLOCKED_FOR_READ_IN_READ         \
        | BAIO_BLOCKED_FOR_READ_IN_SSL_READ     \
        | BAIO_BLOCKED_FOR_READ_IN_SSL_WRITE    \
        )
#define BAIO_BLOCKED_FOR_WRITE_IN_SPECIAL_STATUS_MASK (     \
        0                                           \
        | BAIO_BLOCKED_FOR_WRITE_IN_TCPIP_CONNECT   \
        | BAIO_BLOCKED_FOR_WRITE_IN_SSL_CONNECT     \
        | BAIO_BLOCKED_FOR_WRITE_IN_SSL_ACCEPT      \
        )
#define BAIO_BLOCKED_FOR_WRITE_STATUS_MASK (        \
        0                                           \
        | BAIO_BLOCKED_FOR_WRITE_IN_WRITE           \
        | BAIO_BLOCKED_FOR_WRITE_IN_SSL_WRITE       \
        | BAIO_BLOCKED_FOR_WRITE_IN_SSL_READ        \
        )

#define BAIO_BLOCKED_FOR_READ_STATUSES              (BAIO_BLOCKED_FOR_READ_IN_SPECIAL_STATUS_MASK | BAIO_BLOCKED_FOR_READ_STATUS_MASK)
#define BAIO_BLOCKED_FOR_WRITE_STATUSES             (BAIO_BLOCKED_FOR_WRITE_IN_SPECIAL_STATUS_MASK | BAIO_BLOCKED_FOR_WRITE_STATUS_MASK)

#define BAIO_MSGS_BUFFER_RESIZE_INCREASE_OFFSET     4
#define BAIO_MSGS_PREVIOUS_INDEX(b, i)              ((i)==0?b->msize-1:(i)-1)
#define BAIO_MSGS_NEXT_INDEX(b, i)                  ((i)+1==b->msize?0:(i)+1)

#define JSVAR_STATIC_STRINGS_RING_SIZE               64

#define BAIO_SEND_MAX_MESSAGE_LENGTH                1400    /* max length which can be sent within one send */
#define BAIO_SOCKET_FILE_BUFFER_SIZE                1024


#define BAIO_INIFINITY_INDEX                        (-1)

enum baioTypes {
    BAIO_TYPE_FD,                   // basic baio, fd is supplied from outside
    BAIO_TYPE_FD_SOCKET,            // basic baio, socket is supplied from outside
    BAIO_TYPE_FILE,                 // file i/o
    BAIO_TYPE_PIPED_FILE,           // piped file i/o
    BAIO_TYPE_PIPED_COMMAND,        // piped subprocess
    BAIO_TYPE_TCPIP_CLIENT,         // tcp ip client
    BAIO_TYPE_TCPIP_SERVER,         // tcp ip server
    BAIO_TYPE_TCPIP_SCLIENT,        // a client which has connected to tcp ip server
};

#if !( BAIO_USE_OPEN_SSL || WSAIO_USE_OPEN_SSL || JSVAR_USE_OPEN_SSL)

// Not using OpenSSL, redefined functions and data types provided by OpenSsl to dummy 
// or alternative implementations
#define SSL                                 void
#define SSL_CTX                             void
#define X509_STORE_CTX                      void
#define BIO         						void
#define X509       							void
#define EVP_PKEY    						void
#define BIO_new_mem_buf(...)				(NULL)
#define PEM_read_bio_X509(...)				(NULL)
#define X509_free(...)						(NULL)
#define BIO_free(...)						(NULL)
#define PEM_read_bio_PrivateKey(...)		(NULL)
#define EVP_PKEY_free(x)					{}
#define SSL_free(x)                         {}
#define SSL_load_error_strings()            {}
#define SSL_CTX_new(x)                      (NULL)
#define SSL_CTX_free(x)                     {}
#define SSL_CTX_use_certificate(...)		(-1)
#define SSL_CTX_use_PrivateKey(...)			(-1)
#define SSL_CTX_use_certificate_file(...)   (-1)
#define SSL_CTX_use_PrivateKey_file(...)    (-1)
#define SSL_set_tlsext_host_name(...)       (-1)
#undef SSL_CTX_set_mode
#define SSL_CTX_set_mode(...)               {}
#define SSL_new(x)                          (NULL)
#define SSL_set_fd(...)                     (0)
#define SSL_get_error(...)                  (-1)
#define SSL_connect(...)                    (-1)
#define SSL_read(...)                       (-1)
#define SSL_write(...)                      (-1)
#define SSL_pending(...)                    (0)
#define SSL_library_init()                  {}
#define SSL_accept(...)                     (-1)
#define ERR_print_errors_fp(x)              {}
#define ERR_clear_error()                   {}
#undef SSL_ERROR_WANT_READ
#define SSL_ERROR_WANT_READ                 1
#undef SSL_ERROR_WANT_WRITE
#define SSL_ERROR_WANT_WRITE                2

// endif to openSsl
#endif


static SSL_CTX  *baioSslContext;
// ! If you use OpenSsl you have to set following file names to your key and certificate files !
// ! or assign certificate strings to your certificates.                                       !
char     *baioSslServerKeyFile = NULL; // "server.key";
char     *baioSslServerCertFile = NULL; // "server.crt";
char     *baioSslServerCertPem = 
	"-----BEGIN CERTIFICATE-----\n"
	"MIIDYjCCAkqgAwIBAgIJAJ4o1tEEggimMA0GCSqGSIb3DQEBCwUAMEUxCzAJBgNV\n"
	"BAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX\n"
	"aWRnaXRzIFB0eSBMdGQwIBcNMjIwNjI3MTM0MDM2WhgPMjE1OTA1MjAxMzQwMzZa\n"
	"MEUxCzAJBgNVBAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJ\n"
	"bnRlcm5ldCBXaWRnaXRzIFB0eSBMdGQwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAw\n"
	"ggEKAoIBAQDBpoSqjn63T3M1RHc3eVUdY7p8MoDX0pBlw+9Tzhtj/A0oHtwpO6Lp\n"
	"V4iFaYDWVyuTZuR2P/SHbT6i8vcihibdA56U9hyefkZSCk/d9wwNL9RhZ51iK2JR\n"
	"9/+2Y+KpIUOBbRU/2z8qQAvA4XZD1vtuHYZsgC0Fjlcg7ijv3O2MQ7r1YnJAwIIA\n"
	"ej88o6VPAm2fSzE2QTqMVYh7ZyeoCCCQTtAj4zLwEyrLOTgysPB5javqlw327FDU\n"
	"fVLF8dUJbwvtqGiLWMcil19YJolBDPCmCCRTbqZRFB81ZM7xPxS8D+kDhSF55dbH\n"
	"FykT4QhemOn4qdZChjcJ77//c87CiQZPAgMBAAGjUzBRMB0GA1UdDgQWBBT39hTw\n"
	"axP4xig+j06HeqTGm5Mi4zAfBgNVHSMEGDAWgBT39hTwaxP4xig+j06HeqTGm5Mi\n"
	"4zAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQC25qzNYH4uOstM\n"
	"AH8ku6xo1UzSBZB6cZJGQkXy11NPMGV+16MCI9YppknlTvxNFY2JJRSEkKHgiZRC\n"
	"L7llj49DD+EwU/fLt3DrM0hremb4BxgB+elVfkgchKOkEmp97j0xUIdq9ROH2CZv\n"
	"/5VI8oyY2YDb/pEShxxSVeRRu3gEWIpnlA4HkBKpELzkZvq0SaCR8MknzGB5ApzZ\n"
	"4wgGer9Ww6u7AXflsOAtjJchBAIe6iTUlflUXhdBYUrgLPgXXSvjD7pY+eta7QFa\n"
	"EXsfuOksGMYA1iMFGOznXBleK0t57P9BShCTLN6bXbWRjsUDL+0d8geOk7ircYbY\n"
	"qLw0pmgF\n"
	"-----END CERTIFICATE-----\n"
        ;
char     *baioSslServerKeyPem = 
	"-----BEGIN PRIVATE KEY-----\n"
	"MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQDBpoSqjn63T3M1\n"
	"RHc3eVUdY7p8MoDX0pBlw+9Tzhtj/A0oHtwpO6LpV4iFaYDWVyuTZuR2P/SHbT6i\n"
	"8vcihibdA56U9hyefkZSCk/d9wwNL9RhZ51iK2JR9/+2Y+KpIUOBbRU/2z8qQAvA\n"
	"4XZD1vtuHYZsgC0Fjlcg7ijv3O2MQ7r1YnJAwIIAej88o6VPAm2fSzE2QTqMVYh7\n"
	"ZyeoCCCQTtAj4zLwEyrLOTgysPB5javqlw327FDUfVLF8dUJbwvtqGiLWMcil19Y\n"
	"JolBDPCmCCRTbqZRFB81ZM7xPxS8D+kDhSF55dbHFykT4QhemOn4qdZChjcJ77//\n"
	"c87CiQZPAgMBAAECggEBAIj1i5jRJR/iyjmfTa3nW2Jo2UjjsypxWv0OgaLE/6xM\n"
	"ZMW+Zbmn2wWrifvJbXyqtEARIn3bp8dsZUN8EXvSY4Qm/i6ejgkuh++YKXp0MorV\n"
	"DCFD1hTLWENe4fUOWg2CYCsWilPabaclGur0yt8aGkN8EEmGsdDfJhI9Pqi+mSd8\n"
	"t6BEjY3Xl5akbhRkI5K1M/90/KdpNFZ7s789Pi35tvuadP8f1RHxuy9Q2befb8i4\n"
	"Q+5SbeSo82LbWHoK87Nilx79WzSKr+cc7UUn+z+8a8oCH8btVBzvWzuUq+ramIuM\n"
	"qAavwHtYifKxQUhjkh5nFijxMD3omIqGqINj39yi8jECgYEA8f5Vyg4BFayM3Hcc\n"
	"xPrRebQFXyf+I0eUnQp4odwPJX0AA25X6gBtTQ1G7+qxC7Eb8aLnxLPEUTm2Vvr7\n"
	"CcmsZTXVXQzqpA23f8U6wXGPyr4FXRi2kTur54hZf41FsrcXwUPmqzeDOUyRhCAz\n"
	"Q0Jug5eCU8qsuRzR9ydmqBzQywcCgYEAzNvf+1tivlxa+1HjmQyGabePbnNNTfAL\n"
	"xcvTNAVbn4Mb5UtvjAVHAbU96kaicOcKs8EHudmF1dFt+MwTzAl48tQOQyDb4Mie\n"
	"TSEPu8WSPN/Xu4rR0z9Djey0Iv4/UcI4ziEUk9W395SsNOKYw03bB73RjCkeembf\n"
	"FLP+nk1KcHkCgYEArVQLZI8FTd3qgtq5+4jfUzmTA2YkzGYv1w+x+dUh8CsJQGvf\n"
	"glbN8vuIjL1gFEzGBBw3v5c3DSq2JLTd7FPMLC4T5fMjeV/tyBGflQDfCktykgzq\n"
	"bzn7Vfo+iHLKskgcNqyI4qf/UKI8NBPQQ+OoPo7dpWCsuGYhKdLJ363MCy8CgYEA\n"
	"mYqy9dIo0ESobHWUAMJCfDn4ZvBEoIWqTTXXtsXNRmEeJ13C3U+XSNBu94i5d6Wz\n"
	"f8bN454FkZzGsBNFQ0hWPqpxhh66rl+vRl/hSvtp//ZF22rQmWRxXY5r9U5aZw0L\n"
	"RnPE2Ij2ubnU2E598OQJpmO/Cy5GibdQvFOsIzoK8QECgYBBJhiCXcvbxgKxiyh+\n"
	"/emFRxgvEPqfQjuqiERXy705ks7D96DVaB7hAv3PMmWDlOVu2iPZtvFMCdSV7168\n"
	"wduW08OZ7ih0n6Zr7W9UKyE2sct5Nqvlo8yo/Ucqic+Mm/y6eLzP6XJZ6lz+V0u6\n"
	"JM5zrrRUGrZTpVzTeKUCMkg9VA==\n"
	"-----END PRIVATE KEY-----\n"
        ;
//static char       *baioSslTrustedCertFile = "trusted.pem";


static char jsVarStaticStringsRing[JSVAR_STATIC_STRINGS_RING_SIZE][JSVAR_TMP_STRING_SIZE];
static int  jsVarStaticStringsRingIndex = 0;

struct baio *baioTab[BAIO_MAX_CONNECTIONS];
int         baioTabMax;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

char *baioStaticRingGetTemporaryStringPtr() {
    char *res;
    res = jsVarStaticStringsRing[jsVarStaticStringsRingIndex];
    jsVarStaticStringsRingIndex  = (jsVarStaticStringsRingIndex+1) % JSVAR_STATIC_STRINGS_RING_SIZE;
    return(res);
}

void baioCharBufferDump(char *prefix, char *s, int n) {
    int i;
    printf("%s", prefix);
    for(i=0; i<n; i++) printf(" %02x", ((unsigned char*)s)[i]);
    printf("\n");
    fflush(stdout);
}


void baioWriteBufferDump(struct baioWriteBuffer *b) {
#define BAIO_DUMP_LINE_SIZE 100
    int                     si, i;
    struct baioMsg          *m;
    char                    bb[BAIO_DUMP_LINE_SIZE+10];

    printf("baioWriteBufferDump Start:\n");
    printf("msgs %d-%d of %d:", b->mi, b->mj, b->msize);
    for(i=b->mi; i!=b->mj; i=BAIO_MSGS_NEXT_INDEX(b,i)) {
        m = &b->m[i];
        printf("[%d,%d] ", m->startIndex, m->endIndex);
    }
    printf("\n");
    printf("buffer %d-%d-%d-%d of %d: \n%d: ", b->i, b->ij, b->j, b->jj, b->size, b->i);
    if (b->size != 0) {
        assert(b->i >= 0 && b->i <= b->size);
        assert(b->ij >= 0 && b->ij <= b->size);
        assert(b->j >= 0 && b->j <= b->size);
        assert(b->jj >= 0 && b->jj <= b->size);
    }

    for(i=si=b->i; i!=b->j; i=(i+1)%b->size) {
        if (i%BAIO_DUMP_LINE_SIZE == 0) {bb[BAIO_DUMP_LINE_SIZE]=0; printf("%d: %s\n", si, bb); si=i;}
        if (isprint((unsigned char)b->b[i])) bb[i%BAIO_DUMP_LINE_SIZE] = b->b[i];
        else bb[i%BAIO_DUMP_LINE_SIZE]='.';
    }
    bb[i%BAIO_DUMP_LINE_SIZE]=0; 
    printf("%d: %s\n", si, bb);
    printf("\nbaioWriteBufferDump End.\n");
#undef BAIO_DUMP_LINE_SIZE
}

static int baioFdIsSocket(struct baio *bb) {
    if (bb == NULL) return(0);
    switch (bb->baioType) {
    case BAIO_TYPE_FD:
    case BAIO_TYPE_FILE:
    case BAIO_TYPE_PIPED_FILE:
    case BAIO_TYPE_PIPED_COMMAND:
        return(0);
    case BAIO_TYPE_FD_SOCKET:
    case BAIO_TYPE_TCPIP_CLIENT:
    case BAIO_TYPE_TCPIP_SERVER:
    case BAIO_TYPE_TCPIP_SCLIENT:
        return(1);
    default:
        printf("%s: %s:%d: Unknown baio type %d\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, bb->baioType);
        return(0);
    }
}

/////////

static void baioWriteMsgsInit(struct baioWriteBuffer *b) {
    b->mi = b->mj = 0;
}

static struct baioMsg *wsaioNextMsg(struct baioWriteBuffer *b) {
    if (b->mi == b->mj) return(NULL);
    return(&b->m[b->mi]);
}

static void baioMsgMaybeActivateNextMsg(struct baio *bb) {
    struct baioMsg          *m;
    struct baioWriteBuffer  *b;

    if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return;

    b = &bb->writeBuffer;
    // skip a msg if any
    m = wsaioNextMsg(b);
    if (m == NULL) {
        // no message, check if shall wrap to the beginning of circular buffer
        if (b->i == b->ij && b->j < b->i) {
            b->i = 0;
            b->ij = b->j;
        }
    } else if (m->endIndex != BAIO_INIFINITY_INDEX) {
        // is the next message ready for uotput?
        // printf("checking for msg %d %d  <-> %d %d\n", b->i, b->ij, m->startIndex, m->endIndex); fflush(stdout);
        if (b->ij == m->startIndex) {
            b->ij = m->endIndex;
            b->mi = BAIO_MSGS_NEXT_INDEX(b, b->mi);
        } else if (b->i == b->ij) {
            b->i = m->startIndex;
            b->ij = m->endIndex;
            b->mi = BAIO_MSGS_NEXT_INDEX(b, b->mi);
        }
    } else {
        // we have only one message which is being created, if otherwise buffer is empty, skip the gap
        if (b->i == b->ij) {
            b->i = b->ij = m->startIndex;
        }
    }
}

void baioMsgsResizeToHoldSize(struct baio *bb, int newSize) {
    struct baioWriteBuffer  *b;
    int                     shift, usedlen;

    b = &bb->writeBuffer;
    if (b->msize == 0) {
        b->msize = newSize;
        if (jsVarDebugLevel > 20) printf("%s: %s:%d: Allocating msgs of write buffer of %p to %d\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, b, b->msize);
        JSVAR_REALLOCC(b->m, b->msize, struct baioMsg);
        b->mi = b->mj = 0;
    } else if (b->msize < newSize) {
        usedlen = b->msize - b->mi;
        shift = newSize - b->msize;
        b->msize += shift;
        if (jsVarDebugLevel > 20) printf("%s: %s:%d: Resizing msgs of write buffer of %p to %d\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, b, b->msize);
        JSVAR_REALLOCC(b->m, b->msize, struct baioMsg);
        memmove(b->m+b->mi+shift, b->m+b->mi, usedlen*sizeof(struct baioMsg));
        if (b->mj >= b->mi) b->mj += shift;
        b->mi += shift;
    }   
}

struct baioMsg *baioMsgPut(struct baio *bb, int startIndex, int endIndex) {
    int                     mjplus, mjminus;
    struct baioMsg          *m;
    struct baioWriteBuffer  *b;

    if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return(NULL);

    // this is very important, otherwise we may be blocked
    // do not allow zero sized messages
    if (startIndex == endIndex && endIndex != BAIO_INIFINITY_INDEX) return(NULL);

    b = &bb->writeBuffer;

    if (b->mi == b->mj) baioWriteMsgsInit(b);

    // check that previous message was finished
    if (baioMsgInProgress(bb)) {
        printf("%s:%s:%d: Error: Putting a message while previous one is not finished!\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    }

#if ! BAIO_EACH_WRITTEN_MSG_HAS_RECORD
    if (b->mi == b->mj) {
        // if there are no messages, just fiddle with i,j indexes
        // no message, maybe wrap to the beginning of circular buffer
        if (b->ij == startIndex && endIndex != BAIO_INIFINITY_INDEX) {
            if (startIndex > endIndex) {
                b->ij = b->jj;
            } else {
                b->ij = endIndex;
            }
            return(NULL);
        }
    }
#endif

    //  if there is no pending msg, reset msg "list"
    if (b->msize == 0) {
        baioMsgsResizeToHoldSize(bb, BAIO_MSGS_BUFFER_RESIZE_INCREASE_OFFSET);
    } else {
        mjplus = BAIO_MSGS_NEXT_INDEX(b,  b->mj);
        if (mjplus == b->mi) {
            // no (more) space in "msg" list resize
            baioMsgsResizeToHoldSize(bb, b->msize*2 + BAIO_MSGS_BUFFER_RESIZE_INCREASE_OFFSET);
        }
    }
    mjplus = BAIO_MSGS_NEXT_INDEX(b,  b->mj);

#if ! BAIO_EACH_WRITTEN_MSG_HAS_RECORD
    // if this is direct continuation of the previous msg, just join them
    // we can not do it if endIndex == BAIO_INIFINITY_INDEX, because such a message is in construction
    // and its startIndex can move
    if (b->mi != b->mj && endIndex != BAIO_INIFINITY_INDEX) {
        mjminus = BAIO_MSGS_PREVIOUS_INDEX(b, b->mj);
        m = &b->m[mjminus];
        if (m->endIndex == startIndex) {
            m->endIndex = endIndex;
            return(m);
        }
    }
#endif
    // add new message
    m = &b->m[b->mj];
    m->startIndex = startIndex;
    m->endIndex = endIndex;
    b->mj = mjplus;

#if BAIO_EACH_WRITTEN_MSG_HAS_RECORD
    // if each message has record, maybe we have to activate the one we have just added
    baioMsgMaybeActivateNextMsg(bb);
#endif
    return(m);
}

void baioMsgStartNewMessage(struct baio *bb) {
    baioMsgPut(bb, bb->writeBuffer.j, BAIO_INIFINITY_INDEX);
}

int baioMsgLastStartIndex(struct baio *bb) {
    struct baioWriteBuffer  *b;
    struct baioMsg          *m;

    b = &bb->writeBuffer;
    if (b->mi == b->mj) return(-1);
    m = &b->m[BAIO_MSGS_PREVIOUS_INDEX(b, b->mj)];
    return(m->startIndex);
}

int baioMsgInProgress(struct baio *bb) {
    struct baioWriteBuffer  *b;
    struct baioMsg          *m;

    b = &bb->writeBuffer;
    if (b->mi == b->mj) return(0);
    m = &b->m[BAIO_MSGS_PREVIOUS_INDEX(b, b->mj)];
    if (m->endIndex != BAIO_INIFINITY_INDEX) return(0);
    return(1);
}

void baioMsgRemoveLastMsg(struct baio *bb) {
    struct baioWriteBuffer  *b;
    struct baioMsg          *m, *mm;

    b = &bb->writeBuffer;
    if (b->mi == b->mj) return;
    b->mj = BAIO_MSGS_PREVIOUS_INDEX(b, b->mj);
    m = &b->m[b->mj];
    if (m->endIndex == BAIO_INIFINITY_INDEX || m->endIndex == b->j) {
        if (b->mi == b->mj) {
            b->j = b->ij;
        } else {
            mm = &b->m[BAIO_MSGS_PREVIOUS_INDEX(b, b->mj)];
            b->j = mm->endIndex;
            if (b->i < b->j && b->j < b->ij) {
                printf("%s: %s:%d: Internall error: baioMsgRemoveLastMsg: message was partially send out, can not be removed\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            }
        }
    }
}

int baioMsgResetStartIndexForNewMsgSize(struct baio *bb, int newSize) {
    struct baioWriteBuffer  *b;
    struct baioMsg          *m;

    if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return(-1);

    b = &bb->writeBuffer;
    if (newSize == 0) {
        // zero size message, simply remove it
        b->mj = BAIO_MSGS_PREVIOUS_INDEX(b, b->mj);
        return(-1);
    }

    assert(b->mi != b->mj);
    m = &b->m[BAIO_MSGS_PREVIOUS_INDEX(b, b->mj)];
    assert(m != NULL);
    assert(newSize <= bb->writeBuffer.j);
    m->startIndex = bb->writeBuffer.j - newSize;
    return(m->startIndex);
}

void baioMsgSend(struct baio *bb) {
    struct baioWriteBuffer  *b;
    struct baioMsg          *m;

    if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return;

    b = &bb->writeBuffer;
    assert(b->mi != b->mj);
    m = &b->m[BAIO_MSGS_PREVIOUS_INDEX(b, b->mj)];
    assert(m != NULL);
    m->endIndex = bb->writeBuffer.j;
    baioMsgMaybeActivateNextMsg(bb);
}


void baioMsgLastSetSize(struct baio *bb, int newSize) {
    baioMsgResetStartIndexForNewMsgSize(bb, newSize);
    baioMsgSend(bb);
}

void baioLastMsgDump(struct baio *bb) {
    struct baioWriteBuffer  *b;
    struct baioMsg          *m;

    b = &bb->writeBuffer;
    if (b->mi == b->mj) {
        printf("baioLastMsgDump: Message was joined\n");
        return;
    }
    m = &b->m[BAIO_MSGS_PREVIOUS_INDEX(b, b->mj)];
    printf("baioLastMsgDump: Sending msg %d - %d\n", m->startIndex, m->endIndex);
    printf(": offset+3:  %.*s\n", m->endIndex-m->startIndex-3, b->b+m->startIndex+3);
    // baioCharBufferDump(": ", b->b+m->startIndex, m->endIndex-m->startIndex);
}

//////////////////////////////////////////////////////////////////////////////////////////////////


#if 0
static void baioReadBufferInit(struct baioReadBuffer *b, int initSize) {
    if (initSize == 0) {
        FREE(b->b);
        b->b = NULL;
    } else {
        REALLOCC(b->b, initSize, char);
    }
    b->i = b->j = 0;
    b->size = initSize;
}
#endif

static void baioReadBufferFree(struct baioReadBuffer *b) {
    JSVAR_FREE(b->b);
    b->b = NULL;
    b->i = b->j = 0;
    b->size = 0;
}

int baioReadBufferResize(struct baioReadBuffer *b, int minSize, int maxSize) {
    if (b->size >= maxSize) {
        printf("%s: %s:%d: Error: buffer %p too long, can't resize over %d.\n", 
               JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, b, maxSize);
        return(-1);
    }
    b->size = JSVAR_NEXT_RECOMMENDED_BUFFER_SIZE(b->size);
    if (b->size < minSize) b->size = minSize;
    if (b->size >= maxSize) b->size = maxSize;
    JSVAR_REALLOCC(b->b, b->size, char);
    // Write message after actual resizing. Otherwise, this message can reinvoke resizing and enter into a loop
    if (jsVarDebugLevel > 20) printf("%s: resizing read buffer %p to %d of %d\n", JSVAR_PRINT_PREFIX(), b, b->size, maxSize);
    return(0);
}

static int baioReadBufferShift(struct baio *bb, struct baioReadBuffer *b) {
    int         delta;

    assert(b->i <= b->j);
    delta = b->i;
    memmove(b->b, b->b+delta, (b->j - delta) * sizeof(b->b[0]));
    b->j -= delta;
    b->i -= delta;
    return(delta);
}

#if 0
static void baioWriteBufferInit(struct baioWriteBuffer *b, int initSize) {
    if (initSize == 0) {
        JSVAR_FREE(b->b);
        b->b = NULL;
    } else {
        JSVAR_REALLOCC(b->b, initSize, char);
    }
    b->i = b->ij = b->j = b->jj = 0;
    b->size = initSize;
    if (b->m != NULL) JSVAR_FREE(b->m);
    b->m = NULL;
    b->msize = 0;
    baioWriteMsgsInit(b);
}
#endif

static void baioWriteBufferFree(struct baioWriteBuffer *b) {
    if (jsVarDebugLevel > 20) printf("%s: freeing write buffer %p b->b == %p.\n", JSVAR_PRINT_PREFIX(), b, b->b);
    JSVAR_FREE(b->b);
    b->b = NULL;
    b->i = b->ij = b->j = b->jj = 0;
    b->size = 0;
    if (b->m != NULL) JSVAR_FREE(b->m);
    b->m = NULL;
    b->msize = 0;
    baioWriteMsgsInit(b);   
}

static int baioWriteBufferResize(struct baioWriteBuffer *b, int minSize, int maxSize) {
    if (b->size >= maxSize) {
        printf("%s: %s:%d: Error: write buffer %p too long, can't resize over %d.", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, b, maxSize);
        return(-1);
    }
    b->size = JSVAR_NEXT_RECOMMENDED_BUFFER_SIZE(b->size);
    if (b->size < minSize) b->size = minSize;
    if (b->size >= maxSize) b->size = maxSize;
    JSVAR_REALLOCC(b->b, b->size, char);
    // Write message after actual resizing. Otherwise, this message can reinvoke resizing and enter into a loop
    if (jsVarDebugLevel > 20) printf("%s: resizing write buffer %p to %d of %d. b->b == %p.\n", JSVAR_PRINT_PREFIX(), b, b->size, maxSize, b->b);
    return(0);
}

static void baioSslDisconnect(struct baio *bb) {
    // it can happen that during normal connect we are disconneted even before starting SSL connection
    if (bb->sslHandle != NULL) {
        // For some reason calling SSL_shutdown on a broken session made next connection wrong
        // so I prefer not to close such connections
        // SSL_shutdown(bb->sslHandle);
        SSL_free(bb->sslHandle);
        bb->sslHandle = NULL;
    }
}

static void baioFreeZombie(struct baio *bb) {
    int i;

    i = bb->index;
    assert(bb == baioTab[i]);

    if (bb->readBuffer.i != bb->readBuffer.j) {
        printf("%s:%s:%d: Warning: zombie read.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    }
    if (bb->writeBuffer.i != bb->writeBuffer.j) {
        printf("%s:%s:%d: Warning: zombie write.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    }
    baioReadBufferFree(&bb->readBuffer);
    baioWriteBufferFree(&bb->writeBuffer);

    // free all hooks
    jsVarCallBackFreeHook(&bb->callBackOnRead);
    jsVarCallBackFreeHook(&bb->callBackOnWrite);
    jsVarCallBackFreeHook(&bb->callBackOnError);
    jsVarCallBackFreeHook(&bb->callBackOnEof);
    jsVarCallBackFreeHook(&bb->callBackOnDelete);
    jsVarCallBackFreeHook(&bb->callBackOnReadBufferShift);
    jsVarCallBackFreeHook(&bb->callBackOnTcpIpConnect);
    jsVarCallBackFreeHook(&bb->callBackOnConnect);
    jsVarCallBackFreeHook(&bb->callBackOnAccept);
    jsVarCallBackFreeHook(&bb->callBackAcceptFilter);
    jsVarCallBackFreeHook(&bb->callBackOnTcpIpAccept);

    if (bb->sslSniHostName != NULL) JSVAR_FREE(bb->sslSniHostName);
    bb->sslSniHostName = NULL;
    JSVAR_FREE(bb); 
    baioTab[i] = NULL;
}

static void baioCloseFd(struct baio *bb) {
    if (baioFdIsSocket(bb)) closesocket(bb->fd); else close(bb->fd);
}

static int baioImmediateDeactivate(struct baio *bb) {
    // zombie will be cleared at the next cycle
    bb->status = BAIO_STATUS_ZOMBIE;
    bb->baioMagic = 0;
    // soft clean buffers
    bb->readBuffer.i = bb->readBuffer.j = 0;
    bb->writeBuffer.i = bb->writeBuffer.ij = bb->writeBuffer.j = bb->writeBuffer.jj = 0;
    bb->writeBuffer.mi = bb->writeBuffer.mj = 0;
    bb->sslPending = 0;

    if (bb->fd < 0) return(-1);
    if (bb->sslHandle != NULL) baioSslDisconnect(bb);
    if (bb->baioType != BAIO_TYPE_FD && bb->baioType != BAIO_TYPE_FD_SOCKET) {
        baioCloseFd(bb);
    }
    // we have to call callback befre reseting fd to -1, because delete call back may need it
    JSVAR_CALLBACK_CALL(bb->callBackOnDelete, callBack(bb));
    bb->fd = -1;

    return(0);
}

int baioCloseOnError(struct baio *bb) {
    JSVAR_CALLBACK_CALL(bb->callBackOnError, callBack(bb));
    baioImmediateDeactivate(bb);
    return(-1);
}

struct baio *baioFromMagic(int baioMagic) {
    int             i;
    struct baio     *bb;
    // baioMagic can never be 0
    if (baioMagic == 0) return(NULL);
    i = (baioMagic % BAIO_MAX_CONNECTIONS);
    bb = baioTab[i];
    if (bb == NULL) return(NULL);
    if (bb->baioMagic != baioMagic) return(NULL);
    if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return(NULL);
    if ((bb->status & BAIO_STATUS_PENDING_CLOSE) != 0) return(NULL);
    return(bb);
}

/////////////////////////////////////////////////////////////////////////////////

static int baioTabFindUnusedEntryIndex(int baioStructType) {
    int i;

    for(i=0; i<baioTabMax && baioTab[i] != NULL; i++) ;

    if (i >= baioTabMax) {
        if (i >= BAIO_MAX_CONNECTIONS) {
            printf("%s: %s:%d: Error: Can't allocate baio. Too many connections\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            return(-1);
        }
        baioTabMax = i+1;
    }
    return(i);
}

static void cloneCallBackHooks(struct baio *bb, struct baio *parent) {
    jsVarCallBackCloneHook(&bb->callBackOnRead, &parent->callBackOnRead);
    jsVarCallBackCloneHook(&bb->callBackOnWrite, &parent->callBackOnWrite);
    jsVarCallBackCloneHook(&bb->callBackOnError, &parent->callBackOnError);
    jsVarCallBackCloneHook(&bb->callBackOnEof, &parent->callBackOnEof);
    jsVarCallBackCloneHook(&bb->callBackOnDelete, &parent->callBackOnDelete);
    jsVarCallBackCloneHook(&bb->callBackOnReadBufferShift, &parent->callBackOnReadBufferShift);
    jsVarCallBackCloneHook(&bb->callBackOnTcpIpConnect, &parent->callBackOnTcpIpConnect);
    jsVarCallBackCloneHook(&bb->callBackOnConnect, &parent->callBackOnConnect);
    jsVarCallBackCloneHook(&bb->callBackOnAccept, &parent->callBackOnAccept);
    jsVarCallBackCloneHook(&bb->callBackAcceptFilter, &parent->callBackAcceptFilter);
    jsVarCallBackCloneHook(&bb->callBackOnTcpIpAccept, &parent->callBackOnTcpIpAccept);
}

JSVAR_STATIC int baioLibraryInit(int deInitializationFlag) {
    static int      libraryInitialized = 0;
    int             r;
#if _WIN32
    static WSADATA  wsaData;
#endif

    if (libraryInitialized == 0 && deInitializationFlag == 0) {
#if _WIN32
        r = WSAStartup(MAKEWORD(2,2), &wsaData);
        if (r != 0) {
            printf("WSAStartup failed with error: %d\n", r);
            return(-1);
        }
#endif
        libraryInitialized = 1;
    } else if (libraryInitialized != 0 && deInitializationFlag != 0) {
#if _WIN32
        WSACleanup();
#endif
        libraryInitialized = 0;
    } else {
        return(1);
    }
    return(0);
}

static struct baio *baioInitBasicStructure(int i, int baioType, int ioDirections, int additionalSpaceAllocated, struct baio *parent) {
    struct baio *bb;
    int         parentsize;

    bb = baioTab[i];
    assert(bb != NULL);
    if (parent == NULL) {
        memset(bb, 0, sizeof(struct baio)+additionalSpaceAllocated);
    } else {
        parentsize = sizeof(struct baio) + parent->additionalSpaceAllocated;
        assert(parentsize <= (int)sizeof(struct baio) + additionalSpaceAllocated);
        memcpy(bb, parent, parentsize);
        // clone callbackHooks
        cloneCallBackHooks(bb, parent);
        if (additionalSpaceAllocated > parent->additionalSpaceAllocated) {
            memset((char *)bb+parentsize, 0, additionalSpaceAllocated - parent->additionalSpaceAllocated);
        }
    }
    if (parent == NULL) bb->initialReadBufferSize = (1<<10);
    if (parent == NULL) bb->readBufferRecommendedSize = (1<<16);
    if (parent == NULL) bb->initialWriteBufferSize = (1<<10);
    if (parent == NULL) bb->minFreeSizeBeforeRead = 1600;   /* to hold TCPIP packet */
    if (parent == NULL) bb->minFreeSizeAtEndOfReadBuffer = 1;
    if (parent == NULL) bb->maxReadBufferSize = (1<<24);
    if (parent == NULL) bb->maxWriteBufferSize = (1<<24);
    if (parent == NULL) bb->optimizeForSpeed = 0;

    bb->ioDirections = ioDirections;
    bb->status = BAIO_STATUS_ACTIVE;
    bb->fd = -1;
    bb->index = i;
    // baiomagic shall never be zero 
    // bb->baioMagic = (((magicCounter++) & 0x1ffffe) + 1) * BAIO_MAX_CONNECTIONS + i;
    bb->baioMagic = ((rand() & 0x1ffffe) + 1) * BAIO_MAX_CONNECTIONS + i;
    assert(bb->baioMagic != 0);
    bb->baioType = baioType;
    bb->additionalSpaceAllocated = additionalSpaceAllocated;

    // useSsl is inherited, but sslHandle and sslPending are not
    // bb->useSsl = 0;
    bb->sslHandle = NULL;
    bb->sslPending = 0;
    bb->sslSniHostName = NULL;

    memset(&bb->readBuffer, 0, sizeof(bb->readBuffer));
#if 0
    if (bb->ioDirections == BAIO_IO_DIRECTION_READ || bb->ioDirections == BAIO_IO_DIRECTION_RW) {
        // Hmm. I do not want to allocate buffer here, because then bb->initialReadBufferSize
        // set by user is unused
        baioBufferInit(&bb->readBuffer, bb->initialReadBufferSize);
    }
#endif
    // printf("reset writebuffer %p\n", &bb->writeBuffer);
    memset(&bb->writeBuffer, 0, sizeof(bb->writeBuffer));
#if 0
    if (bb->ioDirections == BAIO_IO_DIRECTION_WRITE || bb->ioDirections == BAIO_IO_DIRECTION_RW) {
        baioWriteBufferInit(&bb->writeBuffer, bb->initialWriteBufferSize);
    }
#endif
    return(bb);
}

struct baio *baioNewBasic(int baioType, int ioDirections, int additionalSpaceToAllocate) {
    int             i;
    struct baio     *bb;

    baioLibraryInit(0);
    i = baioTabFindUnusedEntryIndex(baioType);
    if (i < 0) return(NULL);
    assert(baioTab[i] == NULL);
    if (baioTab[i] == NULL) JSVAR_ALLOC_SIZE(baioTab[i], struct baio, sizeof(struct baio)+additionalSpaceToAllocate);
    bb = baioInitBasicStructure(i, baioType, ioDirections, additionalSpaceToAllocate, NULL);
    return(bb);
}

int baioClose(struct baio *bb) {
    if (bb->ioDirections == BAIO_IO_DIRECTION_READ || bb->ioDirections == BAIO_IO_DIRECTION_RW) {
        baioReadBufferFree(&bb->readBuffer);
    }
    if (bb->ioDirections == BAIO_IO_DIRECTION_WRITE || bb->ioDirections == BAIO_IO_DIRECTION_RW) {
        if (baioMsgInProgress(bb)) {
            // if there is an unfinished message in progress, remove it
            baioMsgRemoveLastMsg(bb);
        }
        if (bb->writeBuffer.i != bb->writeBuffer.j) {
            bb->status |= BAIO_STATUS_PENDING_CLOSE;
        }
    }
    if ((bb->status & BAIO_STATUS_PENDING_CLOSE) == 0) {
        baioImmediateDeactivate(bb);
        return(0);
    }
    return(1);
}

int baioCloseMagic(int baioMagic) {
    struct baio *bb;
    int         res;
    bb = baioFromMagic(baioMagic);
    if (bb == NULL) return(-1);
    res = baioClose(bb);
    return(res);
}

int baioCloseMagicOnError(int baioMagic) {
    struct baio *bb;
    int         res;
    bb = baioFromMagic(baioMagic);
    if (bb == NULL) return(-1);
    res = baioCloseOnError(bb);
    return(res);
}

int baioAttachFd(struct baio *bb, int fd) {
    JsVarInternalCheck(bb->fd == -1);
    JsVarInternalCheck(bb->status == BAIO_STATUS_ACTIVE);
    bb->fd = fd;
    return(0);
}

int baioDettachFd(struct baio *bb, int fd) {
    bb->fd = -1;
    bb->sslPending = 0;
    return(0);
}

// in fact, the real buffer size is b->size - 1, because if b->i == b->j we consider as empty buffer
// so the buffer is full if b->i == b->j-1, i.e. there is always 1 byte unused in our ring buffer
static int baioSizeOfContinuousFreeSpaceForWrite(struct baioWriteBuffer *b) {
    int i,j;
    i = b->i;
    j = b->j;
    if (i <= j) {
        return(b->size - j - 1);
    } else {
        return(i - j - 1);
    }
}

int baioWriteBufferUsedSpace(struct baio *b) {
    int     i,j;
    i = b->writeBuffer.i;
    j = b->writeBuffer.j;
    if (i <= j) {
        return(j - i);
    } else {
        return(b->writeBuffer.jj - i + b->writeBuffer.j);
    }
}

int baioPossibleSpaceForWrite(struct baio *bb) {
    struct baioWriteBuffer  *b;
    struct baioMsg          *m;

    b = &bb->writeBuffer;
    if (bb->optimizeForSpeed) {
        int     i,j;
        int     s1, s2;
        i = b->i;
        j = b->j;
        if (i <= j) {
            s1 = bb->maxWriteBufferSize - j - 1;
            s2 = i;
            if (b->mi != b->mj) {
                m = &b->m[BAIO_MSGS_PREVIOUS_INDEX(b, b->mj)];
                if (m->endIndex == BAIO_INIFINITY_INDEX) {JsVarInternalCheck(b->j-m->startIndex>=0); s2 -= b->j - m->startIndex;}
            }
            return(JSVAR_MAX(s1, s2));
        } else {
            return(i-j-1);
        }
    } else {
        return(bb->maxWriteBufferSize - baioWriteBufferUsedSpace(bb) - 1);
    }
}

static void baioMsgsReindexInterval(struct baioWriteBuffer  *b, int from, int to, int offset) {
    int             i;
    struct baioMsg  *m;
    for(i=b->mi; i!=b->mj; i=BAIO_MSGS_NEXT_INDEX(b, i)) {
        m = &b->m[i];
        if (m->startIndex != BAIO_INIFINITY_INDEX && m->startIndex >= from && m->startIndex <= to) m->startIndex += offset;
        if (m->endIndex != BAIO_INIFINITY_INDEX && m->endIndex >= from && m->endIndex <= to) m->endIndex += offset;
    }
}

static int baioGetSpaceForWriteInWrappedBuffer(struct baio *bb, int n) {
    int                     r, offset;
    struct baioWriteBuffer  *b;

    
    b = &bb->writeBuffer;
    assert(b->j < b->i);
    assert(b->jj > 0);

// printf("RESIZE: %p: baioGetSpaceForWriteInWrappedBuffer: 0\n", b);

    // we have space between j until i, if not enough allocate extra space and increase i
    if (b->i - b->j <= n && bb->optimizeForSpeed) return(-1);

    // printf("6");fflush(stdout);
    while (b->size - b->jj + b->i - b->j <= n) {
        r = baioWriteBufferResize(&bb->writeBuffer, bb->initialWriteBufferSize, bb->maxWriteBufferSize);
        if (r < 0) return(-1);
    }
// printf("RESIZE: %p: baioGetSpaceForWriteInWrappedBuffer: 1\n", b);
    if (b->i - b->j <= n) {
        // printf("RESIZE: %p: baioGetSpaceForWriteInWrappedBuffer: 2\n", b);
        offset = b->size - b->jj;
        // printf("offset == %d, b->i == %d, b->ij == %d, b->j == %d, b->jj == %d;  b->mi == %d, b->mj == %d\n", offset, b->i, b->ij, b->j, b->jj, b->mi, b->mj);
        memmove(b->b+b->i+offset, b->b+b->i, b->jj-b->i);
        // move all messages from j to the end of resized buffer
        baioMsgsReindexInterval(b, b->i, b->jj, offset);
        assert(b->ij >= b->i);
        if (b->ij >= b->i) b->ij += offset;
        b->i += offset;
        b->jj += offset;
    }
    return(0);
}

static int baioGetSpaceForWriteInEmptyBuffer(struct baio *bb, int n) {
    int                     r;
    struct baioWriteBuffer  *b;
    struct baioMsg          *m;

//printf("RESIZE: %p: baioGetSpaceForWriteInEmptyBuffer: 0\n", b);

    b = &bb->writeBuffer;
    // actually there can be message which is going to be constructed
    // assert(b->mi == b->mj);

    while (b->size <= n) {
        r = baioWriteBufferResize(&bb->writeBuffer, bb->initialWriteBufferSize, bb->maxWriteBufferSize);
        if (r < 0) return(-1);
    }
    // O.K. if we have enough of space
    // if (baioSizeOfContinuousFreeSpaceForWrite(b) > n) return(0);

    // no, reset buffer
    b->i = b->ij = b->j = b->jj = 0;
    if (b->mi != b->mj) {
//printf("RESIZE: %p: baioGetSpaceForWriteInEmptyBuffer: 1\n", b);
        assert(BAIO_MSGS_NEXT_INDEX(b, b->mi) == b->mj);
        m = &b->m[b->mi];
        assert(m->endIndex == BAIO_INIFINITY_INDEX);
        m->startIndex = 0;
    }
    return(0);
}

static int baioGetSpaceForWriteInNonEmptyLinearBufferAllMessagesReady(struct baio *bb, int n) {
    int                     r, offset;
    struct baioWriteBuffer  *b;

    b = &bb->writeBuffer;
    assert(b->i < b->j);

    // Unfortunately, all this branch is absolutely non-tested, because we always have a message being composed
    // TODO: Test this in some way
//printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferAllMessagesReadys: 0\n", b);

    // if we have enough of space at the end of buffer, do nothing
    if (b->size - b->j > n) return(0);
    // if not, if we have enough of space at the begginning of buffer, wrap buffer
    if (b->i > n) {
//printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferAllMessagesReadys: 1\n", b);
        // printf("1");fflush(stdout);
        b->jj = b->j;
        b->j = 0;
    } else {
// printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferAllMessagesReadys: 2\n", b);
        while (b->size - b->j <= n && b->size < bb->maxWriteBufferSize) {
            // if we still can resize, then resize
            r = baioWriteBufferResize(&bb->writeBuffer, bb->initialWriteBufferSize, bb->maxWriteBufferSize);
            if (r < 0) return(-1);
        }
        if (b->size - b->j <= n) {
// printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferAllMessagesReadys: 3\n", b);
            if (bb->optimizeForSpeed) return(-1);
            // TODO: Test this branch!
            if (b->size - b->j + b->i < n) return(-1);
// printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferAllMessagesReadys: 4\n", b);
            // it is better to shift the used buffer to the end, because new space will come from beginning
            // and in the future we will not need to shift it anymore
            offset = b->size - b->j;
            memmove(b->b+b->i+offset, b->b+b->i, b->j-b->i);
            baioMsgsReindexInterval(b, b->i, b->j, offset);
            b->jj = b->size;
            assert(b->ij >= b->i);
            if (b->ij >= b->i) b->ij += offset;
            b->i += offset;
            b->j = 0;
        }
    }
    return(0);
}

static void baioMemSwap(char *p1, char *p2, char *pe) {
    char    *t;
    int     l1, l2;
    // TODO: Do it without temporary allocation
    assert(p1 <= p2);
    assert(p2 <= pe);
    l1 = p2-p1;
    l2 = pe-p2;
    JSVAR_ALLOCC(t, l2, char);
    memmove(t, p2, l2);
    memmove(pe-l1, p1, l1);
    memmove(p1, t, l2);
    JSVAR_FREE(t);
}

static int baioGetSpaceForWriteInNonEmptyLinearBufferWithWaitingMessages(struct baio *bb, int n) {
    int                     r, msglen, offset;
    struct baioWriteBuffer  *b;
    struct baioMsg          *m;

    b = &bb->writeBuffer;
    assert(b->i < b->j);
    assert(b->mi != b->mj);

//printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferWithWaitingMessages: 0\n", b);

    // if we have enough of space at the end of buffer, do nothing
    if (b->size - b->j > n) return(0);
    // get last message
    m = &b->m[BAIO_MSGS_PREVIOUS_INDEX(b, b->mj)];

    if (m->endIndex != BAIO_INIFINITY_INDEX) {
        // printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferWithWaitingMessages: 1\n", b);
        // last message was finished we do not need to care about it
        r = baioGetSpaceForWriteInNonEmptyLinearBufferAllMessagesReady(bb, n);
        return(r);
    }

    // we are currently composing message and we need more space
    msglen = b->j - m->startIndex;
    if (b->i/*??? why there was -1*/ > msglen + n || (b->i == m->startIndex && b->size > msglen + n)) {
        // printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferWithWaitingMessages: 2. %d,%d,%d,%d  %d,%d,  %d,  %d,%d,   %d\n", b, b->i, b->ij, b->j, b->jj, b->mi, b->mj, m->startIndex, msglen, n, b->size);
        // we have enough of space, move the message to the beginning of buffer
        b->jj = m->startIndex;
        memmove(b->b, b->b+m->startIndex, msglen);
        if (b->i == m->startIndex) {
            // printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferWithWaitingMessages: 3\n", b);
            b->i = b->ij = 0;
        }
        m->startIndex = 0;
        b->j = msglen;
    } else {
        // not enough of space at the beginning of buffer, we have to allocate extra space at the end
        while (b->size - b->j <= n && b->size < bb->maxWriteBufferSize) {
            // if we still can resize, then resize
            r = baioWriteBufferResize(&bb->writeBuffer, bb->initialWriteBufferSize, bb->maxWriteBufferSize);
            if (r < 0) return(-1);
        }
        // printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferWithWaitingMessages: 4\n", b);
        if (b->size - b->j <= n) {
            if (bb->optimizeForSpeed) return(-1);
            if (b->size - b->j + b->i < n) return(-1);
            // printf("RESIZE: %p: baioGetSpaceForWriteInNonEmptyLinearBufferWithWaitingMessages: 5\n", b);
            offset = b->size - m->startIndex;
            // it is better to shift the used buffer to the end, because new space will come from beginning
            // and in the future we will not need to shift it anymore
            baioMemSwap(b->b+b->i, b->b+m->startIndex, b->b+b->size);
            // now the last message starts at b->i, move it to the beginning of buffer
            // TODO: Inline the swap and do not copy current message three times
            memmove(b->b, b->b+b->i, msglen);
            baioMsgsReindexInterval(b, b->i, b->j, offset);
            b->jj = b->size;
            assert(b->ij >= b->i);
            if (b->ij >= b->i) b->ij += offset;
            b->i += offset;
            b->j = msglen;
            m->startIndex = 0;
            assert(b->i - b->j >= n);
        }
    }
    return(0);
}

static int baioGetSpaceForWriteInLinearBuffer(struct baio *bb, int n) {
    int                     r;
    struct baioWriteBuffer  *b;

    b = &bb->writeBuffer;
    if (b->mi == b->mj) {
        r = baioGetSpaceForWriteInNonEmptyLinearBufferAllMessagesReady(bb, n);
    } else {
        r = baioGetSpaceForWriteInNonEmptyLinearBufferWithWaitingMessages(bb, n);
    }
    return(r);
}

static int baioGetSpaceForWrite(struct baio *bb, int n) {
    int                     r;
    struct baioWriteBuffer  *b;

    // We need space in a msg based ring buffer
    b = &bb->writeBuffer;
    if (b->i == b->j) {
        r = baioGetSpaceForWriteInEmptyBuffer(bb, n);
    } else if (b->i < b->j) {
        r = baioGetSpaceForWriteInLinearBuffer(bb, n);
    } else {
        r = baioGetSpaceForWriteInWrappedBuffer(bb, n);
    }
    return(r);
}

int baioMsgReserveSpace(struct baio *bb, int len) {
    int                     r;
    struct baioWriteBuffer  *b;

    if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return(-1);

    b = &bb->writeBuffer;
    r = baioGetSpaceForWrite(bb, len);
    if (r < 0) return(-1);
    b->j += len;
    return(len);
}

int baioWriteToBuffer(struct baio *bb, char *s, int len) {
    int                     r;
    char                    *d;
    struct baioWriteBuffer  *b;

    if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return(-1);

    r = baioGetSpaceForWrite(bb, len);
    if (r < 0) return(-1);
    b = &bb->writeBuffer;
    d = b->b + b->j;
    memmove(d, s, len);
    b->j += len;
    return(len);
}

int baioWriteMsg(struct baio *bb, char *s, int len) {
    int n;
    n = baioWriteToBuffer(bb, s, len);
    if (n >= 0) baioMsgPut(bb, bb->writeBuffer.j-n, bb->writeBuffer.j);
    return(n);
}

int baioVprintfToBuffer(struct baio *bb, char *fmt, va_list arg_ptr) {
    int                     n, r, dsize;
    struct baioWriteBuffer  *b;
    va_list                 arg_ptr_copy;

    if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return(-1);

    b = &bb->writeBuffer;
    dsize = baioSizeOfContinuousFreeSpaceForWrite(b);
    if (dsize < 0) dsize = 0;
    va_copy(arg_ptr_copy, arg_ptr);
    n = vsnprintf(b->b + b->j, dsize, fmt, arg_ptr_copy);
#if _WIN32
    while (n < 0) {
        dsize = dsize * 2 + 1024;
        r = baioGetSpaceForWrite(bb, dsize+1);
        if (r < 0) return(-1);
        n = vsnprintf(b->b + b->j, dsize, fmt, arg_ptr_copy);
    }
#endif
    JsVarInternalCheck(n >= 0);
    va_copy_end(arg_ptr_copy);
    if (n >= dsize) {
        r = baioGetSpaceForWrite(bb, n+1);
        if (r < 0) return(-1);
        va_copy(arg_ptr_copy, arg_ptr);
        n = vsnprintf(b->b + b->j, n+1, fmt, arg_ptr_copy);
        JsVarInternalCheck(n>=0);
        va_copy_end(arg_ptr_copy);
    }

    b->j += n;
    return(n);
}

int baioPrintfToBuffer(struct baio *bb, char *fmt, ...) {
    int             res;
    va_list         arg_ptr;

    va_start(arg_ptr, fmt);
    res = baioVprintfToBuffer(bb, fmt, arg_ptr);
    va_end(arg_ptr);
    return(res);
}

int baioVprintfMsg(struct baio *bb, char *fmt, va_list arg_ptr) {
    int n;
    n = baioVprintfToBuffer(bb, fmt, arg_ptr);
    if (n >= 0) baioMsgPut(bb, bb->writeBuffer.j-n, bb->writeBuffer.j);
    return(n);
}

int baioPrintfMsg(struct baio *bb, char *fmt, ...) {
    int             res;
    va_list         arg_ptr;

    va_start(arg_ptr, fmt);
    res = baioVprintfMsg(bb, fmt, arg_ptr);
    va_end(arg_ptr);
    return(res);
}

//////////////////////////////////////////////////////////

static int baioSslVerifyCallback(int preverify, X509_STORE_CTX* x509_ctx) {
    if (! preverify) {
        printf("%s: %s:%d: Verification problem.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    }
    return preverify;
}

JSVAR_STATIC int baioSslLibraryInit() {
    static int  libraryInitialized = 0;
    int         rc, rk;
    BIO         *bio;
    X509        *x509;
    EVP_PKEY    *pkey;
    char        *errtype;
    
    if (libraryInitialized) return(0);

    // Register the error strings for libcrypto & libssl
    SSL_load_error_strings();
    // Register the available ciphers and digests
    SSL_library_init();

    // We are probably sticked with SSL version 2 because of some servers
    baioSslContext = SSL_CTX_new(SSLv23_method());
	// A more recommended metod (according doc)
    // baioSslContext = SSL_CTX_new(TLS_method());

    if (baioSslContext == NULL) {
        ERR_print_errors_fp(stderr);
        return(-1);
    }

    /* Load server certificate */
    if (baioSslServerCertFile != NULL) {
        // if file was specified, load from file
        rc = SSL_CTX_use_certificate_file(baioSslContext, baioSslServerCertFile, SSL_FILETYPE_PEM);
    } else if (baioSslServerCertPem != NULL) {
        // otherwise load from memory
        rc = -1;
        bio = BIO_new_mem_buf(baioSslServerCertPem, -1);
        if(bio != NULL) {
            x509 = PEM_read_bio_X509(bio, NULL, 0, NULL);
            if (x509 != NULL) {
                rc = SSL_CTX_use_certificate(baioSslContext, x509);
                X509_free(x509);
            }
            BIO_free(bio);
        }
    } else {
        // This is the setting for SSL client
        rc = 1;
    }
    
    /* Load private-key */
    if (baioSslServerKeyFile != NULL) {
        // if file was specified
        rk = SSL_CTX_use_PrivateKey_file(baioSslContext, baioSslServerKeyFile, SSL_FILETYPE_PEM);
    } else if (baioSslServerKeyPem != NULL) {
        // otherwise load from memory
        rk = -1;
        bio = BIO_new_mem_buf(baioSslServerKeyPem, -1);
        if(bio != NULL) {
            pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
            if(pkey != NULL) {
                rk = SSL_CTX_use_PrivateKey(baioSslContext, pkey);
                EVP_PKEY_free(pkey);
            }
            BIO_free(bio);
        }
    } else {
        // This is the setting for SSL client
        rk = 1;
    }
    if (rc <= 0 || rk <= 0) {
        printf("%s:%s:%d: Warning: Can't load SSL server certificate or key!", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        // ERR_print_errors_fp(stdout);
        // This is just a warning because it does not mind if you only use client side, uncomment following if you need an Ssl server
        // SSL_CTX_free(baioSslContext);
        // baioSslContext = NULL;
        // return(-1);
    }

    
#if 0
    /* unlock this code and define baioSslTrustedCertFile, if you want to verify certificates */
    SSL_CTX_set_verify(baioSslContext, SSL_VERIFY_PEER, baioSslVerifyCallback);

    if(! SSL_CTX_load_verify_locations(baioSslContext, baioSslTrustedCertFile, NULL)) {
        printf("%s:%s:%d: Can't load trusted certificates %s.", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, baioSslTrustedCertFile);
        ERR_print_errors_fp(stdout);
        SSL_CTX_free(baioSslContext);
        baioSslContext = NULL;
        return(-1);
    }
#endif

    SSL_CTX_set_mode(baioSslContext, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    libraryInitialized = 1;
    return(0);
}

static int baioSslHandleInit(struct baio *bb) {
    int r;

    r = baioSslLibraryInit();
    if (r != 0) return(r);

    // Create an SSL struct for the connection
    bb->sslHandle = SSL_new(baioSslContext);
    if (bb->sslHandle == NULL) return(-1);

    // Connect the SSL struct to our connection
    if (! SSL_set_fd(bb->sslHandle, bb->fd)) return(-1);

    return(0);
}

static void baioSslConnect(struct baio *bb) {
    int err, r;

    if (bb->sslHandle == NULL) return;

    ERR_clear_error();
    // SNI support
    if (bb->sslSniHostName != NULL) SSL_set_tlsext_host_name(bb->sslHandle, bb->sslSniHostName);
    r = SSL_connect(bb->sslHandle);

    if (r == 0) {
        // First set status, then callbak, so that I can use the same callback as for copy
        bb->status |= BAIO_STATUS_EOF_READ;
        JSVAR_CALLBACK_CALL(bb->callBackOnEof, callBack(bb));
    }

    if (r < 0) {
        err = SSL_get_error(bb->sslHandle, r);
        if (err == SSL_ERROR_WANT_READ) {
            bb->status |= BAIO_BLOCKED_FOR_READ_IN_SSL_CONNECT;
        } else if (err==SSL_ERROR_WANT_WRITE) {
            bb->status |= BAIO_BLOCKED_FOR_WRITE_IN_SSL_CONNECT;
        } else {
            // printf("ssl connect r==%d err == %d\n", r, err);
            baioCloseOnError(bb);
            return;
        }
    } else {
        // Connection established
        JSVAR_CALLBACK_CALL(bb->callBackOnConnect, callBack(bb));
    }
}

static void baioOnTcpIpConnected(struct baio *bb) {
    int     r;

    JSVAR_CALLBACK_CALL(bb->callBackOnTcpIpConnect, callBack(bb));
    // It seems that I should getsockopt to read the SO_ERROR to check if connection was successful, however is it really necessary?
    {
        int err;
        socklen_t len = sizeof(err);
        getsockopt(bb->fd, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
        if (err != 0) {baioCloseOnError(bb); return;}
    }

    if (bb->useSsl == 0) {
        // nothing more to do if clean connection
        JSVAR_CALLBACK_CALL(bb->callBackOnConnect, callBack(bb));
        return;
    }
    r = baioSslHandleInit(bb);
    if (r != 0) {
        baioCloseOnError(bb);
        return;
    }
    baioSslConnect(bb);
}

static void baioSslAccept(struct baio *bb) {
    int r, err;

    if (bb->sslHandle == NULL) return;

    ERR_clear_error();
    r = SSL_accept(bb->sslHandle);

	// printf("%s: ssl_accept returned %d\n", JSVAR_PRINT_PREFIX(), r); fflush(stdout);
    if (r == 1) {
        // accepted
        JSVAR_CALLBACK_CALL(bb->callBackOnAccept, callBack(bb));
        if (jsVarDebugLevel > 0) printf("%s: %s:%d: SSL connection accepted on socket %d\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, bb->fd);
        return;
    }

    // not accepted
    err = SSL_get_error(bb->sslHandle, r);
	// printf("%s: err == %d\n", JSVAR_PRINT_PREFIX(), err); fflush(stdout);	
    if (err == SSL_ERROR_WANT_READ) {
        bb->status |= BAIO_BLOCKED_FOR_READ_IN_SSL_ACCEPT;
    } else if (err == SSL_ERROR_WANT_WRITE) {
        bb->status |= BAIO_BLOCKED_FOR_WRITE_IN_SSL_ACCEPT;
    } else {
        // printf("r == %d, err == %d errno %s\n", r, err, JSVAR_STR_ERRNO());
        if (jsVarDebugLevel > 0) printf("%s: %s:%d: SSL accept on socket %d failed\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, bb->fd);
        baioImmediateDeactivate(bb);
    }
}

static void baioOnTcpIpListenActivated(struct baio *bb) {
    struct sockaddr_in      clientaddr;
    int                     fd, i, r;
    unsigned                ip, port;
    struct baio             *cc;
    socklen_t               len;

    // anyway I'll continue listening
    bb->status |= BAIO_BLOCKED_FOR_READ_IN_TCPIP_LISTEN;

    len = sizeof(clientaddr);
    fd = accept(bb->fd, (struct sockaddr *)&clientaddr, &len);
    if (fd < 0) {
        if (jsVarDebugLevel > 0) printf("A new client connection failed.\n");
        return;
    }
    r = jsVarSetSocketNonBlocking(fd);
    if (r < 0) {
        printf("%s: Can't set new tcp/ip client connection to non-blocking state, closing it.\n", JSVAR_PRINT_PREFIX());
        closesocket(fd);
        return;
    }
    ip = clientaddr.sin_addr.s_addr;
    port = clientaddr.sin_port;

    if (jsVarDebugLevel > 0) printf("%s: connection accepted from %s:%d on socket %d.\n", JSVAR_PRINT_PREFIX(), jsVarSprintfIpAddress_st(ip), port, bb->fd);

    r = 0;
    JSVAR_CALLBACK_CALL(bb->callBackAcceptFilter, (r = callBack(bb, ip, port)));

    // reject connection if filter rejected
    if (r < 0) {
        closesocket(fd);
        return;
    }

    // continue with creating new baio
    i = baioTabFindUnusedEntryIndex(BAIO_TYPE_TCPIP_SCLIENT);
    if (i < 0) {
        printf("%s: Can't allocate new baio for new client %s:%d on socket %d\n", JSVAR_PRINT_PREFIX(), jsVarSprintfIpAddress_st(ip), port, bb->fd);
        closesocket(fd);
        return;
    }
    assert(baioTab[i] == NULL);
    if (baioTab[i] == NULL) JSVAR_ALLOC_SIZE(baioTab[i], struct baio, sizeof(struct baio)+bb->additionalSpaceAllocated);
    cc = baioInitBasicStructure(i, BAIO_TYPE_TCPIP_SCLIENT, BAIO_IO_DIRECTION_RW, bb->additionalSpaceAllocated, bb);
    cc->fd = fd;
    cc->remoteComputerIp = ip;
    cc->parentBaioMagic = bb->baioMagic;
    JSVAR_CALLBACK_CALL(cc->callBackOnTcpIpAccept, callBack(cc));
    if (cc->useSsl == 0) {
        JSVAR_CALLBACK_CALL(cc->callBackOnAccept, callBack(cc));
    } else {
        r = baioSslHandleInit(cc);
        if (r != 0) {
            baioCloseOnError(cc);
            return;
        }
        baioSslAccept(cc);
    }
}

static int baioItIsPreferrableToResizeReadBufferOverMovingData(struct baioReadBuffer *b, struct baio *bb) {
    // if there is room for resizeing and data occupy too much space, prefer resizing
    // buffer is of maximal size, move
    if (b->size >= bb->maxReadBufferSize / 2) return(0);
    // More than half of space occupied in the buffer, prefer resize
    if (b->j - b->i >= b->size / 2) return(1);
    return(0);
}

static void baioNormalizeBufferBeforeRead(struct baioReadBuffer *b, struct baio *bb) {
    int delta; 

    if (b->i == b->j) {
        // we have previously processed all the buffer, reset indexes to start from the very beginning
        b->i = b->j = 0;
    } 
    if (b->b != NULL
        && b->size - b->j < bb->minFreeSizeBeforeRead + bb->minFreeSizeAtEndOfReadBuffer   // not enough space for read
        && baioItIsPreferrableToResizeReadBufferOverMovingData(b, bb) == 0
        ) {
        // we rolled to the end of the buffer, move the trail to get enough of space
        delta = baioReadBufferShift(bb, b);
        JSVAR_CALLBACK_CALL(bb->callBackOnReadBufferShift, callBack(bb, delta));
    }
    while (b->size - b->j < bb->minFreeSizeBeforeRead + bb->minFreeSizeAtEndOfReadBuffer && b->size < bb->maxReadBufferSize) {
        // buffer too small, resize it until it fits the size or max value
        baioReadBufferResize(b, bb->initialReadBufferSize, bb->maxReadBufferSize);
    }
}

static void baioHandleSuccesfullRead(struct baio *bb, int n) {
    struct baioReadBuffer   *b;
    int                     sj;

    b = &bb->readBuffer;
    if (n == 0) {
        // end of file
        bb->status |= BAIO_STATUS_EOF_READ;
        // Hmm. maybe I shall call eof callback only when readbuffer is empty
        // Unfortunately this is not possible, because we can miss it (if read buffer is emptied out of baio).
        JSVAR_CALLBACK_CALL(bb->callBackOnEof, callBack(bb));
    } else if (n > 0) {
        bb->totalBytesRead += n;
        sj = b->j;
        b->j += n;
        if (bb->minFreeSizeAtEndOfReadBuffer > 0) b->b[b->j] = 0;
        JSVAR_CALLBACK_CALL(bb->callBackOnRead, callBack(bb, sj, n));
        // If we have read half of the read buffer at once, maybe it is time to resize it
        if (n > b->size / 2 && b->size < bb->readBufferRecommendedSize) {
            baioReadBufferResize(b, bb->initialReadBufferSize, bb->maxReadBufferSize);
        }
    }
}

static int baioOnCanRead(struct baio *bb) {
    struct baioReadBuffer   *b;
    int                     n;
    int                     minFreeSizeAtEndOfReadBuffer;

    b = &bb->readBuffer;
    minFreeSizeAtEndOfReadBuffer = bb->minFreeSizeAtEndOfReadBuffer;
    baioNormalizeBufferBeforeRead(b, bb);
    if (baioFdIsSocket(bb)) {
        n = recv(bb->fd, b->b+b->j, b->size-b->j-1-minFreeSizeAtEndOfReadBuffer, 0);
    } else {
        n = read(bb->fd, b->b+b->j, b->size-b->j-1-minFreeSizeAtEndOfReadBuffer);
    }
    if (jsVarDebugLevel > 20) {printf("read returned %d\n", n); fflush(stdout);}
    if (n < 0) return(baioCloseOnError(bb));
    baioHandleSuccesfullRead(bb, n);
    return(n);
}

static int baioOnCanSslRead(struct baio *bb) {
    struct baioReadBuffer   *b;
    int                     n, err;
    int                     minFreeSizeAtEndOfReadBuffer;

    assert(bb->useSsl);
    if (bb->sslHandle == NULL) return(0);

    b = &bb->readBuffer;
    minFreeSizeAtEndOfReadBuffer = bb->minFreeSizeAtEndOfReadBuffer;
    baioNormalizeBufferBeforeRead(b, bb);
    bb->sslPending = 0;
    ERR_clear_error();
    n = SSL_read(bb->sslHandle, b->b + b->j, b->size - b->j - minFreeSizeAtEndOfReadBuffer);
    if (jsVarDebugLevel > 20) {printf("SSL_read(,,%d) returned %d\n", b->size - b->j - minFreeSizeAtEndOfReadBuffer, n); fflush(stdout);}
    if (n < 0) {
        err = SSL_get_error(bb->sslHandle, n);
        if (err == SSL_ERROR_WANT_WRITE) {
            bb->status |= BAIO_BLOCKED_FOR_WRITE_IN_SSL_READ;
            return(0);
        }
        if (err == SSL_ERROR_WANT_READ) {
            bb->status |= BAIO_BLOCKED_FOR_READ_IN_SSL_READ;
            return(0);
        }
        return(baioCloseOnError(bb));
    }
    baioHandleSuccesfullRead(bb, n);
    if (bb->sslHandle != NULL) bb->sslPending = SSL_pending(bb->sslHandle); 
    return(n);
}

static int baioWriteOrSslWrite(struct baio *bb) {
    struct baioWriteBuffer  *b;
    int                     n, err, len;

    b = &bb->writeBuffer;
    if (bb->useSsl == 0) {
        len = b->ij - b->i;
        if (baioFdIsSocket(bb)) {
            if (len > BAIO_SEND_MAX_MESSAGE_LENGTH) len = BAIO_SEND_MAX_MESSAGE_LENGTH;
            n = send(bb->fd, b->b+b->i, len, 0);
        } else {
            n = write(bb->fd, b->b+b->i, len);
        }
        if (jsVarDebugLevel > 20) {printf("write(,,%d) returned %d\n", b->ij - b->i, n); fflush(stdout);}
        if (n < 0 && (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)) n = 0;
    } else {
        if (bb->sslHandle == NULL) return(-1);
        n = SSL_write(bb->sslHandle, b->b+b->i, b->ij - b->i);
        if (jsVarDebugLevel > 20) {printf("SSL_write(,,%d) returned %d\n", b->ij - b->i, n); fflush(stdout);}
        if (n < 0) {
            err = SSL_get_error(bb->sslHandle, n);
            if (err == SSL_ERROR_WANT_WRITE) {
                bb->status |= BAIO_BLOCKED_FOR_WRITE_IN_SSL_WRITE;
                n = 0;
            } else if (err == SSL_ERROR_WANT_READ) {
                bb->status |= BAIO_BLOCKED_FOR_READ_IN_SSL_WRITE;
                n = 0;
            }
        }
    }
    if (n > 0) bb->totalBytesWritten += n;
    // printf("write: %d bytes, b->i == %d: Bytes: '%.20s...%.20s'\n", n, b->i, b->b+b->i, b->b+b->i+n-20);
    // printf("write: b->i == %d: %.*s\n", b->i, n, b->b+b->i);
    // printf("write: b->i == %d: %.*s\n", b->i, n, b->b+b->i);
    // printf("b->i == %d: ", b->i);baioCharBufferDump("baioWritten: ", b->b+b->i, n);
    // printf("b->j == %d: ", b->j);
    // printf("%s: writen %d bytes to fd %d\n", JSVAR_PRINT_PREFIX(), n, bb->fd);
    return(n);
}

int baioOnCanWrite(struct baio *bb) {
    struct baioWriteBuffer  *b;
    int                     n;
    int                     si;

    n = 0;
    b = &bb->writeBuffer;
    si = b->i;

    // only one write is allowed by select, go back to the main loop after a single write
    if (bb->fd >= 0 && b->i < b->ij) {
        n = baioWriteOrSslWrite(bb);
        if (n < 0) return(baioCloseOnError(bb));
        b->i += n;
    }
    baioMsgMaybeActivateNextMsg(bb);

    if (n != 0) JSVAR_CALLBACK_CALL(bb->callBackOnWrite, callBack(bb, si, n));
    // Attention has been taken here, because onWrite callback can itself write something
    // On the other hand we shall not call it on closed bb and we shall not shift it before
    // calling onwrite callback, because it would not receive correct indexes
    if (BAIO_WRITE_BUFFER_DO_FULL_SIZE_RING_CYCLES == 0 && b->i == b->j) {
        // with this optimization you can not use write buffer to retrieving historical data sent out
        // if buffer is empty after a write it is reinitialized to zero indexes
        b->i = b->ij = b->j = b->jj = 0;
        assert(bb->writeBuffer.mi == bb->writeBuffer.mj || bb->status == BAIO_STATUS_ZOMBIE);
        baioWriteMsgsInit(b);
    }
    if (b->i == b->j) {
        if (bb->status & BAIO_STATUS_PENDING_CLOSE) baioImmediateDeactivate(bb);
    }
    return(n);
}

/////////////////////


static int baioAddSelectFdToSet(int maxfd, int fd, fd_set *ss) {
    if (ss != NULL) {
        FD_SET(fd, ss);
        if (fd > maxfd) return(fd);
    }
    return(maxfd);
}

int baioSetSelectParams(int maxfd, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *t) {
    int             i, fd;
    struct baio     *bb;
    unsigned        status;
    int             flagWaitingForReadFd;
    int             flagWaitingForWriteFd;
    int             flagForZeroTimeoutAsBufferedDataAvailableWithoutFd;

    flagForZeroTimeoutAsBufferedDataAvailableWithoutFd = 0;
    for(i=0; i<baioTabMax; i++) {
        bb = baioTab[i];
        if (bb == NULL) goto nextfd;
        status = bb->status;
        fd = bb->fd;
        flagWaitingForReadFd = flagWaitingForWriteFd = 0;
        // free zombies
        if (1 && bb->status == BAIO_STATUS_ZOMBIE) {
            baioFreeZombie(bb);
        } else if (bb->baioType == BAIO_TYPE_TCPIP_SCLIENT && (status & BAIO_STATUS_EOF_READ)) {
            // I am allocating server clients, so I am going to delete them as well
            baioImmediateDeactivate(bb);
        } else if ((status & BAIO_STATUS_ACTIVE) &&  fd >= 0) {
            // special statuses (like connect, ssl_accept, ...
            if (status & BAIO_BLOCKED_FOR_READ_IN_SPECIAL_STATUS_MASK) {
                flagWaitingForReadFd = 1;
            } else if (status & BAIO_BLOCKED_FOR_WRITE_IN_SPECIAL_STATUS_MASK) {
                flagWaitingForWriteFd = 1;
            } else {
                // check for reading
                if (status & BAIO_BLOCKED_FOR_READ_STATUS_MASK) {
                    flagWaitingForReadFd = 1;
                } else if (
                    (bb->ioDirections == BAIO_IO_DIRECTION_READ || bb->ioDirections == BAIO_IO_DIRECTION_RW)
                    && (status & BAIO_STATUS_EOF_READ) == 0
                    && (status & BAIO_STATUS_PENDING_CLOSE) == 0
                    && bb->maxReadBufferSize - (bb->readBuffer.j - bb->readBuffer.i) > bb->minFreeSizeBeforeRead
                    ) {
                    if (bb->useSsl) {
                        if (bb->sslHandle != NULL && bb->sslPending > 0) {
                            bb->status |= BAIO_BUFFERED_DATA_AVAILABLE_FOR_SSL_READ;
                            if (fd > maxfd) maxfd = fd;
                            flagForZeroTimeoutAsBufferedDataAvailableWithoutFd = 1;
                        } else {
                            bb->status |= BAIO_BLOCKED_FOR_READ_IN_SSL_READ;
                            flagWaitingForReadFd = 1;
                        }
                    } else {
                        bb->status |= BAIO_BLOCKED_FOR_READ_IN_READ;
                        flagWaitingForReadFd = 1;
                    }
                }
                // check for writing
                if (status & BAIO_BLOCKED_FOR_WRITE_STATUS_MASK) {
                    flagWaitingForWriteFd = 1;
                } else if (
                    (bb->ioDirections == BAIO_IO_DIRECTION_WRITE || bb->ioDirections == BAIO_IO_DIRECTION_RW)
                    // previously there was the !=, but it may be safer to use <
                    // && bb->writeBuffer.i != bb->writeBuffer.ij
                    && bb->writeBuffer.i < bb->writeBuffer.ij
                    ) {
                    if (bb->useSsl) bb->status |= BAIO_BLOCKED_FOR_WRITE_IN_SSL_WRITE;
                    else bb->status |= BAIO_BLOCKED_FOR_WRITE_IN_WRITE;
                    flagWaitingForWriteFd = 1;
                }
            }
            if (flagWaitingForReadFd) {
                maxfd = baioAddSelectFdToSet(maxfd, fd, readfds);
            }
            if (flagWaitingForWriteFd) {
                maxfd = baioAddSelectFdToSet(maxfd, fd, writefds);
            }
            if (flagWaitingForReadFd || flagWaitingForWriteFd) {
                maxfd = baioAddSelectFdToSet(maxfd, fd, exceptfds);
            }
            if (jsVarDebugLevel > 20) {printf("%s: Set      %d: fd %d: status: %6x -> %6x; rwe : %d%d%d\n", JSVAR_PRINT_PREFIX(), i, fd, status, bb->status, (readfds != NULL&&FD_ISSET(fd, readfds)), (writefds != NULL && FD_ISSET(fd, writefds)), (exceptfds != NULL && FD_ISSET(fd, exceptfds))); fflush(stdout);}
        }
    nextfd:;
    }
    if (flagForZeroTimeoutAsBufferedDataAvailableWithoutFd) {
        if (t != NULL) {
            t->tv_sec = 0;
            t->tv_usec = 0;
        } else {
            printf("%s:%s:%d: Warning: Timeout parameter zero when read buffer full. Possible latency problem!\n", 
                   JSVAR_PRINT_PREFIX(), __FILE__, __LINE__
                );
        }
    }
    return(maxfd+1);
}

int baioOnSelectEvent(int maxfd, fd_set *readfds, fd_set *writefds, fd_set *exceptfds) {
    int             i, fd, max, res;
    unsigned        status;
    struct baio     *bb;

    res = 0;
    // loop until current baioTabMax, actions in the loop may add new descriptors, but those
    // were not waited at this moment. Do not check them
    max = baioTabMax;
    for(i=0; i<max; i++) {
        bb = baioTab[i];
        if (bb == NULL) goto nextfd;
#if EDEBUG
        assert(bb->writeBuffer.ij >= bb->writeBuffer.i);
#endif
        status = bb->status;
        bb->status &= ~(BAIO_STATUSES_CLEARED_PER_TICK);
        fd = bb->fd;
        // printf("fd == %d, maxfd == %d\n", fd, maxfd);
        if (0 && bb->status == BAIO_STATUS_ZOMBIE) {
            baioFreeZombie(bb);
        } else if (fd >= 0 && fd < maxfd) {
            if (jsVarDebugLevel > 20) {printf("%s: Event on %d: fd %d: status: %6x    %6s; rwep: %d%d%d%d\n", JSVAR_PRINT_PREFIX(), i, fd, status, "", (readfds != NULL&&FD_ISSET(fd, readfds)), (writefds != NULL && FD_ISSET(fd, writefds)), (exceptfds != NULL && FD_ISSET(fd, exceptfds)), (status & BAIO_BUFFERED_DATA_AVAILABLE_FOR_SSL_READ)); fflush(stdout);}
            // first check for ssl_pending
            if (exceptfds != NULL && FD_ISSET(fd, exceptfds)) {
                res ++;
                baioImmediateDeactivate(bb);
                goto nextfd;
            }
            if (status & BAIO_BUFFERED_DATA_AVAILABLE_FOR_SSL_READ) {
                baioOnCanSslRead(bb);
                res ++;
            } else if ((readfds == NULL || FD_ISSET(fd, readfds) == 0) && (writefds == NULL || FD_ISSET(fd, writefds) == 0)) {
                // no activity on this fd, status remains unchanged
                bb->status = status;
            } else {
                if (readfds != NULL && FD_ISSET(fd, readfds)) {
                    res ++;
                    if (status & BAIO_BLOCKED_FOR_READ_IN_TCPIP_LISTEN) {
                        baioOnTcpIpListenActivated(bb);
                        if (jsVarDebugLevel > 30) baioWriteBufferDump(&bb->writeBuffer);
                        goto nextfd;
                    } 
                    if (status & BAIO_BLOCKED_FOR_READ_IN_SSL_CONNECT) {
                        baioSslConnect(bb);
                        if (jsVarDebugLevel > 30) baioWriteBufferDump(&bb->writeBuffer);
                        goto nextfd;
                    } 
                    if (status & BAIO_BLOCKED_FOR_READ_IN_SSL_ACCEPT) {
                        baioSslAccept(bb);
                        if (jsVarDebugLevel > 30) baioWriteBufferDump(&bb->writeBuffer);
                        goto nextfd;
                    } 
                    if (status & BAIO_BLOCKED_FOR_READ_IN_READ) {
                        baioOnCanRead(bb);
                    } 
                    if (status & BAIO_BLOCKED_FOR_READ_IN_SSL_READ) {
                        baioOnCanSslRead(bb);
                    } 
                    if (status & BAIO_BLOCKED_FOR_READ_IN_SSL_WRITE) {
                        baioOnCanWrite(bb);
                    } 
                }
                if (writefds != NULL && FD_ISSET(fd, writefds)) {
                    res ++;
                    if (status & BAIO_BLOCKED_FOR_WRITE_IN_TCPIP_CONNECT) {
                        baioOnTcpIpConnected(bb);
                        if (jsVarDebugLevel > 30) baioWriteBufferDump(&bb->writeBuffer);
                        goto nextfd;
                    } 
                    if (status & BAIO_BLOCKED_FOR_WRITE_IN_SSL_CONNECT) {
                        baioSslConnect(bb);
                        if (jsVarDebugLevel > 30) baioWriteBufferDump(&bb->writeBuffer);
                        goto nextfd;
                    } 
                    if (status & BAIO_BLOCKED_FOR_WRITE_IN_SSL_ACCEPT) {
                        baioSslAccept(bb);
                        if (jsVarDebugLevel > 30) baioWriteBufferDump(&bb->writeBuffer);
                        goto nextfd;
                    } 
                    if (status & BAIO_BLOCKED_FOR_WRITE_IN_WRITE) {
                        baioOnCanWrite(bb);
                    }
                    if (status & BAIO_BLOCKED_FOR_WRITE_IN_SSL_READ) {
                        baioOnCanSslRead(bb);
                    } 
                    if (status & BAIO_BLOCKED_FOR_WRITE_IN_SSL_WRITE) {
                        baioOnCanWrite(bb);
                    } 
                }
            }
        } else {
            // fd was not watched at this tick, do not change status
            bb->status = status;
        }
        if (jsVarDebugLevel > 40) baioWriteBufferDump(&bb->writeBuffer);
#if EDEBUG
        assert(bb->writeBuffer.ij >= bb->writeBuffer.i);
#endif
    nextfd:;
    }
    return(res);
}

//////////////////////////////////////////////////////////

int baioSelect(int maxfd, fd_set *r, fd_set *w, fd_set *e, struct timeval *t) {
    int             res;
#if _WIN32
    int             i, fd;
    struct baio     *bb;

    // Remove all file related fds from select as Windows do not manage this.
    // Only sockets can be sent to select. Files are considered as ready to read/write.
    uint8_t         imask[BAIO_MAX_CONNECTIONS];
    for(i=0; i<baioTabMax; i++) {
        imask[i] = 0;
        bb = baioTab[i];
        if (bb != NULL && ! baioFdIsSocket(bb)) {
            fd = bb->fd;
            if (FD_ISSET(fd, r)) {imask[i] |= 0x1; FD_CLR(fd, r);}
            if (FD_ISSET(fd, w)) {imask[i] |= 0x2; FD_CLR(fd, w);}
            if (FD_ISSET(fd, e)) {imask[i] |= 0x4; FD_CLR(fd, e);}
        }
    }
    if (maxfd == 1) {
        Sleep(t->tv_sec*1000 + t->tv_usec/1000);
        res = 0;
    } else {
        res = select(maxfd, r, w, e, t);
    }
    for(i=0; i<baioTabMax; i++) {
        bb = baioTab[i];
        if (bb != NULL) {
            fd = bb->fd;
            if (imask[i] & 0x1) {FD_SET(fd, r);}
            if (imask[i] & 0x2) {FD_SET(fd, w);}
        }
    }
#else
    res = select(maxfd, r, w, e, t);
#endif  

    return(res);
}

int baioPoll2(int timeOutUsec, int (*addUserFds)(int maxfd, fd_set *r, fd_set *w, fd_set *e), void (*processUserFds)(int maxfd, fd_set *r, fd_set *w, fd_set *e)) {
    fd_set          r, w, e;
    struct timeval  t;
    int             maxfd, res;

    FD_ZERO(&r);
    FD_ZERO(&w);
    FD_ZERO(&e);

    t.tv_sec = timeOutUsec / 1000000LL;
    t.tv_usec = timeOutUsec % 1000000LL; 
    maxfd = baioSetSelectParams(0, &r, &w, &e, &t);
    if (addUserFds != NULL) maxfd = addUserFds(maxfd, &r, &w, &e);
    // printf("calling select, maxfd == %d\n", maxfd);fflush(stdout);
    res = baioSelect(maxfd, &r, &w, &e, &t);
    if (res < 0) {
        if (errno != EINTR) {
            printf("%s:%s:%d: ERROR: select returned %d and errno == %d (%s)!\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, res, errno, strerror(errno));
        }
        FD_ZERO(&r); FD_ZERO(&w); FD_ZERO(&e); 
    }
    // printf("returning from select, maxfd == %d\n", maxfd);fflush(stdout);
    if (processUserFds != NULL) processUserFds(maxfd, &r, &w, &e);
    baioOnSelectEvent(maxfd, &r, &w, &e);
    return(res);
}

int baioPoll(int timeOutUsec) {
    int res;
    res =  baioPoll2(timeOutUsec, NULL, NULL);
    return(res);
}

//////////////////////////////////////////////////////////
// baio file

struct baio *baioNewFile(char *path, int ioDirection, int additionalSpaceToAllocate) {
    struct baio     *bb;
    int             r, fd;
    unsigned        flags;

    if (path == NULL) return(NULL);

    if (ioDirection == BAIO_IO_DIRECTION_READ) flags = (O_RDONLY);
    else if (ioDirection == BAIO_IO_DIRECTION_WRITE) flags = (O_WRONLY | O_CREAT);
    else if (ioDirection == BAIO_IO_DIRECTION_RW) flags = (O_RDWR | O_CREAT);
    else {
        printf("%s:%s:%d: Invalid ioDirection\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        return(NULL);
    }

    // if not windows
    // flags |= O_NONBLOCK;

    // printf("%s: Opening file %s:%o\n", JSVAR_PRINT_PREFIX(), path, flags);

    fd = open(path, flags, 00644);
#if ! _WIN32
    r = jsVarSetFileNonBlocking(fd);
    if (r < 0) {
        printf("%s: Error: Can't set file descriptor to non-blocking state, closing it.", JSVAR_PRINT_PREFIX());
        close(fd);
        return(NULL);
    }
#endif

    bb = baioNewBasic(BAIO_TYPE_FILE, ioDirection, additionalSpaceToAllocate);
    bb->fd = fd;
    return(bb);
}

//////////////////////////////////////////////////////////
// pseudo read file
// This is used to provide a single string in form of buffered input (for assynchronous copy, for example)
// It is used mainly for sending precalculated html pages in web server.

struct baio *baioNewPseudoFile(char *string, int stringLength, int additionalSpaceToAllocate) {
    struct baio     *bb;
    int             r;

    if (string == NULL) return(NULL);

    // printf("%s: Opening pseudo file\n", JSVAR_PRINT_PREFIX());

    bb = baioNewBasic(BAIO_TYPE_FILE, BAIO_IO_DIRECTION_READ, additionalSpaceToAllocate);
    bb->status |= BAIO_STATUS_EOF_READ;
    r = baioReadBufferResize(&bb->readBuffer, stringLength, stringLength);
    if (r < 0) {
        baioImmediateDeactivate(bb);
        return(NULL);
    }
    memmove(bb->readBuffer.b, string, stringLength);
    bb->readBuffer.i = 0;
    bb->readBuffer.j = stringLength;

    return(bb);
}

//////////////////////////////////////////////////////////
// baio piped file

struct baio *baioNewPipedFile(char *path, int ioDirection, int additionalSpaceToAllocate) {
    struct baio     *bb;
    int             r, fd;
    char            *iod;

#if _WIN32
    printf("%s:%s:%d: Piped files are not available for Windows system. Use socket file instaed.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    return(NULL);
#else

    if (path == NULL) return(NULL);

    if (ioDirection == BAIO_IO_DIRECTION_READ) iod = "r";
    else if (ioDirection == BAIO_IO_DIRECTION_WRITE) iod = "w";
    else {
        printf("%s:%s:%d: Invalid ioDirection: piped file can be open for read only or write only.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        return(NULL);
    }


    fd = jsVarPopen2File(path, iod);
    if (fd < 0) return(NULL);
    r = jsVarSetFileNonBlocking(fd);
    if (r < 0) {
        printf("%s: Error: Can't set file descriptor to non-blocking state, closing it.\n", JSVAR_PRINT_PREFIX());
        close(fd);
        return(NULL);
    }

    // printf("%s: Opening piped file %s\n", JSVAR_PRINT_PREFIX(), path);

    bb = baioNewBasic(BAIO_TYPE_PIPED_FILE, ioDirection, additionalSpaceToAllocate);
    bb->fd = fd;
    return(bb);
#endif
}

//////////////////////////////////////////////////////////
// baio piped command

struct baio *baioNewPipedCommand(char *command, int ioDirection, int additionalSpaceToAllocate) {
    struct baio     *bb;
    int             r, fd;
    char            *iod;

#if _WIN32
    printf("%s:%s:%d: Piped commandss are not available for Windows system. \n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    return(NULL);
#else

    if (command == NULL) return(NULL);
    if (ioDirection == BAIO_IO_DIRECTION_READ) {
        r = jsVarPopen2(command, &fd, NULL, 1);
    } else if (ioDirection == BAIO_IO_DIRECTION_WRITE) {
        r = jsVarPopen2(command, NULL, &fd, 1);
    } else {
        printf("%s:%s:%d: Invalid ioDirection: piped command can be open for read only or write only.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        r = -1;
    }
    if (r < 0) return(NULL);
    if (fd < 0) return(NULL);
    r = jsVarSetFileNonBlocking(fd);
    if (r < 0) {
        printf("%s: Error: Can't set file descriptor to non-blocking state, closing it.\n", JSVAR_PRINT_PREFIX());
        close(fd);
        return(NULL);
    }
    bb = baioNewBasic(BAIO_TYPE_PIPED_COMMAND, ioDirection, additionalSpaceToAllocate);
    bb->fd = fd;
    return(bb);
#endif
}

//////////////////////////////////////////////////////////
// baio TCP/IP network

static int baioSetUseSslFromSslFlag(struct baio *bb, enum baioSslFlags sslFlag) {
    if (sslFlag == BAIO_SSL_NO) {
        bb->useSsl = 0;
        return(0);
    } else if (sslFlag == BAIO_SSL_YES) {
#if BAIO_USE_OPEN_SSL || WSAIO_USE_OPEN_SSL || JSVAR_USE_OPEN_SSL
        bb->useSsl = 1;
        return(0);
#else
        return(-1);
#endif
    } else {
        return(1);
    }
}

static int baioGetSockAddr(char *hostName, int port, struct sockaddr *out_saddr, int *out_saddrlen) {
#if 1 || _WIN32
    int                     r;
    struct addrinfo         *server, hints;
    char                    ps[JSVAR_TMP_STRING_SIZE];

    server = NULL;
    memset(&hints, 0, sizeof(hints) );
    // hints.ai_family = AF_UNSPEC;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    sprintf(ps, "%d", port);
    // Resolve the server address and port
    r = getaddrinfo(hostName, ps, &hints, &server);
    if ( r != 0 ) {
        printf("%s: getaddrinfo %s:%d failed with error: %d\n", JSVAR_PRINT_PREFIX(), hostName, port, r);
        return(-1);
    }
    if (server == NULL) {
        printf("%s: getaddrinfo gives no result\n", JSVAR_PRINT_PREFIX());
        return(-1);
    }
    if (out_saddr == NULL) return(-1);
    if (*out_saddrlen < (int)server->ai_addrlen) return(-1);
    *out_saddrlen = (int)server->ai_addrlen;
    memcpy(out_saddr, server->ai_addr, *out_saddrlen);
#else
    struct hostent      *server;
    int                 n;

    server = gethostbyname(hostName);
    if (server == NULL) return(-1);
    if (out_saddr == NULL) return(-1);
    if (*out_saddrlen < sizeof(struct sockaddr_in)) return(-1);
    *out_saddrlen = sizeof(struct sockaddr_in);
    memset(out_saddr, 0, sizeof(struct sockaddr_in));
    memcpy(&((struct sockaddr_in *)out_saddr)->sin_addr.s_addr, server->h_addr, server->h_length);
    ((struct sockaddr_in *)out_saddr)->sin_port = htons(port);
    ((struct sockaddr_in *)out_saddr)->sin_family = AF_INET;
#endif
    return(0);
}

struct baio *baioNewTcpipClient(char *hostName, int port, enum baioSslFlags sslFlag, int additionalSpaceToAllocate) {
    struct baio             *bb;
    int                     r, fd;
    struct sockaddr         saddr;
    int                     saddrlen;


    saddrlen = sizeof(saddr);
    r = baioGetSockAddr(hostName, port, &saddr, &saddrlen);

    if (r != 0) return(NULL);
    if (saddrlen < 0) return(NULL);

    bb = baioNewBasic(BAIO_TYPE_TCPIP_CLIENT, BAIO_IO_DIRECTION_RW, additionalSpaceToAllocate);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("%s: %s:%d: Can't create socket\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        baioImmediateDeactivate(bb);
        return(NULL);
    }

    r = jsVarSetSocketNonBlocking(fd);
    if (r < 0) {
        printf("%s: %s:%d: Can't set socket non blocking\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        goto failreturn;
    }

    bb->fd = fd;
    r = baioSetUseSslFromSslFlag(bb, sslFlag);
    if (r < 0) {
        printf("%s: %s:%d: Can't set ssl. Baio is compiled without ssl support\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        goto failreturn;
    } else if (r > 0) {
        printf("%s: %s:%d: Warning: wrong value for sslFlag\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    }
    // SNI support?
    bb->sslSniHostName = strDuplicate(hostName);

    if (jsVarDebugLevel > 5) printf("%s: Connecting to %s:%d\n", JSVAR_PRINT_PREFIX(), jsVarSprintfIpAddress_st(((struct sockaddr_in*)&saddr)->sin_addr.s_addr), ntohs(((struct sockaddr_in*)&saddr)->sin_port));

    // connect
    r = connect(fd, &saddr, saddrlen);
#if _WIN32
    if (r == 0 || (r == SOCKET_ERROR && (WSAGetLastError() == EINPROGRESS || WSAGetLastError() == WSAEWOULDBLOCK))) 
#else
    if (r == 0 || (r < 0 && errno == EINPROGRESS)) 
#endif
    {
        // connected or pending connect
        // do not immediately call sslconnect even if connected here, rather return bb let the user to setup
        // callbacks go through select and continue there
        bb->status |= BAIO_BLOCKED_FOR_WRITE_IN_TCPIP_CONNECT;
    } else {
        printf("%s: %s:%d: Connect returned %d: %s!\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, r, JSVAR_STR_ERRNO());
        printf("%s: %s", JSVAR_PRINT_PREFIX(), JSVAR_STR_ERRNO());
        goto failreturn;
    }
    return(bb);

failreturn:
    closesocket(fd);
    baioImmediateDeactivate(bb);
    return(NULL);
}

struct baio *baioNewTcpipServer(int port, enum baioSslFlags sslFlag, int additionalSpaceToAllocate) {
    struct baio             *bb;
    struct sockaddr_in      sockaddr;
    int                     r, fd;
#if _WIN32
    DWORD                   one;
#else
    int                     one;
#endif

    if (port < 0) return(NULL);

    bb = baioNewBasic(BAIO_TYPE_TCPIP_SERVER, BAIO_IO_DIRECTION_RW, additionalSpaceToAllocate);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("%s: %s:%d: Can't create socket\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        baioImmediateDeactivate(bb);
        return(NULL);
    }

    one = 1;
    r = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *) &one, sizeof(one));
    if (r < 0) {
        printf("%s: %s:%d: Can't set socket reusable\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        goto failreturn;
    }

    r = jsVarSetSocketNonBlocking(fd);
    if (r < 0) {
        printf("%s: %s:%d: Can't set socket non blocking\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        goto failreturn;
    }

    r = baioSetUseSslFromSslFlag(bb, sslFlag);
    if (r < 0) {
        printf("%s: %s:%d: Can't set ssl. Baio is compiled without ssl support\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        goto failreturn;
    } else if (r > 0) {
        printf("%s: %s:%d: Warning: wrong value for sslFlag\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    }

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    // next line shall restrict connections on loppback device only
    // sockaddr.sin_addr.s_addr = htonl(2130706433L)
    sockaddr.sin_port = htons(port);

    if (jsVarDebugLevel > 0) printf("%s: Listening on %d\n", JSVAR_PRINT_PREFIX(), port);

    r = bind(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
    if (r < 0) {
        printf("%s: %s:%d: Bind to %d failed: %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, port, JSVAR_STR_ERRNO());
        goto failreturn;
    }

    r = listen(fd, 128);
    if (r < 0) {
        printf("%s: %s:%d: Listen for %d failed: %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, port, JSVAR_STR_ERRNO());
        goto failreturn;
    }

    bb->status |= BAIO_BLOCKED_FOR_READ_IN_TCPIP_LISTEN;
    bb->fd = fd;
    return(bb);

failreturn:
    closesocket(fd);
    baioImmediateDeactivate(bb);
    return(NULL);
}


//////////////////////////////////////////////////////////
// baio socket file

// This is a simple TCP/IP server, accepting connection and sending/reading a file content.
#if _WIN32
DWORD WINAPI baioSocketFileServerThreadStartRoutine(LPVOID arg)
#else
void *baioSocketFileServerThreadStartRoutine(void *arg) 
#endif
{
    struct baioOpenSocketFileData   *targ;
    int                             newsockfd;
    socklen_t                       clilen;
    struct sockaddr_in              cli_addr;
    int                             r, n, nn;
    FILE                            *ff;
    char                            buffer[BAIO_SOCKET_FILE_BUFFER_SIZE];
    fd_set                          rset;
    struct timeval                  tout;

    newsockfd = -1;

    // signal(SIGCHLD, zombieHandler);              // avoid system of keeping child zombies
    targ = (struct baioOpenSocketFileData *) arg;

    // This is a kind of timeout, if the main thread does not connect within 10 seconds, exit
    FD_ZERO(&rset);
    FD_SET(targ->sockFd, &rset);
    tout.tv_sec = 10;
    tout.tv_usec = 0;
    r = select(targ->sockFd+1, &rset, NULL, NULL, &tout);

    // If timeout, abort thread
    if (r == 0) goto finito;

    clilen = sizeof(cli_addr);
    newsockfd = accept(targ->sockFd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0) {
        printf("%s:%s:%d: baioOpenSocketFileStartRoutine: Error on accept: %s", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
        goto finito;
    }
    //printf("%s: baioOpenSocketFileServerThread: Connection Accepted for %s\n", JSVAR_PRINT_PREFIX(), targ->path);

    if (targ->ioDirection == BAIO_IO_DIRECTION_READ) {
        ff = fopen(targ->path, "rb");
        if (ff == NULL) {
            printf("%s:%s:%d: baioOpenSocketFileStartRoutine: Cant' open %s for read: %s", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, targ->path, strerror(errno));
            goto finito;
        }
        do {
            n = fread(buffer, 1, BAIO_SOCKET_FILE_BUFFER_SIZE, ff);
            //printf("%s: baioOpenSocketFileServerThread: Read %d bytes from %s\n", JSVAR_PRINT_PREFIX(), n, targ->path);
            if (n > 0) {
                nn = send(newsockfd, buffer, n, 0);
                if (n != nn) printf("%s: baioOpenSocketFileServerThread: Sent %d bytes from %s while read %d\n", JSVAR_PRINT_PREFIX(), nn, targ->path, n);              
            }
        } while (n == BAIO_SOCKET_FILE_BUFFER_SIZE);
        fclose(ff);
    } else if (targ->ioDirection == BAIO_IO_DIRECTION_WRITE) {
        ff = fopen(targ->path, "wb");
        if (ff == NULL) {
            printf("%s:%s:%d: baioOpenSocketFileStartRoutine: Cant' open %s for write: %s", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, targ->path, strerror(errno));
            goto finito;
        }
        do {
            n = recv(newsockfd, buffer, BAIO_SOCKET_FILE_BUFFER_SIZE, 0);
            if (n > 0) n = fwrite(buffer, n, 1, ff);
        } while (n > 0);
        fclose(ff);
    }

finito:
    if (newsockfd >= 0) closesocket(newsockfd);
    closesocket(targ->sockFd);
    free(targ);
#if _WIN32
    return(0);
#else
    return(NULL);
#endif
}

// returns port number
static int baioSocketFileLaunchServerThread(char *path, int ioDirection) {
    int                             r, pathlen;
    struct baioOpenSocketFileData   *targ;
    int                             sockfd, portno;
    socklen_t                       slen;
    struct sockaddr_in              serv_addr;

    static int                      baioOpenSocketFileDisabled = 0;

    if (baioOpenSocketFileDisabled) return(-1);

    // Start a simple TCP/IP server, which will listen in another thread
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("%s:%s:%d: baioOpenSocketFileStartRoutine: Can't get socket: %s", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
        return(-1);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = 0;     // port number 0 informs the system that any port can be taken
    r = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (r < 0) {
        printf("%s:%s:%d: baioOpenSocketFileStartRoutine: Can't bind: %s", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
        closesocket(sockfd);
        return(-1);
    }
    slen = sizeof(serv_addr);
    r = getsockname(sockfd, (struct sockaddr *) &serv_addr, &slen);
    if (r < 0) {
        printf("%s:%s:%d: baioOpenSocketFileStartRoutine: Getsockname failed: %s", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
        closesocket(sockfd);
        return(-1);
    }
    portno = ntohs(serv_addr.sin_port);
    r = listen(sockfd, 1);
    if (r < 0) {
        printf("%s:%s:%d: baioOpenSocketFileStartRoutine: listen failed: %s", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
        closesocket(sockfd);
        return(-1);
    }

    pathlen = strlen(path);
    targ = (struct baioOpenSocketFileData *) malloc(sizeof(struct baioOpenSocketFileData) + pathlen + 1);
    targ->ioDirection = ioDirection;
    strcpy(targ->path, path);
    targ->sockFd = sockfd;

#if _WIN32

    {
        HANDLE ttt;
        ttt = CreateThread(NULL, 0, baioSocketFileServerThreadStartRoutine, targ, 0, NULL);
        if (ttt == NULL) {
            printf("%s:%s:%d: CreateThread failed in baioSocketFileLaunchServerThread\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            closesocket(sockfd);
            return(-1);
        }
        // detach the thread
        r = CloseHandle(ttt);
        if (r == 0) {
            printf("%s: %s:%d: Error: baioOpenSocketFile can't CloseHandle. Preventing any other socket files!\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            // Be severe here. Otherwise there is a risk to exhaust thread limit in which case
            // the whole task would be terminated by the system.
            baioOpenSocketFileDisabled = 1;
        }
    }

#else

#if JSVAR_PTHREADS
    {
        pthread_t ttt;
        r = pthread_create(&ttt, NULL, baioSocketFileServerThreadStartRoutine, targ);
        if (r) {
            printf("%s:%s:%d: pthread_create failed in baioSocketFileLaunchServerThread: %s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
            closesocket(sockfd);
            return(-1);
        }
        r = pthread_detach(ttt);
        if (r) {
            printf("%s: %s:%d: Error: baioOpenSocketFile can't detach thread: %s. Preventing any other socket files!\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
            // Be severe here. Otherwise there is a risk to exhaust thread limit in which case
            // the whole task would be terminated by the system.
            baioOpenSocketFileDisabled = 1;
        }
    }
#else
    printf("%s: %s:%d: Error: JSVAR must be linked with pthreads and macro JSVAR_PTHREADS defined to use socket files on Unix systems!\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
#endif
#endif

    return(portno);
}

struct baio *baioNewSocketFile(char *path, int ioDirection, int additionalSpaceToAllocate) {
    struct baio         *bb;
    int                 portno;
    struct sockaddr_in  serv_addr;
    struct hostent      *server;

    if (path == NULL) return(NULL);

    if (ioDirection != BAIO_IO_DIRECTION_READ && ioDirection != BAIO_IO_DIRECTION_WRITE) {
        printf("%s:%s:%d: Invalid ioDirection: socket file can be open for read only or write only.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        return(NULL);
    }
    portno = baioSocketFileLaunchServerThread(path, ioDirection);
    if (portno < 0) return(NULL);

    // printf("Socket file: using port %d\n", portno);
    // printf("%s: Opening socket file %s\n", JSVAR_PRINT_PREFIX(), path);

    // TODO: I need some elementary synchronization, not to connect before the thread is accepting connections
    // usleep(1);
    bb = baioNewTcpipClient("127.0.0.1", portno, BAIO_SSL_NO, additionalSpaceToAllocate);
    return(bb);
}


















/////////////////////////////////////////////////////////////////////////////////////////
// Wsaio stands for Web and Websocket Server based on baio Assynchronous I/O


#define WSAIO_CHUNKED_ANSWER_LENGTH_SPACE_RESERVE   10

#define WSAIO_WEBSOCKET_GUID            "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

#define WSAIO_WEBSOCKET_FIN_MSK         0x80
#define WSAIO_WEBSOCKET_RSV1_MSK        0x40
#define WSAIO_WEBSOCKET_RSV2_MSK        0x20
#define WSAIO_WEBSOCKET_RSV3_MSK        0x10

#define WSAIO_WEBSOCKET_OP_CODE_MSK             0x0f
#define WSAIO_WEBSOCKET_OP_CODE_CONTINUATION    0x00
#define WSAIO_WEBSOCKET_OP_CODE_TEXT            0x01
#define WSAIO_WEBSOCKET_OP_CODE_BIN             0x02
#define WSAIO_WEBSOCKET_OP_CODE_CLOSED          0x08
#define WSAIO_WEBSOCKET_OP_CODE_PING            0x09
#define WSAIO_WEBSOCKET_OP_CODE_PONG            0x0a

#define WSAIO_WEBSOCKET_MASK_MSK        0x80
#define WSAIO_WEBSOCKET_PLEN_MSK        0x7f

#define WSAIO_SEND_FILE_BUFFER_SIZE     1400 /* a value around a typical MTU (1500) minus IP, TCP and TLS header and some reserve */

enum wsaioStates {
    WSAIO_STATE_WAITING_FOR_WWW_REQUEST,
    WSAIO_STATE_WAITING_PROCESSING_WWW_REQUEST,
    WSAIO_STATE_WEBSOCKET_ACTIVE,
};

enum wsaioHttpHeaders {
    WSAIO_HTTP_HEADER_NONE,
    WSAIO_HTTP_HEADER_Connection,
    WSAIO_HTTP_HEADER_Host,
    WSAIO_HTTP_HEADER_Origin,
    WSAIO_HTTP_HEADER_Upgrade,

    WSAIO_HTTP_HEADER_Accept_Encoding,
    WSAIO_HTTP_HEADER_Content_Length,

    WSAIO_HTTP_HEADER_Sec_WebSocket_Key,
    WSAIO_HTTP_HEADER_Sec_WebSocket_Protocol,
    WSAIO_HTTP_HEADER_Sec_WebSocket_Version,
    WSAIO_HTTP_HEADER_MAX,
};

static char *wsaioHttpHeader[WSAIO_HTTP_HEADER_MAX];
static int  wsaioHttpHeaderLen[WSAIO_HTTP_HEADER_MAX];


//////////////////////////////////////////////////////////////////////////////////

#if 0
static char *wsaioStrnchr(char *s, int len, int c) {
    char *send;

    if (s == NULL) return(NULL);
    send = s+len;
    while (s<send && *s != c) s++;
    if (s < send) return(s);
    return(NULL);
}
#endif

static int wsaioHexDigitToInt(int hx) {
    if (hx >= '0' && hx <= '9') return(hx - '0');
    if (hx >= 'A' && hx <= 'F') return(hx - 'A' + 10);
    if (hx >= 'a' && hx <= 'f') return(hx - 'a' + 10);
    return(-1);
}

/* 
If OpenSSL is not linked with jsvar, we need another implementation of SHA1.
The SHA1 code originates from https://github.com/clibs/sha1, all credits
for it belongs to Steve Reid.
*/
#if BAIO_USE_OPEN_SSL || WSAIO_USE_OPEN_SSL || JSVAR_USE_OPEN_SSL

// Using OpenSSL, use SHA1 from openSsl
#define SHA1_DIGEST_LENGTH                  SHA_DIGEST_LENGTH 
#define JSVAR_SHA1(s, r)                    SHA1(s, strlen((const char *)s), r);


#else


// Not using OpenSSL, redefined functions and data types provided by OpenSsl to dummy 
// or alternative implementations
#define SHA1_DIGEST_LENGTH                  20
#define JSVAR_SHA1(s, r)                    SHA1((char *)r, (char *)s, strlen((const char *)s));

/////////////////////////////////////////////////////////
// start of included sha1.[hc] code
/*
SHA-1 in C
By Steve Reid <steve@edmweb.com>
100% Public Domain
*/
/* #define LITTLE_ENDIAN * This should be #define'd already, if true. */
/* #define SHA1HANDSOFF * Copies data before messing with it. */

#define SHA1HANDSOFF

typedef struct
{
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
} SHA1_CTX;

void SHA1Transform(
    uint32_t state[5],
    const unsigned char buffer[64]
    );

void SHA1Init(
    SHA1_CTX * context
    );

void SHA1Update(
    SHA1_CTX * context,
    const unsigned char *data,
    uint32_t len
    );

void SHA1Final(
    unsigned char digest[20],
    SHA1_CTX * context
    );

void SHA1(
    char *hash_out,
    const char *str,
    int len);

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))
#if BYTE_ORDER == LITTLE_ENDIAN
#define blk0(i) (block->l[i] = (rol(block->l[i],24)&0xFF00FF00) \
    |(rol(block->l[i],8)&0x00FF00FF))
#elif BYTE_ORDER == BIG_ENDIAN
#define blk0(i) block->l[i]
#else
#error "Endianness not defined!"
#endif
#define blk(i) (block->l[i&15] = rol(block->l[(i+13)&15]^block->l[(i+8)&15] \
    ^block->l[(i+2)&15]^block->l[i&15],1))

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);

/* Hash a single 512-bit block. This is the core of the algorithm. */

void SHA1Transform(
    uint32_t state[5],
    const unsigned char buffer[64]
)
{
    uint32_t a, b, c, d, e;

    typedef union
    {
        unsigned char c[64];
        uint32_t l[16];
    } CHAR64LONG16;

#ifdef SHA1HANDSOFF
    CHAR64LONG16 block[1];      /* use array to appear as a pointer */

    memcpy(block, buffer, 64);
#else
    /* The following had better never be used because it causes the
     * pointer-to-const buffer to be cast into a pointer to non-const.
     * And the result is written through.  I threw a "const" in, hoping
     * this will cause a diagnostic.
     */
    CHAR64LONG16 *block = (const CHAR64LONG16 *) buffer;
#endif
    /* Copy context->state[] to working vars */
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    /* 4 rounds of 20 operations each. Loop unrolled. */
    R0(a, b, c, d, e, 0);
    R0(e, a, b, c, d, 1);
    R0(d, e, a, b, c, 2);
    R0(c, d, e, a, b, 3);
    R0(b, c, d, e, a, 4);
    R0(a, b, c, d, e, 5);
    R0(e, a, b, c, d, 6);
    R0(d, e, a, b, c, 7);
    R0(c, d, e, a, b, 8);
    R0(b, c, d, e, a, 9);
    R0(a, b, c, d, e, 10);
    R0(e, a, b, c, d, 11);
    R0(d, e, a, b, c, 12);
    R0(c, d, e, a, b, 13);
    R0(b, c, d, e, a, 14);
    R0(a, b, c, d, e, 15);
    R1(e, a, b, c, d, 16);
    R1(d, e, a, b, c, 17);
    R1(c, d, e, a, b, 18);
    R1(b, c, d, e, a, 19);
    R2(a, b, c, d, e, 20);
    R2(e, a, b, c, d, 21);
    R2(d, e, a, b, c, 22);
    R2(c, d, e, a, b, 23);
    R2(b, c, d, e, a, 24);
    R2(a, b, c, d, e, 25);
    R2(e, a, b, c, d, 26);
    R2(d, e, a, b, c, 27);
    R2(c, d, e, a, b, 28);
    R2(b, c, d, e, a, 29);
    R2(a, b, c, d, e, 30);
    R2(e, a, b, c, d, 31);
    R2(d, e, a, b, c, 32);
    R2(c, d, e, a, b, 33);
    R2(b, c, d, e, a, 34);
    R2(a, b, c, d, e, 35);
    R2(e, a, b, c, d, 36);
    R2(d, e, a, b, c, 37);
    R2(c, d, e, a, b, 38);
    R2(b, c, d, e, a, 39);
    R3(a, b, c, d, e, 40);
    R3(e, a, b, c, d, 41);
    R3(d, e, a, b, c, 42);
    R3(c, d, e, a, b, 43);
    R3(b, c, d, e, a, 44);
    R3(a, b, c, d, e, 45);
    R3(e, a, b, c, d, 46);
    R3(d, e, a, b, c, 47);
    R3(c, d, e, a, b, 48);
    R3(b, c, d, e, a, 49);
    R3(a, b, c, d, e, 50);
    R3(e, a, b, c, d, 51);
    R3(d, e, a, b, c, 52);
    R3(c, d, e, a, b, 53);
    R3(b, c, d, e, a, 54);
    R3(a, b, c, d, e, 55);
    R3(e, a, b, c, d, 56);
    R3(d, e, a, b, c, 57);
    R3(c, d, e, a, b, 58);
    R3(b, c, d, e, a, 59);
    R4(a, b, c, d, e, 60);
    R4(e, a, b, c, d, 61);
    R4(d, e, a, b, c, 62);
    R4(c, d, e, a, b, 63);
    R4(b, c, d, e, a, 64);
    R4(a, b, c, d, e, 65);
    R4(e, a, b, c, d, 66);
    R4(d, e, a, b, c, 67);
    R4(c, d, e, a, b, 68);
    R4(b, c, d, e, a, 69);
    R4(a, b, c, d, e, 70);
    R4(e, a, b, c, d, 71);
    R4(d, e, a, b, c, 72);
    R4(c, d, e, a, b, 73);
    R4(b, c, d, e, a, 74);
    R4(a, b, c, d, e, 75);
    R4(e, a, b, c, d, 76);
    R4(d, e, a, b, c, 77);
    R4(c, d, e, a, b, 78);
    R4(b, c, d, e, a, 79);
    /* Add the working vars back into context.state[] */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    /* Wipe variables */
    a = b = c = d = e = 0;
#ifdef SHA1HANDSOFF
    memset(block, '\0', sizeof(block));
#endif
}


/* SHA1Init - Initialize new context */

void SHA1Init(
    SHA1_CTX * context
)
{
    /* SHA1 initialization constants */
    context->state[0] = 0x67452301;
    context->state[1] = 0xEFCDAB89;
    context->state[2] = 0x98BADCFE;
    context->state[3] = 0x10325476;
    context->state[4] = 0xC3D2E1F0;
    context->count[0] = context->count[1] = 0;
}


/* Run your data through this. */

void SHA1Update(
    SHA1_CTX * context,
    const unsigned char *data,
    uint32_t len
)
{
    uint32_t i;

    uint32_t j;

    j = context->count[0];
    if ((context->count[0] += len << 3) < j)
        context->count[1]++;
    context->count[1] += (len >> 29);
    j = (j >> 3) & 63;
    if ((j + len) > 63)
    {
        memcpy(&context->buffer[j], data, (i = 64 - j));
        SHA1Transform(context->state, context->buffer);
        for (; i + 63 < len; i += 64)
        {
            SHA1Transform(context->state, &data[i]);
        }
        j = 0;
    }
    else
        i = 0;
    memcpy(&context->buffer[j], &data[i], len - i);
}


/* Add padding and return the message digest. */

void SHA1Final(
    unsigned char digest[20],
    SHA1_CTX * context
)
{
    unsigned i;

    unsigned char finalcount[8];

    unsigned char c;

#if 0    /* untested "improvement" by DHR */
    /* Convert context->count to a sequence of bytes
     * in finalcount.  Second element first, but
     * big-endian order within element.
     * But we do it all backwards.
     */
    unsigned char *fcp = &finalcount[8];

    for (i = 0; i < 2; i++)
    {
        uint32_t t = context->count[i];

        int j;

        for (j = 0; j < 4; t >>= 8, j++)
            *--fcp = (unsigned char) t}
#else
    for (i = 0; i < 8; i++)
    {
        finalcount[i] = (unsigned char) ((context->count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) & 255);      /* Endian independent */
    }
#endif
    c = 0200;
    SHA1Update(context, &c, 1);
    while ((context->count[0] & 504) != 448)
    {
        c = 0000;
        SHA1Update(context, &c, 1);
    }
    SHA1Update(context, finalcount, 8); /* Should cause a SHA1Transform() */
    for (i = 0; i < 20; i++)
    {
        digest[i] = (unsigned char)
            ((context->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
    }
    /* Wipe variables */
    memset(context, '\0', sizeof(*context));
    memset(&finalcount, '\0', sizeof(finalcount));
}

void SHA1(
    char *hash_out,
    const char *str,
    int len)
{
    SHA1_CTX ctx;
    unsigned int ii;

    SHA1Init(&ctx);
    for (ii=0; ii<len; ii+=1)
        SHA1Update(&ctx, (const unsigned char*)str + ii, 1);
    SHA1Final((unsigned char *)hash_out, &ctx);
    hash_out[20] = '\0';
}

// end of SHA1 implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif









void wsaioWebsocketStartNewMessage(struct wsaio *ww) {
    // put there a gap of inifinite length (will be resize when the msg is finished)
    baioMsgPut(&ww->b, ww->b.writeBuffer.j, BAIO_INIFINITY_INDEX);
    baioMsgReserveSpace(&ww->b, 10);
}

// This function overwrites original parameter (decoded URL is never longer than original)
static char *wsaioUriDecode(char *uri) {
    char *s, *d;
    for(d=s=uri; *s; s++, d++) {
        if (*s == '+') *d = ' ';
        else if (*s == '%') {
            if (s[1] != ' ' && s[1] != 0 && s[2] != ' ' && s[2] != 0) {
                *d = wsaioHexDigitToInt(s[1]) * 16 + wsaioHexDigitToInt(s[2]);
                s += 2;
            }
        } else {
            *d = *s;
        }
    }
    *d = 0;
    return(d);
}

static char *wsaioStrFindTwoNewlines(char *s) {
    char *res;
    res = strstr(s, "\r\n\r\n");
    if (res != NULL) return(res);
    res = strstr(s, "\n\n");
    if (res != NULL) return(res);
    return(NULL);
}

#define WSAIO_HTTP_HEADER_SET(name, namelen, val, vallen, header) { \
        if (strncasecmp(name, #header, sizeof(#header)-1) == 0) {       \
            wsaioHttpHeader[WSAIO_HTTP_HEADER_##header] = val;          \
            wsaioHttpHeaderLen[WSAIO_HTTP_HEADER_##header] = vallen;        \
        }                                                               \
    }
#define WSAIO_HTTP_HEADER_SET2(name, namelen, val, vallen, header1, header2) { \
        if (strncasecmp(name, #header1 "-" #header2, sizeof(#header1)+sizeof(#header2)-1) == 0) {       \
            wsaioHttpHeader[WSAIO_HTTP_HEADER_##header1##_##header2] = val;         \
            wsaioHttpHeaderLen[WSAIO_HTTP_HEADER_##header1##_##header2] = vallen;       \
        }                                                               \
    }
#define WSAIO_HTTP_HEADER_SET3(name, namelen, val, vallen, header1, header2, header3) { \
        if (strncasecmp(name, #header1 "-" #header2 "-" #header3, sizeof(#header1)+sizeof(#header2)+sizeof(#header3)-1) == 0) {     \
            wsaioHttpHeader[WSAIO_HTTP_HEADER_##header1##_##header2##_##header3] = val;         \
            wsaioHttpHeaderLen[WSAIO_HTTP_HEADER_##header1##_##header2##_##header3] = vallen;       \
        }                                                               \
    }
#define WSAIO_HTTP_HEADER_EQUALS(headerIndex, headerValue) (            \
        wsaioHttpHeader[headerIndex] != NULL                            \
        && strlen(headerValue) == wsaioHttpHeaderLen[headerIndex]       \
        && strncasecmp(wsaioHttpHeader[headerIndex], headerValue, wsaioHttpHeaderLen[headerIndex]) == 0 \
        )

static int wsaioHttpHeaderContains(int headerIndex, char *str) {
    char    *s;
    int     len;

    len = strlen(str);
    s = wsaioHttpHeader[headerIndex];
    while (s != NULL && *s != 0) {
        if (strncasecmp(s, str, len) == 0) return(1);
        while (*s != '\n' && *s != ',' && *s != 0) s ++;
        if (*s != ',') return(0);
        s++;
        while (*s != 0 && isblank(*s)) s++ ;
    }
    return(0);
}

// parse http header. Only those header fields that we are interested in are tested
static void wsaioAddHttpHeaderField(char *name, int namelen, char *val, int vallen) {
    // check if this field is interesting for us and save it if yes
    if (name == NULL || namelen == 0) return;
    // printf("Adding header pair %.*s : %.*s\n", namelen, name, vallen, val);
    switch (name[0]) {
    case 'a':
    case 'A':
        WSAIO_HTTP_HEADER_SET2(name, namelen, val, vallen, Accept,Encoding);
        break;
    case 'c':
    case 'C':
        WSAIO_HTTP_HEADER_SET(name, namelen, val, vallen, Connection);
        WSAIO_HTTP_HEADER_SET2(name, namelen, val, vallen, Content,Length);
        break;
    case 'h':
    case 'H':
        WSAIO_HTTP_HEADER_SET(name, namelen, val, vallen, Host);
        break;
    case 'o':
    case 'O':
        WSAIO_HTTP_HEADER_SET(name, namelen, val, vallen, Origin);
        break;
    case 'u':
    case 'U':
        WSAIO_HTTP_HEADER_SET(name, namelen, val, vallen, Upgrade);
        break;
    case 's':
    case 'S':
        WSAIO_HTTP_HEADER_SET3(name, namelen, val, vallen, Sec,WebSocket,Key);
        WSAIO_HTTP_HEADER_SET3(name, namelen, val, vallen, Sec,WebSocket,Protocol);
        WSAIO_HTTP_HEADER_SET3(name, namelen, val, vallen, Sec,WebSocket,Version);
        break;
    }
}

static void wsaioOnWwwToWebsocketProtocolSwitchRequest(struct wsaio *ww, struct baio *bb, int requestHeaderLen, int contentLength, char *uri) {
    int             acceptKeyLen, keylen;
    char            acceptKey[JSVAR_TMP_STRING_SIZE];
    unsigned char   sha1hash[SHA1_DIGEST_LENGTH];
    char            ttt[JSVAR_TMP_STRING_SIZE];

    if (wsaioHttpHeader[WSAIO_HTTP_HEADER_Sec_WebSocket_Key] == NULL) goto wrongRequest;
    keylen = wsaioHttpHeaderLen[WSAIO_HTTP_HEADER_Sec_WebSocket_Key];
    if (keylen + sizeof(WSAIO_WEBSOCKET_GUID) >= JSVAR_TMP_STRING_SIZE-1) {
        printf("%s: %s:%d: Websocket key is too large.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
        goto wrongRequest;
    }
    strncpy(ttt, wsaioHttpHeader[WSAIO_HTTP_HEADER_Sec_WebSocket_Key], keylen);
    strcpy(ttt+keylen, WSAIO_WEBSOCKET_GUID);
    //printf("GOING TO HASH: %s\n", ttt);
    JSVAR_SHA1((unsigned char*)ttt, sha1hash);
    acceptKeyLen = jsVarBase64Encode((char*)sha1hash, SHA1_DIGEST_LENGTH, acceptKey, sizeof(acceptKey));
    //printf("accept key ssl:   %s\n", acceptKey);

    // Generate websocket accept header
    baioPrintfMsg(bb, 
                  "HTTP/1.1 101 Switching Protocols\r\n"
                  "Upgrade: websocket\r\n"
                  "Connection: Upgrade\r\n"
                  "Sec-WebSocket-Accept: %s\r\n"
                  "Sec-WebSocket-Protocol: %.*s\r\n"
                  "Sec-WebSocket-Origin: %.*s\r\n"
                  "Sec-WebSocket-Location: %s://%.*s%s\r\n"
                  // "Sec-WebSocket-Extensions: permessage-deflate\r\n"
                  "\r\n"
                  ,
                  acceptKey,
                  wsaioHttpHeaderLen[WSAIO_HTTP_HEADER_Sec_WebSocket_Protocol],
                  wsaioHttpHeader[WSAIO_HTTP_HEADER_Sec_WebSocket_Protocol],
                  wsaioHttpHeaderLen[WSAIO_HTTP_HEADER_Origin],wsaioHttpHeader[WSAIO_HTTP_HEADER_Origin],
                  (ww->b.useSsl?"wss":"ws"), wsaioHttpHeaderLen[WSAIO_HTTP_HEADER_Host],wsaioHttpHeader[WSAIO_HTTP_HEADER_Host],uri
        );

    bb->readBuffer.i += requestHeaderLen+contentLength;
    ww->state = WSAIO_STATE_WEBSOCKET_ACTIVE;
    ww->previouslySeenWebsocketFragmentsOffset = 0;
    wsaioWebsocketStartNewMessage(ww);

    if (jsVarDebugLevel > 0) printf("%s: %s:%d: Switching protocol to websocket.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    JSVAR_CALLBACK_CALL(ww->callBackOnWebsocketAccept, callBack(ww, uri));
    return;

wrongRequest:
    printf("%s: %s:%d: Closing connection: Wrong websocket protocol switch request.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    baioClose(bb);
}

void wsaioHttpStartNewAnswer(struct wsaio *ww) {
    baioMsgStartNewMessage(&ww->b);
    baioMsgReserveSpace(&ww->b, ww->answerHeaderSpaceReserve);
    ww->fixedSizeAnswerHeaderLen = baioPrintfToBuffer(
        &ww->b,
        "Server: %s\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n"
        ,
        ww->serverName
        );
}

static void wsaioHttpFinishAnswerHeaderAndSendBuffer(struct wsaio *ww, char *statusCodeAndDescription, char *contentType, char *additionalHeaders, int chunkedFlag) {
    int         startIndex, contentLen, variableSizeAnswerHeaderLen;
    char        *ss, *dd;

    if ((ww->b.status & BAIO_STATUS_ACTIVE) == 0) return;

    startIndex = baioMsgLastStartIndex(&ww->b);
    ss = ww->b.writeBuffer.b + startIndex;
    contentLen = ww->b.writeBuffer.j - startIndex - ww->answerHeaderSpaceReserve - ww->fixedSizeAnswerHeaderLen;
    assert(contentLen >= 0);

    variableSizeAnswerHeaderLen = sprintf(
        ss,
        "HTTP/1.1 %s\r\n"
        "Content-type: %s\r\n"
        "%s"
        ,
        statusCodeAndDescription,
        contentType,
        (additionalHeaders==NULL?"":additionalHeaders)
        );

    if (chunkedFlag) {
        variableSizeAnswerHeaderLen += sprintf(ss + variableSizeAnswerHeaderLen, "Transfer-Encoding: chunked\r\n");
    } else {
        variableSizeAnswerHeaderLen += sprintf(ss + variableSizeAnswerHeaderLen, "Content-Length: %d\r\n", contentLen);
    }

    assert(variableSizeAnswerHeaderLen < ww->answerHeaderSpaceReserve);
    // move it to the right place
    dd = ss + ww->answerHeaderSpaceReserve - variableSizeAnswerHeaderLen;
    memmove(dd, ss, variableSizeAnswerHeaderLen);

    baioMsgResetStartIndexForNewMsgSize(&ww->b, variableSizeAnswerHeaderLen+ww->fixedSizeAnswerHeaderLen+contentLen);
    baioMsgSend(&ww->b);
}

void wsaioHttpFinishAnswer(struct wsaio *ww, char *statusCodeAndDescription, char *contentType, char *additionalHeaders) {
    wsaioHttpFinishAnswerHeaderAndSendBuffer(ww, statusCodeAndDescription, contentType, additionalHeaders, 0);
    // only when request was answered, we can start to process next request
    ww->b.readBuffer.i += ww->requestSize;
    ww->state = WSAIO_STATE_WAITING_FOR_WWW_REQUEST;
}

void wsaioHttpStartChunkInChunkedAnswer(struct wsaio *ww) {
    int     r;

    baioMsgStartNewMessage(&ww->b);
#if EDEBUG
    r = baioPrintfToBuffer(&ww->b, "1234567890");
#else
    r = baioMsgReserveSpace(&ww->b, WSAIO_CHUNKED_ANSWER_LENGTH_SPACE_RESERVE);
#endif
    JsVarInternalCheck(r == WSAIO_CHUNKED_ANSWER_LENGTH_SPACE_RESERVE);
}

void wsaioHttpFinishAnswerHeaderAndStartChunkedAnswer(struct wsaio *ww, char *statusCodeAndDescription, char *contentType, char *additionalHeaders) {
    wsaioHttpFinishAnswerHeaderAndSendBuffer(ww, statusCodeAndDescription, contentType, additionalHeaders, 1);
    wsaioHttpStartChunkInChunkedAnswer(ww);
}

void wsaioHttpFinalizeAndSendCurrentChunk(struct wsaio *ww, int finalChunkFlag) {
    int         chunkSize;
    int         startIndex, n, r;
    char        *ss, *dd;

    if ((ww->b.status & BAIO_STATUS_ACTIVE) == 0) return;

    startIndex = baioMsgLastStartIndex(&ww->b);
    chunkSize = ww->b.writeBuffer.j - startIndex - WSAIO_CHUNKED_ANSWER_LENGTH_SPACE_RESERVE;
    assert(chunkSize >= 0);

    // printf("chunkSize == %d && finalChunkFlag == %d\n",chunkSize, finalChunkFlag);

    if (chunkSize == 0 && finalChunkFlag == 0) return;

    // chunk terminates by CRLF
    r = baioWriteToBuffer(&ww->b, "\r\n", 2);
    // TODO: It may happen that there is not enough of space in write buffer for the CRLF
    JsVarInternalCheck(r == 2);

    // get start index one more time, because previous write could wrap the buffer
    startIndex = baioMsgLastStartIndex(&ww->b);
    ss = ww->b.writeBuffer.b + startIndex;

    // TODO: Simply print chunk size in reversed order up to (ss + WSAIO_CHUNKED_ANSWER_LENGTH_SPACE_RESERVE) 
    n = sprintf(ss, "%x\r\n", chunkSize);
    dd = ss + WSAIO_CHUNKED_ANSWER_LENGTH_SPACE_RESERVE - n;
    memmove(dd, ss, n);

    baioMsgResetStartIndexForNewMsgSize(&ww->b, chunkSize+n+2);
    baioMsgSend(&ww->b);

    if (chunkSize != 0) {
        if (finalChunkFlag) {
            r = baioPrintfMsg(&ww->b, "0\r\n\r\n");
            JsVarInternalCheck(r == 5);         
        } else {
            wsaioHttpStartChunkInChunkedAnswer(ww);
        }
    }
    if (finalChunkFlag) {
        // final one, we can start to process next request
        ww->b.readBuffer.i += ww->requestSize;  
        ww->state = WSAIO_STATE_WAITING_FOR_WWW_REQUEST;
    }
}

char *wsaioGetFileMimeType(char *fname) {
    char        *suffix;
    char        *mimetype;
    char        sss[JSVAR_TMP_STRING_SIZE];
    int         i;

    mimetype = "text/html";
    if (1 || fname != NULL) {
        suffix = strrchr(fname, '.');
        if (suffix != NULL) {
            // convert to lower cases
            for(i=0; suffix[i] != 0 && i<JSVAR_TMP_STRING_SIZE-1; i++) sss[i] = tolower(suffix[i]);
            sss[i] = 0;
            if (strcmp(sss, ".js") == 0) mimetype = "text/javascript";
            else if (strcmp(sss, ".css") == 0) mimetype = "text/css";
            else if (strcmp(sss, ".json") == 0) mimetype = "application/json";
            else if (strcmp(sss, ".pdf") == 0) mimetype = "application/pdf";
            else if (strcmp(sss, ".gif") == 0) mimetype = "image/gif";
            else if (strcmp(sss, ".jpeg") == 0) mimetype = "image/jpeg";
            else if (strcmp(sss, ".jpg") == 0) mimetype = "image/jpeg";
            else if (strcmp(sss, ".png") == 0) mimetype = "image/png";
            else if (strcmp(sss, ".svg") == 0) mimetype = "image/svg+xml";
            else if (strcmp(sss, ".tif") == 0) mimetype = "image/tiff";
            else if (strcmp(sss, ".tiff") == 0) mimetype = "image/tiff";
            else if (strcmp(sss, ".webp") == 0) mimetype = "image/webp";
        }
    }
    return(mimetype);
}

int wsaioHttpSendFile(struct wsaio *ww, char *fname) {
    FILE    *ff;
    char    b[WSAIO_SEND_FILE_BUFFER_SIZE];
    int     n, res;

    ff = fopen(fname, "r");
    if (ff == NULL) {
        wsaioHttpFinishAnswer(ww, "404 Not Found", "text/html", NULL);
        return(-1);
    }
    res = 0;
    while ((n = fread(b, 1, WSAIO_SEND_FILE_BUFFER_SIZE, ff)) > 0) res += baioWriteToBuffer(&ww->b, b, n);
    fclose(ff);
    wsaioHttpFinishAnswer(ww, "200 OK", wsaioGetFileMimeType(fname), NULL);
    return(res);
}

//////////
// assynchronous  file send

static int wsaioHttpSendFileAsyncCallBackOnError(struct baio *b) ;
static int wsaioHttpSendFileAsyncCallBackOnReadWrite(struct baio *b, int fromj, int n) ;

static void wsaioHttpSendFileAsyncRemoveCallbacks(struct baio *dest) {
    if (dest == NULL) return;
    jsVarCallBackRemoveFromHook(&dest->callBackOnWrite, (void *) wsaioHttpSendFileAsyncCallBackOnReadWrite);
    jsVarCallBackRemoveFromHook(&dest->callBackOnError, (void *) wsaioHttpSendFileAsyncCallBackOnError);
}

static int wsaioHttpSendFileAsyncCallBackOnError(struct baio *b) {
    struct baio *src, *dest;
    // printf("CALLBACK ERROR\n"); fflush(stdout);
    src = baioFromMagic(b->u[0].i);
    dest = baioFromMagic(b->u[1].i);

    if (src != NULL && src->fd >= 0) baioCloseFd(src);
    wsaioHttpSendFileAsyncRemoveCallbacks(dest);

    baioCloseMagic(b->u[0].i);
    baioCloseMagic(b->u[1].i);
    return(0);
}

static int wsaioHttpSendFileAsyncCallBackOnReadWrite(struct baio *b, int fromj, int num) {
    struct baio *src, *dest;
    int         n, srcsize, destsize, csize;

    // printf("%s: CALLBACK COPY: fd == %d\n", JSVAR_PRINT_PREFIX(), b->fd); fflush(stdout);

    src = baioFromMagic(b->u[0].i);
    dest = baioFromMagic(b->u[1].i);

    if (dest == NULL) return(wsaioHttpSendFileAsyncCallBackOnError(b));
    // if src == NULL, we are just pumping rest of write buffer, do nothing
    if (src == NULL) return(0);
    srcsize = src->readBuffer.j - src->readBuffer.i;
    destsize = baioPossibleSpaceForWrite(dest)-WSAIO_CHUNKED_ANSWER_LENGTH_SPACE_RESERVE-8;
    csize = JSVAR_MIN(srcsize, destsize);

    // printf("srcsize, destsize, csize == %d, %d, %d\n", srcsize, destsize, csize);

    if (csize > 0) {
        // copy csize bytes from src to dest
        n = baioWriteToBuffer(dest, src->readBuffer.b+src->readBuffer.i, csize);
        JsVarInternalCheck(n > 0);
        if (n > 0) {
            src->readBuffer.i += n;
            wsaioHttpFinalizeAndSendCurrentChunk((struct wsaio*)dest, 0);
        }
    }
    // test also destsize as we need to start the next chunk of a chunked answer
    if ((src->status & BAIO_STATUS_EOF_READ) && src->readBuffer.i == src->readBuffer.j) {
        // printf("%s: FINAL: fd == %d\n\n", JSVAR_PRINT_PREFIX(), b->fd);
        // Hmm. Why I am closing it twice here ?
        if (src->fd >= 0) {
            baioCloseFd(src);
            src->fd = -1;
        }
        baioCloseMagic(b->u[0].i);
        wsaioHttpSendFileAsyncRemoveCallbacks(dest);
        wsaioHttpFinalizeAndSendCurrentChunk((struct wsaio*)dest, 1);
    }
    return(0);
}

static int wsaioHttpForwardFromBaio(struct wsaio *ww, struct baio *bb, char *mimeType, char *additionalHeaders) {
    int                 cb1, cb2;

    baioReadBufferResize(&bb->readBuffer, WSAIO_SEND_FILE_BUFFER_SIZE, WSAIO_SEND_FILE_BUFFER_SIZE);

    jsVarCallBackAddToHook(&bb->callBackOnRead, (void *) wsaioHttpSendFileAsyncCallBackOnReadWrite);
    jsVarCallBackAddToHook(&bb->callBackOnEof, (void *) wsaioHttpSendFileAsyncCallBackOnReadWrite);
    jsVarCallBackAddToHook(&bb->callBackOnError, (void *) wsaioHttpSendFileAsyncCallBackOnError);
    cb1 = jsVarCallBackAddToHook(&ww->b.callBackOnWrite, (void *) wsaioHttpSendFileAsyncCallBackOnReadWrite);
    cb2 = jsVarCallBackAddToHook(&ww->b.callBackOnError, (void *) wsaioHttpSendFileAsyncCallBackOnError);
    bb->u[0].i = ww->b.u[0].i = bb->baioMagic;
    bb->u[1].i = ww->b.u[1].i = ww->b.baioMagic;

    wsaioHttpFinishAnswerHeaderAndStartChunkedAnswer(ww, "200 OK", mimeType, additionalHeaders);

    return(0);
}

int wsaioHttpForwardFd(struct wsaio *ww, int fd, char *mimeType, char *additionalHeaders) {
    struct baio         *bb;
    int                 r;

    bb = baioNewBasic(BAIO_TYPE_FD, BAIO_IO_DIRECTION_READ, 0);
    if (bb == NULL) return(-1);
    bb->fd = fd;
    r = wsaioHttpForwardFromBaio(ww, bb, mimeType, additionalHeaders);
    return(r);
}

int wsaioHttpForwardString(struct wsaio *ww, char *str, int strlen, char *mimeType) {
    struct baio         *bb;
    int                 cb1, cb2;

    bb = baioNewPseudoFile(str, strlen, 0);
    if (bb == NULL) return(-1);

    cb1 = jsVarCallBackAddToHook(&ww->b.callBackOnWrite, (void *) wsaioHttpSendFileAsyncCallBackOnReadWrite);
    cb2 = jsVarCallBackAddToHook(&ww->b.callBackOnError, (void *) wsaioHttpSendFileAsyncCallBackOnError);

    bb->u[0].i = ww->b.u[0].i = bb->baioMagic;
    bb->u[1].i = ww->b.u[1].i = ww->b.baioMagic;

    wsaioHttpFinishAnswerHeaderAndStartChunkedAnswer(ww, "200 OK", mimeType, NULL);
    return(0);
}

int wsaioHttpSendFileAsync(struct wsaio *ww, char *fname, char *additionalHeaders) {
    int             r;
    struct baio     *bb;

    bb = baioNewFile(fname, BAIO_IO_DIRECTION_READ, 0);
    // bb = baioNewPipedFile(fname, BAIO_IO_DIRECTION_READ, 0);
    // bb = baioNewSocketFile(fname, BAIO_IO_DIRECTION_READ, 0);
    if (bb == NULL) return(-1);
    // printf("%s: SendFileAsync %s: forwarding fd %d --> %d\n", JSVAR_PRINT_PREFIX(), fname, bb->fd, ww->b.fd);
    r = wsaioHttpForwardFromBaio(ww, bb, wsaioGetFileMimeType(fname), additionalHeaders);

    return(r);
}

static int wsaioStrListCmp(struct wsaioStrList *e1, struct wsaioStrList *e2) {
	return(strcmp(e1->s, e2->s));
}

int wsaioHttpSendDirectory(struct wsaio *ww, char *path, char *dirname, char *additionalHeaders) {
    int             	r;
    struct baio     	*bb;
	DIR 				*d;
	struct dirent 		*dir;
	struct stat			st;
	char				ppp[JSVAR_TMP_STRING_SIZE];
	struct wsaioStrList *ll, *ee;
	
	bb = &ww->b;

	baioPrintfToBuffer(
		bb,
		"<html>\n<head><title>Index of %s</title><style>td{padding:3px}</style></head>\n"
		"<body>\n<h1>Index of %s</h1>\n<table>\n<tr><th>Name</th><th>Last modified</th><th>Size</th></tr>\n",
		dirname, dirname
		);

	ll = NULL;
	d = opendir(path);
	if (d != NULL) {
		while ((dir = readdir(d)) != NULL) {
			JSVAR_ALLOC(ee, struct wsaioStrList);
			ee->s = strDuplicate(dir->d_name);
			ee->next = ll;
			ll = ee;
		}
		closedir(d);
		SGLIB_LIST_SORT(struct wsaioStrList, ll, wsaioStrListCmp, next);
		while(ll != NULL) {
			snprintf(ppp, JSVAR_TMP_STRING_SIZE, "%s%s", path, ll->s);
			ppp[JSVAR_TMP_STRING_SIZE-1] = 0;
			if (stat(ppp, &st) == 0) {
				baioPrintfToBuffer(	
					bb,
					"<tr><td><a href='%s%s'>%s%c</a></td><td align='right'>%.20s</td><td align='right'>%d</td></tr>\n",
					dirname, ll->s, ll->s, (S_ISDIR(st.st_mode)?'/':' '), ctime(&st.st_mtim.tv_sec), st.st_size
					);
			}
			ee = ll->next;
			JSVAR_FREE(ll->s);
			JSVAR_FREE(ll);
			ll = ee;
		}
	}

	baioPrintfToBuffer(bb, "<tr><th colspan=3><hr></th></tr>\n</table>\n<address>JsVar Server</address></body>\n</html>\n");
	wsaioHttpFinishAnswer(ww, "200 OK", "text/html", additionalHeaders);
	
    return(r);
}

////////////////////////////////////////////////////

void wsaioWebsocketCompleteFrame(struct wsaio *ww, int opcode) {
    char                    *hh;
    int                     msgIndex, msgSize;
    int                     headbyte;
    long long               payloadlen;
    struct baio             *bb;
    struct baioWriteBuffer  *b;

    bb = &ww->b;
    if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return;
    
    b = &bb->writeBuffer;
    msgIndex = baioMsgLastStartIndex(bb);
    assert(msgIndex >= 0);
    hh = b->b + msgIndex;
    payloadlen = b->j - msgIndex - 10;

    // do not send empty messages
    if (payloadlen <= 0) return;

    // TODO: Maybe implement fragmented messages
    headbyte = (opcode | WSAIO_WEBSOCKET_FIN_MSK);

    // printf("msgIndex, b->j == %d, %d, payloadlen == %lld\n", msgIndex, b->j, payloadlen); fflush(stdout);
    if (payloadlen < 126) {
        msgSize = payloadlen+2;
        hh += 8;
        *(hh++) = headbyte;
        *(hh++) = 0x00 | payloadlen;            
    } else if (payloadlen < (1<<16)) {
        msgSize = payloadlen+4;
        hh += 6;
        *(hh++) = headbyte;
        *(hh++) = 0x00 | 126;
        *(hh++) = (payloadlen >> 8) & 0xff;
        *(hh++) = (payloadlen) & 0xff;
    } else {
        msgSize = payloadlen+10;
        *(hh++) = headbyte;
        *(hh++) = 0x00 | 127;
        *(hh++) = (payloadlen >> 56) & 0xff;
        *(hh++) = (payloadlen >> 48) & 0xff;
        *(hh++) = (payloadlen >> 40) & 0xff;
        *(hh++) = (payloadlen >> 32) & 0xff;
        *(hh++) = (payloadlen >> 24) & 0xff;
        *(hh++) = (payloadlen >> 16) & 0xff;
        *(hh++) = (payloadlen >> 8) & 0xff;
        *(hh++) = (payloadlen) & 0xff;
    }
    baioMsgResetStartIndexForNewMsgSize(bb, msgSize);
    baioMsgSend(bb);
    // baioLastMsgDump(&ww->b);
    wsaioWebsocketStartNewMessage(ww);
}

void wsaioWebsocketCompleteMessage(struct wsaio *ww) {
    wsaioWebsocketCompleteFrame(ww, WSAIO_WEBSOCKET_OP_CODE_TEXT);
}

void wsaioWebsocketCompleteDataMessage(struct wsaio *ww) {
    wsaioWebsocketCompleteFrame(ww, WSAIO_WEBSOCKET_OP_CODE_BIN);
}

int wsaioWebsocketVprintf(struct wsaio *ww, char *fmt, va_list arg_ptr) {
    int res;
    res = baioVprintfToBuffer(&ww->b, fmt, arg_ptr);
    wsaioWebsocketCompleteMessage(ww);
    return(res);
}

int wsaioWebsocketPrintf(struct wsaio *ww, char *fmt, ...) {
    int             res;
    va_list         arg_ptr;

    va_start(arg_ptr, fmt);
    res = wsaioWebsocketVprintf(ww, fmt, arg_ptr);
    va_end(arg_ptr);
    return(res);
}

//////////
enum wsaioRequestTypeEnum {
    WSAIO_RQT_NONE,
    WSAIO_RQT_GET,
    WSAIO_RQT_POST,
    WSAIO_RQT_MAX,
};


static void wsaioOnWwwRequest(struct wsaio *ww, struct baio *bb, int requestHeaderLen, int contentLength, char *uri, int requestType) {
    int     r;
    int     maximalAcceptableContentLength;

    ww->currentRequest.postQuery = bb->readBuffer.b + bb->readBuffer.i + requestHeaderLen;
    ww->requestSize = requestHeaderLen + contentLength;
    ww->state = WSAIO_STATE_WAITING_FOR_WWW_REQUEST;
    wsaioHttpStartNewAnswer(ww);

    r = 0;
    if (requestType == WSAIO_RQT_GET) {
        JSVAR_CALLBACK_CALL(ww->callBackOnWwwGetRequest, (r = callBack(ww, uri)));
    } else if (requestType == WSAIO_RQT_POST) {
        JSVAR_CALLBACK_CALL(ww->callBackOnWwwPostRequest, (r = callBack(ww, uri)));
    } else {
        printf("%s: %s:%d: Web: Wrong requestType %d.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, requestType);
        goto wrongRequest;  
    }

    // finalize answer
    return;

wrongRequest:
    printf("%s: %s:%d: Closing connection.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
    baioClose(bb);
}

static void wsaioOnWwwRead(struct wsaio *ww) {
    struct baio     *bb;
    int             requestHeaderLen, contentLength, maximalAcceptableContentLength;
    char            *s, *hend, *hstart, *method, *httpVersion;
    char            *fieldName, *fieldValue;
    char            *uristr, *query;
    int             i, uriLen, fieldNameLen, fieldValueLen;

    bb = &ww->b;

    assert(bb->readBuffer.j < bb->readBuffer.size);
    bb->readBuffer.b[bb->readBuffer.j] = 0;

    hstart = bb->readBuffer.b + bb->readBuffer.i;
    hend = wsaioStrFindTwoNewlines(hstart);

    // printf("READ %.*s\n",  bb->readBuffer.j - bb->readBuffer.i, hstart); fflush(stdout);

    // if I do not have whole header, wait until I have
    if (hend == NULL) return;

    if (ww->state != WSAIO_STATE_WAITING_FOR_WWW_REQUEST) {
        assert(0 && "Delayed answer to requests are not yet implemented");
    }
    // printf("GOT HEADER\n"); fflush(stdout);

    // parsing of HTTP header 
    // clean previous http header values
    for(i=0; i<WSAIO_HTTP_HEADER_MAX; i++) {
        wsaioHttpHeader[i] = NULL;
        wsaioHttpHeaderLen[i] = 0;
    }
    // skip any initial newlines (I allow lines terminated by single \n instead of \r\n)
    s = hstart;
    while (*s == '\r' || *s == '\n') s ++;
    method = s;
    while (s < hend && *s != ' ' && *s != '\n') s++;
    if (*s != ' ') goto wrongRequest;
    s++;
    uristr = s;
    while (s < hend && *s != ' '&& *s != '\n') s++;
    if (*s != ' ') goto wrongRequest;
    uriLen = s - uristr;
    s++;
    httpVersion = s;
    while (s < hend && *s != '\n') s++;
    s++;
    // parse HTTP header fields
    while (! ((s[0] == '\r' && s[1] == '\n') || (s[0] == '\n'))) {
        // skip blanks
        while (*s == '\r' || *s == '\n' || *s == ' ' || *s == '\t') s++;
        fieldName = s;
        while (*s != ':' && *s != '\n') s++;
        if (*s == ':') {
            fieldNameLen = s - fieldName;
            while (fieldNameLen>0 && isblank(fieldName[fieldNameLen-1])) fieldNameLen--;
            s ++;
            // skip blank, maybe I shall relax from the (obsolete part of) standard allowing multiline header values?
            while (s[0] == ' ' 
                   || s[0] == '\t' 
                   || (s[0] == '\r' && s[1] == '\n' && (s[2] == ' ' || s[2] == '\t'))
                   || (s[0] == '\n' && (s[1] == ' ' || s[1] == '\t'))
                ) {
                s ++;
            }
            fieldValue = s;
            while (! ((s[0] == '\r' && s[1] == '\n' && s[2] != ' ' && s[2] != '\t') || (s[0] == '\n' && s[1] != ' ' && s[1] != '\t'))) s ++;
            fieldValueLen = s - fieldValue;
            while (fieldValueLen>0 && isblank(fieldValue[fieldValueLen-1])) fieldValueLen--;
            s += 2;
            wsaioAddHttpHeaderField(fieldName, fieldNameLen, fieldValue, fieldValueLen);
        }
    }
    while (*s != '\n') s++;
    s++;
    requestHeaderLen = s - hstart;

    // for the moment no chunked connection, content length required
    contentLength = 0;
    if (wsaioHttpHeader[WSAIO_HTTP_HEADER_Content_Length] != NULL) {
        contentLength = atoi(wsaioHttpHeader[WSAIO_HTTP_HEADER_Content_Length]);
        maximalAcceptableContentLength = bb->maxReadBufferSize - requestHeaderLen - bb->minFreeSizeAtEndOfReadBuffer - bb->minFreeSizeBeforeRead-1;
        // printf("MAX %d\n", maximalAcceptableContentLength);
        if (contentLength < 0 || contentLength > maximalAcceptableContentLength) {
            printf("%s: %s:%d: Web: Wrong or too large content length %d.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, contentLength);
            goto wrongRequest;
        }
    }

    // if we do not have whole request, do nothing
    if (bb->readBuffer.i + requestHeaderLen + contentLength > bb->readBuffer.j) return;

    uristr[uriLen] = 0;
    query = strchr(uristr, '?');
    if (query == NULL) {
        query = "";
    } else {
        *query = 0;
        query ++;
    }
    ww->currentRequest.getQuery = query;
    ww->currentRequest.postQuery = "";      // will be set later
    wsaioUriDecode(uristr);

    // only GET and POST method is accepted for the moment
    if (strncmp(method, "GET", 3) == 0) {
        if (wsaioHttpHeaderContains(WSAIO_HTTP_HEADER_Upgrade, "websocket") 
            && wsaioHttpHeaderContains(WSAIO_HTTP_HEADER_Connection, "Upgrade")) {
            wsaioOnWwwToWebsocketProtocolSwitchRequest(ww, bb, requestHeaderLen, contentLength, uristr);
        } else {
            wsaioOnWwwRequest(ww, bb, requestHeaderLen, contentLength, uristr, WSAIO_RQT_GET);
        }
    } else if (strncmp(method, "POST", 4) == 0) {
        wsaioOnWwwRequest(ww, bb, requestHeaderLen, contentLength, uristr, WSAIO_RQT_POST);
    } else {
        goto wrongRequest;
    }

    return;

wrongRequest:
    printf("%s: %s:%d: Closing connection: Wrong request: %.*s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, (int)(hend-hstart), hstart);
    baioClose(bb);
}

static int wsaioCompactCompletedFragmentedWebsocketMessage(struct wsaio *ww, int *opcodeOut, unsigned char **payloadOut, long long *payloadLenOut) {
    struct baio         *bb;
    unsigned char       *s, *msgStart, *payload;
    int                 fin, masked, opcode, firstFrameFlag;
    unsigned            b;
    long long           payloadLen, payloadLenTotal;

    bb = &ww->b;
    firstFrameFlag = 1;
    fin = 0;
    s = (unsigned char *)bb->readBuffer.b + bb->readBuffer.i;
    while(fin == 0) {
        b = *s++;
        fin = b & WSAIO_WEBSOCKET_FIN_MSK;
        opcode = b & WSAIO_WEBSOCKET_OP_CODE_MSK;

        b = *s++;
        masked = b & WSAIO_WEBSOCKET_MASK_MSK;
        payloadLen = b & WSAIO_WEBSOCKET_PLEN_MSK;
        if (payloadLen == 126) {
            payloadLen = (s[0] << 8) + s[1];
            s += 2;
        } else if (payloadLen == 127) {
            payloadLen = (((((((((((((((long long)s[0]<<8)+s[1])<<8)+s[2])<<8)+s[3])<<8)+s[4])<<8)+s[5])<<8)+s[6])<<8)+s[7]);
            s += 8;
        }

        if (masked) s += 4;

        if (firstFrameFlag) {
            if (opcode !=  WSAIO_WEBSOCKET_OP_CODE_TEXT) return(-1);
            if (opcodeOut != NULL) *opcodeOut = opcode;
            payload = s;
            payloadLenTotal = payloadLen;
        } else {
            if (opcode ==  WSAIO_WEBSOCKET_OP_CODE_CONTINUATION) {
                memmove(payload+payloadLenTotal, s, payloadLen);
                payloadLenTotal += payloadLen;
            }
        }
        s += payloadLen;
        firstFrameFlag = 0;
    };
    if (payloadOut != NULL) *payloadOut = payload;
    if (payloadLenOut != NULL) *payloadLenOut = payloadLenTotal;
    return(0);
}

static void wsaioOnWsRead(struct wsaio *ww) {
    struct baio         *bb;
    unsigned            b;
    long long           payloadLen;
    unsigned char       *mask;
    int                 availableSize;
    int                 maximalPayloadLen;
    int                 r, masked, opcode, fin, i;
    unsigned char       *s, *msgStart, *payload;

    bb = &ww->b;
    // printf("WSREAD %.*s\n",  bb->readBuffer.j - bb->readBuffer.i, bb->readBuffer.b + bb->readBuffer.i); fflush(stdout);
    for(;;) {
    continueWithNextFrame:
        // we need whole message in our buffer, so there is a maximum for payload length
        maximalPayloadLen = bb->maxReadBufferSize - bb->minFreeSizeAtEndOfReadBuffer - bb->minFreeSizeBeforeRead - ww->previouslySeenWebsocketFragmentsOffset - 11;
        // parse websockets frames
        // we need complete frame header
        availableSize = bb->readBuffer.j - bb->readBuffer.i - ww->previouslySeenWebsocketFragmentsOffset;
        if (availableSize < 2) return;

        s = msgStart = (unsigned char *)bb->readBuffer.b + bb->readBuffer.i + ww->previouslySeenWebsocketFragmentsOffset;
        b = *s++;
        fin = b & WSAIO_WEBSOCKET_FIN_MSK;
        opcode = b & WSAIO_WEBSOCKET_OP_CODE_MSK;

        b = *s++;
        masked = b & WSAIO_WEBSOCKET_MASK_MSK;
        payloadLen = b & WSAIO_WEBSOCKET_PLEN_MSK;
        if (payloadLen == 126) {
            if (availableSize < 4) return;
            payloadLen = (s[0] << 8) + s[1];
            s += 2;
        } else if (payloadLen == 127) {
            if (availableSize < 10) return;
            payloadLen = (((((((((((((((long long)s[0]<<8)+s[1])<<8)+s[2])<<8)+s[3])<<8)+s[4])<<8)+s[5])<<8)+s[6])<<8)+s[7]);
            s += 8;
        }
        if (payloadLen >= maximalPayloadLen) {
            printf("%s: %s:%d: Websockets message too long (try to increase maxReadBufferSize). Closing connection\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            // This may mean we are lost in websockets frames, close the connection
            baioCloseOnError(bb);
            return;
        }

        mask = NULL;
        if (masked) {
            // According to the standard, client shall always mask its data!
            mask = s;
            s += 4;
        }
        // we are waiting for the whole frame before demasking it
        if (availableSize < s - msgStart + payloadLen) return;

        payload = s;
        if (masked) {
            // demask payload
            for(i=0; i<payloadLen; i++) payload[i] ^= mask[i%4];
        }
        s += payloadLen;

        // printf("%s: %s:%d: Info: opcode fin == %d, %d.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, opcode, fin);
        if (fin == 0) {
            // printf("%s: %s:%d: Info: received a fragmented websocket msg.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            ww->previouslySeenWebsocketFragmentsOffset += s - msgStart;
            goto continueWithNextFrame;
        } else if (ww->previouslySeenWebsocketFragmentsOffset != 0) {
            // this may be a control frame within fragmented message or a final frame of a fragmented message
            ww->previouslySeenWebsocketFragmentsOffset += s - msgStart;
            if (opcode == WSAIO_WEBSOCKET_OP_CODE_CONTINUATION) {
                // printf("%s: %s:%d: Info: received last frame of a fragmented websocket msg.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
                wsaioCompactCompletedFragmentedWebsocketMessage(ww, &opcode, &payload, &payloadLen);
            }
        }

        if (opcode ==  WSAIO_WEBSOCKET_OP_CODE_TEXT) {
            r = 0;
            // JSVAR_CALLBACK_CALL(ww->callBackOnWebsocketGetMessage, (r = callBack(ww, bb->readBuffer.i+(payload-msgStart), payloadLen)));
            JSVAR_CALLBACK_CALL(ww->callBackOnWebsocketGetMessage, (r = callBack(ww, (char*)payload - bb->readBuffer.b, payloadLen)));
        } else if (opcode == WSAIO_WEBSOCKET_OP_CODE_CLOSED) {
            if (jsVarDebugLevel > 0) printf("%s: Websocket connection closed by remote host.\n", JSVAR_PRINT_PREFIX());
            baioClose(bb);
            return;
        } else if (opcode == WSAIO_WEBSOCKET_OP_CODE_PING) {
            // ping, answer with pong frame with the same payload as ping
            // printf("%s: %s:%d: Warning: a websocket ping received. Not yet implemented\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            baioWriteToBuffer(&ww->b, (char*)payload, payloadLen);
            wsaioWebsocketCompleteFrame(ww, WSAIO_WEBSOCKET_OP_CODE_PONG);
            // a ping request within fragmented message
            if (ww->previouslySeenWebsocketFragmentsOffset != 0) goto continueWithNextFrame;
        } else if (opcode == WSAIO_WEBSOCKET_OP_CODE_PONG) {
            // pong, ignore
            printf("%s: %s:%d: Warning: a websocket pong received. Not yet implemented\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
            // a pong within fragmented message
            if (ww->previouslySeenWebsocketFragmentsOffset != 0) goto continueWithNextFrame;
        } else {
            printf("%s: %s:%d: Warning a non textual frame with opcode == %d received. Not yet implemented\n", 
                   JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, opcode);
            // This may mean we are lost in websockets frames, close the connection
            baioCloseOnError(bb);
            return;
        }

        // the whole message is processed whether it was fragmented or not
        bb->readBuffer.i = ((char*)s - bb->readBuffer.b);
        ww->previouslySeenWebsocketFragmentsOffset = 0;
    }
}

///////////////////////////////////////////////////////////////

static int wsaioOnBaioRead(struct baio *bb, int fromj, int n) {
    struct wsaio *ww;

    ww = (struct wsaio *) bb;

    // We do not process new read requests unless having some space available in write buffer (to write headers, etc).
    if (baioPossibleSpaceForWrite(bb) < ww->minSpaceInWriteBufferToProcessRequest) {
        if (bb->maxWriteBufferSize < ww->minSpaceInWriteBufferToProcessRequest * 2) {
            static uint8_t warningIssued = 0;
            if (warningIssued == 0) {
                printf("%s: %s:%d: Warning: maxWriteBufferSize is less than 2*minSpaceInWriteBufferToProcessReques. The web server may have trouble answering requests.\n",
                       JSVAR_PRINT_PREFIX(), __FILE__, __LINE__
                    );
                warningIssued = 1;
            }
        }
        return(0);
    }

    switch (ww->state) {
    case WSAIO_STATE_WAITING_FOR_WWW_REQUEST:
        wsaioOnWwwRead(ww);
        break;
    case WSAIO_STATE_WEBSOCKET_ACTIVE:
        wsaioOnWsRead(ww);
        break;
    default:
        printf("%s: %s:%d: Internal error. Unknown wsaio state %d\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, ww->state);
    }
    return(0);
}

static int wsaioOnBaioWrite(struct baio *bb, int fromi, int n) {
    struct wsaio *ww;
    ww = (struct wsaio *) bb;
    // Hmm. check that it is actually websocket send
    // if (ww->callBackOnWebsocketWrite != NULL) return(ww->callBackOnWebsocketWrite(ww, fromi, n));
    // if there is unprocessed read, maybe it was waiting for some space for write, we shall process it
    if (bb->readBuffer.i != bb->readBuffer.j) wsaioOnBaioRead(bb, bb->readBuffer.j, 0);
    return(0);
}

static int wsaioOnBaioDelete(struct baio *bb) {
    struct wsaio *ww;

    // printf("wsaioOnBaioDelete: bb==%p, bb->writeBuffer.b == %p\n", bb, bb->writeBuffer.b); fflush(stdout);

    ww = (struct wsaio *) bb;
    JSVAR_CALLBACK_CALL(ww->callBackOnDelete, callBack(ww));

    // free my hooks
    jsVarCallBackFreeHook(&ww->callBackAcceptFilter);
    jsVarCallBackFreeHook(&ww->callBackOnAccept);
    jsVarCallBackFreeHook(&ww->callBackOnWwwGetRequest);
    jsVarCallBackFreeHook(&ww->callBackOnWwwPostRequest);
    jsVarCallBackFreeHook(&ww->callBackOnWebsocketAccept);
    jsVarCallBackFreeHook(&ww->callBackOnWebsocketGetMessage);
    jsVarCallBackFreeHook(&ww->callBackOnWebsocketSend);
    jsVarCallBackFreeHook(&ww->callBackOnDelete);

    return(0);
}

static int wsaioBaioAcceptFilter(struct baio *bb, unsigned ip, unsigned port) {
    struct wsaio    *ww;
    int             r;

    ww = (struct wsaio *) bb;
    r = 0;
    JSVAR_CALLBACK_CALL(ww->callBackAcceptFilter, (r = callBack(ww, ip, port)));
    return(r);
}

static int wsaioOnBaioAccept(struct baio *bb) {
    struct wsaio *ww;
    ww = (struct wsaio *) bb;

    JSVAR_CALLBACK_CALL(ww->callBackOnAccept, callBack(ww));
    return(0);
}

static int wsaioOnBaioTcpIpAccept(struct baio *bb) {
    struct wsaio *ww;
    ww = (struct wsaio *) bb;

    jsVarCallBackCloneHook(&ww->callBackAcceptFilter, NULL);
    jsVarCallBackCloneHook(&ww->callBackOnTcpIpAccept, NULL);
    jsVarCallBackCloneHook(&ww->callBackOnAccept, NULL);
    jsVarCallBackCloneHook(&ww->callBackOnWwwGetRequest, NULL);
    jsVarCallBackCloneHook(&ww->callBackOnWwwPostRequest, NULL);
    jsVarCallBackCloneHook(&ww->callBackOnWebsocketAccept, NULL);
    jsVarCallBackCloneHook(&ww->callBackOnWebsocketGetMessage, NULL);
    jsVarCallBackCloneHook(&ww->callBackOnWebsocketSend, NULL);
    jsVarCallBackCloneHook(&ww->callBackOnDelete, NULL);

    JSVAR_CALLBACK_CALL(ww->callBackOnTcpIpAccept, callBack(ww));

    return(0);
}

struct wsaio *wsaioNewServer(int port, enum baioSslFlags sslFlag, int additionalSpaceToAllocate) {
    struct wsaio    *ww;
    struct baio     *bb;

    // casting pointer to the first element of struct is granted in ANSI C.
    bb = baioNewTcpipServer(port, sslFlag, sizeof(struct wsaio) - sizeof(struct baio) + additionalSpaceToAllocate);
    if (bb == NULL) return(NULL);

    ww = (struct wsaio *) bb;
    ww->wsaioSecurityCheckCode = 0xcacebaf;
    ww->state = WSAIO_STATE_WAITING_FOR_WWW_REQUEST;
    ww->serverName = "myserver.com";
    ww->answerHeaderSpaceReserve = 256;
    ww->minSpaceInWriteBufferToProcessRequest = ww->answerHeaderSpaceReserve + 512;

    bb->minFreeSizeAtEndOfReadBuffer = 1;
    jsVarCallBackAddToHook(&bb->callBackAcceptFilter, (void *) wsaioBaioAcceptFilter);
    jsVarCallBackAddToHook(&bb->callBackOnTcpIpAccept, (void *) wsaioOnBaioTcpIpAccept);
    jsVarCallBackAddToHook(&bb->callBackOnAccept, (void *) wsaioOnBaioAccept);
    jsVarCallBackAddToHook(&bb->callBackOnRead, (void *) wsaioOnBaioRead);
    jsVarCallBackAddToHook(&bb->callBackOnWrite, (void *) wsaioOnBaioWrite);
    jsVarCallBackAddToHook(&bb->callBackOnDelete, (void *) wsaioOnBaioDelete);
    return(ww);
}

// This is supposed to retrieve a few short values. Would be slow for large environments
char *jsVarGetEnvPtr(char *env, char *key) {
    int mn, n;
    char *s, kk[JSVAR_TMP_STRING_SIZE];
    mn = JSVAR_TMP_STRING_SIZE-5;
    n = strlen(key);
    if (n >= mn) n = mn;
    kk[0] = '&';
    strncpy(kk+1, key, n);
    kk[n+1] = '=';
    kk[n+2] = 0;

    // printf("looking for '%s' in '%s'\n", kk, env);

    if (strncmp(env, kk+1, n+1) == 0) {
        s = env+n+1;
    } else {
        s = strstr(env, kk);
        if (s == NULL) return(NULL);
        s += n+2;
    }
    return(s);
}

char *jsVarGetEnv(char *env, char *key, char *dst, int dstlen) {
    char            *s, *d;
    int             i;

    if (env == NULL || key == NULL || dst == NULL) return(NULL);

    dst[0] = 0;
    s = jsVarGetEnvPtr(env, key);
    if (s==NULL) return(NULL);

    for(d=dst,i=0; *s && *s!='&' && i<dstlen-1; d++,s++,i++) *d = *s;
    *d = 0;

    wsaioUriDecode(dst);
    return(dst);
}

struct jsVarDstr *jsVarGetEnvDstr(char *env, char *key) {
    struct jsVarDstr   *res;
    char               *s, *d;

    if (env == NULL || key == NULL) return(NULL);
    s = jsVarGetEnvPtr(env, key);
    if (s==NULL) return(NULL);
    res = jsVarDstrCreate();
    for(; *s && *s!='&'; s++) {
        jsVarDstrAddCharacter(res, *s);
    }
    jsVarDstrAddCharacter(res, 0);
    d = wsaioUriDecode(res->s);
    jsVarDstrTruncateToSize(res, d - res->s);
    return(res);
}

char *jsVarGetEnv_st(char *env, char *key) {
    char            *res;

    if (env == NULL || key == NULL) return(NULL);
    res = baioStaticRingGetTemporaryStringPtr();
    res = jsVarGetEnv(env, key, res, JSVAR_TMP_STRING_SIZE);
    return(res);
}

int jsVarGetEnvInt(char *env, char *key, int defaultValue) {
    char *v;
    v = jsVarGetEnv_st(env, key);
    if (v == NULL) return(defaultValue);
    return(atoi(v));
}

double jsVarGetEnvDouble(char *env, char *key, double defaultValue) {
    char *v;
    v = jsVarGetEnv_st(env, key);
    if (v == NULL) return(defaultValue);
    return(strtod(v, NULL));
}


















///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// Javasctript part of the communication.
// It shall be included into each HTML page using jsvar

static char *jsVarMainJavascript = JSVAR_STRINGIFY(
    if (typeof jsvar === "undefined") {
        jsvar = {};
    } else if (typeof jsvar !== 'object') {
        throw new Error("JsVar: Variable 'jsvar' is not an object!");
    }
    \n
	//
	// You can customize behavior by overwriting those objects
	//
	// flag whether websocket is connected to the server
	jsvar.connected = false;
	// you set your debuging level by overwriting this value
	jsvar.debuglevel = 0;
	// overwrite it if you need custom action when websocket connects
	jsvar.onConnected = null;
	// overwrite it if you need custom action when binary data received from websocket
	jsvar.onDataReceived = function(msg) { jsvar.data = msg; };
	// overwrite it if you need custom action when eval request received from websocket
	jsvar.onEvalMessageReceived = function(msg) { eval(msg); };
	// overwrite it if you need custom action when websocket disconnects
	jsvar.onDisconnected = null;
	//
	\n
    jsvar.initialize = function() {
        // names defined here do not pollute global namespace as they are local to function
        var oldOnload = window.onload;
        window.onload = function(evt) {
            if (oldOnload) oldOnload(evt);
            // do not start websocket "immediately", use timeout to start when loading of the script is finished
            setTimeout(function(){ 
					if (jsvar.debuglevel > 0) console.log("JsVar:", (new Date()) + ": Start.");
					jsvarOpenWebSocket();
                }, 100);
        }\n;
		// an auxiliary function
        function removeAppendixAfter(s, delimiter) {
            var res = s;
            var i = res.indexOf(delimiter);
            if (i >= 0) res = res.substring(0, i);
            return(res);
        }
		\n
		// here we hold active websocket connection
        jsvar.websocket = null;
		// This is the function allowing callbacks to C code
		jsvar.callback = function(msg) {
			// console.log("sending message", msg, nlen, index, vlen);
			if (jsvar.websocket != null) jsvar.websocket.send(msg);
		}\n;
        function jsvarOpenWebSocket() {
            if (jsvar.websocket != null) jsvar.websocket.close();
            var wsurl = document.URL.replace("http", "ws");
            wsurl = removeAppendixAfter(wsurl, '?');
            wsurl = removeAppendixAfter(wsurl, '#');
            jsvar.websocket = new WebSocket(wsurl, "jsync");
			jsvar.websocket.binaryType = "arraybuffer";
            jsvar.websocket.onopen = function() {
                if (jsvar.debuglevel > 10) console.log("JsVar:", (new Date()) + ": Websocket to "+wsurl+" is open");
                if (jsvar.onConnected != null) jsvar.onConnected();
                jsvar.connected = true;
            };
            jsvar.websocket.onmessage = function (evt) { 
                var data = evt.data;
                if (data instanceof ArrayBuffer || data instanceof Blob) {
                    // binary frame received
                    if (jsvar.debuglevel > 100) console.log("JsVar:", (new Date()) + ": Websocket Got Data");
                    jsvar.onDataReceived(data);
                } else {
					// text frame, i.e. an eval request
                    if (jsvar.debuglevel > 100) console.log("JsVar:", (new Date()) + ": Websocket Got Message: ", data);
                    jsvar.onEvalMessageReceived(data);
                }
            };
            jsvar.websocket.onclose = function() { 
                if (jsvar.debuglevel > 10) console.log("JsVar:", (new Date()) + ": Websocket connection to "+wsurl+" is closed"); 
                jsvar.connected = false;
                if (jsvar.onDisconnected != null) jsvar.onDisconnected();
                jsvar.websocket = null;
                // try to reconnect
                setTimeout(jsvarOpenWebSocket, 5000);
            };			
        }\n;
    }\n;
    \n
    jsvar.initialize();
    \n
    );


static int jsVarCallbackOnWebSocketGet(struct wsaio *ww, int fromj, int n) {
    struct jsVaraio *jj;
    char            *ss, *ss0;
    
    jj = (struct jsVaraio *) ww;
    ss = ss0 = jj->w.b.readBuffer.b + fromj;
    // ss = jsVarParseVariableUpdate(jj, ss, n);
    if (ss == NULL) {
        printf("%s: %s:%d: Error in message: %.*s\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__, n, ss0);    
        return(-1);
    }
    return(1);
}

static int jsVarCallbackOnDelete(struct wsaio *ww) {
    struct jsVaraio *jj;

    jj = (struct jsVaraio *) ww;

    if (jj->w.b.baioType == BAIO_TYPE_TCPIP_SERVER) {
        JSVAR_FREE(jj->singlePageText);
        JSVAR_FREE(jj->fileServerRootDir);
    }
    return(0);
}

struct jsVaraio *jsVarNewServer(int port, enum baioSslFlags sslFlag, int additionalSpaceToAllocate) {
    struct wsaio        *ww;
    struct jsVaraio     *jj;
    
    ww = wsaioNewServer(port, sslFlag, sizeof(struct jsVaraio) - sizeof(struct wsaio) + additionalSpaceToAllocate);
    if (ww == NULL) return(NULL);

    jj = (struct jsVaraio *) ww;

    // we need to be able to write jsvar.js at once. TODO: Copy jsvarJavascriptText by some assynchronous pipe and remove this requirement
    // jj->w.minSpaceInWriteBufferToProcessRequest = strlen(jsvarJavascriptText) + 1024;

    jsVarCallBackAddToHook(&ww->callBackOnDelete, (void *) jsVarCallbackOnDelete);

    return(jj);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

static int jsVarCallbackOnWwwGetRequestSinglePage(struct wsaio *ww, char *uri) {
    struct jsVaraio     *jj;
    int                 r, jslen;
    struct baio         *bb;

    jj = (struct jsVaraio *) ww;
    bb = &ww->b;

	if (strcmp(uri, "/jsvarmainjavascript.js") == 0) {
		baioPrintfToBuffer(bb, "%s", jsVarMainJavascript);
		wsaioHttpFinishAnswer(ww, "200 OK", "text/javascript", NULL);
	} else {
		baioPrintfToBuffer(bb, "%s", jj->singlePageText);
		wsaioHttpFinishAnswer(ww, "200 OK", "text/html", NULL);
    }
    return(1);
}

static int jsVarCallbackOnWwwGetRequestFile(struct wsaio *ww, char *uri) {
    struct jsVaraio     *jj;
    int                 r, jslen, plen;
    struct baio         *bb;
	int                 i,j;
	char                nuri[JSVAR_PATH_MAX];
	char                path[2*JSVAR_PATH_MAX];
	struct stat         st;

    jj = (struct jsVaraio *) ww;
    bb = &ww->b;

	if (strcmp(uri, "/jsvarmainjavascript.js") == 0) {
		baioPrintfToBuffer(bb, "%s", jsVarMainJavascript);
		wsaioHttpFinishAnswer(ww, "200 OK", "text/javascript", NULL);
		return(1);
	}

	ww = &jj->w;
	bb = &ww->b;

	// printf("%s: FileServer: GET %s\n", JSVAR_PRINT_PREFIX(), uri);

	if (uri == NULL) goto fail;
	if (uri[0] != '/') goto fail;

	// resolve the name to avoid toxic names like "/../../secret.txt"
	for(i=0,j=0; i<JSVAR_PATH_MAX && uri[j];) {
		nuri[i] = uri[j];
		if (uri[j]=='/' && uri[j+1] == '.' && (uri[j+2] == '/' || uri[j+2] == 0)) {
			j += 2;
		} else if (uri[j]=='/' && uri[j+1] == '.' && uri[j+2] == '.' && (uri[j+3] == '/' || uri[j+3] == 0)) {
			j += 3;
			if (i <= 0) goto fail;
			for(i--; i>=0 && nuri[i] != '/'; i--) ;
			if (i < 0) goto fail;
			i ++;
		} else {
			nuri[i++] = uri[j++];
		}
	}
	if (i <= 0 || i >= sizeof(nuri) - 1) goto fail;
	nuri[i] = 0;
	
	snprintf(path, sizeof(path), "%s%s", jj->fileServerRootDir, nuri);
	path[sizeof(path)-1] = 0;
	plen = strlen(path);

	if (stat(path, &st) != 0) goto fail;
	
	if (S_ISDIR(st.st_mode) && plen < sizeof(path) - sizeof(JSVAR_INDEX_FILE_NAME) - 1) {
		if (path[plen-1] != '/') path[plen++] = '/';
		if (nuri[i-1] != '/') {
			nuri[i++] = '/';
			nuri[i] = 0;
		}
		strcpy(&path[plen], JSVAR_INDEX_FILE_NAME);
		if (stat(path, &st) == 0) {
			// index.html exists, load it
			wsaioHttpSendFileAsync(ww, path, NULL);
		} else {
			// index.html doesn't exists
			path[plen] = 0;
			if (jj->fileServerListDirectories == 0) goto fail;
			// load directory
			wsaioHttpSendDirectory(ww, path, nuri, NULL);
		}
	} else {
		// not a directory name, load file
		// check for file existance
		if (stat(path, &st) != 0) goto fail;
		// return the file
		wsaioHttpSendFileAsync(ww, path, NULL);
	}
	return(1);

fail:
	wsaioHttpFinishAnswer(ww, "404 Not Found", "text/html", NULL);
	return(-1);
}

int jsVarCallbackOnAcceptSinglePage(struct wsaio *ww) {
	ww->b.userRuntimeType = JSVAR_CON_SINGLE_PAGE_WEBSERVER_CLIENT;
	return(0);
}

int jsVarCallbackOnAcceptFile(struct wsaio *ww) {
	ww->b.userRuntimeType = JSVAR_CON_FILE_WEBSERVER_CLIENT;
	return(0);
}

int jsVarCallbackOnWebsocketAccept(struct wsaio *ww) {
	ww->b.userRuntimeType = JSVAR_CON_WEBSOCKET_SERVER_CLIENT;
	return(0);
}

struct jsVaraio *jsVarNewSinglePageServer(int port, enum baioSslFlags sslFlag, int additionalSpaceToAllocate, char *body) {
    struct jsVaraio *jj;
    jj = jsVarNewServer(port, sslFlag, additionalSpaceToAllocate);
	if (jj == NULL) return(NULL);
	jj->w.b.userRuntimeType = JSVAR_CON_SINGLE_PAGE_WEBSERVER;
    jj->singlePageText = body;
    jsVarCallBackAddToHook(&jj->w.callBackOnWwwGetRequest, (void *) jsVarCallbackOnWwwGetRequestSinglePage);
	jsVarCallBackAddToHook(&jj->w.callBackOnAccept, (void *) jsVarCallbackOnAcceptSinglePage);
	jsVarCallBackAddToHook(&jj->w.callBackOnWebsocketAccept, (void *) jsVarCallbackOnWebsocketAccept);
    return(jj);
}

struct jsVaraio *jsVarNewFileServer(int port, enum baioSslFlags sslFlag, int additionalSpaceToAllocate, char *rootDirectory) {
    struct jsVaraio *jj;
    jj = jsVarNewServer(port, sslFlag, 0);
	if (jj == NULL) return(NULL);
	jj->w.b.userRuntimeType = JSVAR_CON_FILE_WEBSERVER;
    jj->fileServerRootDir = rootDirectory;
	jj->fileServerListDirectories = 1;
    jsVarCallBackAddToHook(&jj->w.callBackOnWwwGetRequest, (void *) jsVarCallbackOnWwwGetRequestFile);
	jsVarCallBackAddToHook(&jj->w.callBackOnAccept, (void *) jsVarCallbackOnAcceptFile);
	jsVarCallBackAddToHook(&jj->w.callBackOnWebsocketAccept, (void *) jsVarCallbackOnWebsocketAccept);
    return(jj);
}

int jsVarSendData(struct jsVaraio *jj, char *data, int len) {
    len = baioWriteToBuffer(&jj->w.b, data, len);
    wsaioWebsocketCompleteDataMessage(&jj->w);
    return(len);
}

int jsVarIsActiveConnection(struct baio *bb, int userRuntimeType) {
	if (bb == NULL) return(0);
	if ((bb->status & BAIO_STATUS_ACTIVE) == 0) return(0);
	if (bb->userRuntimeType != userRuntimeType) return(0);
	return(1);
}

int jsVarSendDataAll(char *data, int len) {
	int 				i;
    struct jsVaraio 	*jj;
	
	for(i=0; i<BAIO_MAX_CONNECTIONS; i++) {
		// TODO: Keep list of all connected clients ?
		if (jsVarIsActiveConnection(baioTab[i], JSVAR_CON_WEBSOCKET_SERVER_CLIENT)) {
            jj = (struct jsVaraio *) baioTab[i];
            jsVarSendData(jj, data, len);
        }
	}
    return(len);
}

int jsVarVEval(struct jsVaraio *jj, char *fmt, va_list arg_ptr) {
    int                 len;
    struct jsVarDstr    *ss;

    ss = jsVarDstrCreateByVPrintf(fmt, arg_ptr);
    len = baioWriteToBuffer(&jj->w.b, ss->s, ss->size);
    wsaioWebsocketCompleteMessage(&jj->w);
    jsVarDstrFree(&ss);
    return(len);
}

int jsVarEval(struct jsVaraio *jj, char *fmt, ...) {
    int             res;
    va_list         arg_ptr;

    if (jj == NULL) return(-1);
    va_start(arg_ptr, fmt);
    res = jsVarVEval(jj, fmt, arg_ptr);
    va_end(arg_ptr);
    return(res);
}

void jsVarEvalAll(char *fmt, ...) {
    struct jsVaraio *jj;
    int             i;
    va_list         arg_ptr;
    
	va_start(arg_ptr, fmt);
   
	for(i=0; i<BAIO_MAX_CONNECTIONS; i++) {
		// TODO: Keep list of all connected clients ?
		if (jsVarIsActiveConnection(baioTab[i], JSVAR_CON_WEBSOCKET_SERVER_CLIENT)) {
			// assert(baioMsgInProgress(baioTab[i]));
            // printf("sending\n");fflush(stdout);
            jj = (struct jsVaraio *) baioTab[i];
            jsVarVEval(jj, fmt, arg_ptr);
        }
	}

	va_end(arg_ptr);
}

#if JSVAR_USE_WCHAR_T
int jsVarVWEval(struct jsVaraio *jj, wchar_t *fmt, va_list arg_ptr) {
    int                 len;
    struct jsVarWDstr   *ss;
    struct jsVarDstr    *sss;

    ss = jsVarWDstrCreateByVPrintf(fmt, arg_ptr);
	// jsVarWDstrAddCharacter(ss, 0);
	sss = jsVarDstrCreate();
	jsVarDstrExpandToSize(sss, (ss->size+1) * sizeof(wchar_t));
	len = wcstombs(sss->s, ss->s, sss->allocatedSize);
	if (len < 0) {
		printf("%s: %s:%d: Error: wcstombs is unable to convert wchar_t to multibyte. Probably your 'locale' is not UTF-8 compatible.\n", JSVAR_PRINT_PREFIX(), __FILE__, __LINE__);
	} else {
		sss->size = len;
		len = baioWriteToBuffer(&jj->w.b, sss->s, sss->size);
		wsaioWebsocketCompleteMessage(&jj->w);
	}
    jsVarDstrFree(&sss);
    jsVarWDstrFree(&ss);
    return(len);
}

int jsVarWEval(struct jsVaraio *jj, wchar_t *fmt, ...) {
    int             res;
    va_list         arg_ptr;

    if (jj == NULL) return(-1);
    va_start(arg_ptr, fmt);
    res = jsVarVWEval(jj, fmt, arg_ptr);
    va_end(arg_ptr);
    return(res);
}

void jsVarWEvalAll(wchar_t *fmt, ...) {
    struct jsVaraio *jj;
    int             i;
    va_list         arg_ptr;
    
	va_start(arg_ptr, fmt);
   
	for(i=0; i<BAIO_MAX_CONNECTIONS; i++) {
		// TODO: Keep list of all connected clients ?
        if (jsVarIsActiveConnection(baioTab[i], JSVAR_CON_WEBSOCKET_SERVER_CLIENT)) {
			// assert(baioMsgInProgress(baioTab[i]));
            // printf("sending\n");fflush(stdout);
            jj = (struct jsVaraio *) baioTab[i];
            jsVarVWEval(jj, fmt, arg_ptr);
        }
	}

	va_end(arg_ptr);
}
#endif


//////////////////////////////////////////////////////////////////////////////////////////////////
