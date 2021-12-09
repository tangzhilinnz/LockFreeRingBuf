#ifndef __PORTABILITY_H
#define __PORTABILITY_H

#ifdef _MSC_VER
#define NANOLOG_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__)
#define NANOLOG_ALWAYS_INLINE inline __attribute__((__always_inline__))
#else
#define NANOLOG_ALWAYS_INLINE inline
#endif

#ifdef _MSC_VER
#define NANOLOG_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
#define NANOLOG_NOINLINE __attribute__((__noinline__))
#else
#define NANOLOG_NOINLINE
#endif


// difference between #pragma pack (push,1) and #pragma pack (1)
// This is the parameter setting for the compiler, for the structure byte alignment
// setting, #pragma pack is the alignment of the specified data in memory.
// 
// #pragma pack (n) Action: The C compiler will be aligned by N bytes.
// #pragma pack () Effect: cancels custom byte alignment.
//
// #pragma pack (push, 1) function: means to set the original alignment of the 
// stack, and set a new alignment to one byte alignment
// #pragma pack (POP) Action: Restore Alignment State
// 
// As a result, adding push and pop allows alignment to revert to its original 
// state, rather than the compiler default, which can be said to be better, but
// many times the difference is small
// Such as:
// #pragma pack (push): save alignment status
// #pragma pack (4): set to 4-byte alignment, equivalent to #pragma pack (push, 4)
// #pragma pack (1): adjust the boundary alignment of the structure so that it 
//                   aligns in one byte(to align the structure in 1-byte alignment)
// #pragma pack ()
// For example:
// #pragma pack (1)
// struct sample {
//   Char A;
//   Double b;
// };
// #pragma pack ()
// Note: If you do not enclose #pragma pack (1) and #pragma pack (), sample is 
// aligned by the compiler default (that is, the size of the largest member). 
// That is, by 8-byte (double) alignment, sizeof (sample) == 16. Member Char A 
// takes up 8 bytes (7 of which are empty bytes);
// If #pragma pack (1) is used, sizeof (sample) == 9 is aligned to by 1 bytes. 
// (No empty bytes), more space-saving.


#ifdef _MSC_VER
#define NANOLOG_PACK_PUSH __pragma(pack(push, 1))
#define NANOLOG_PACK_POP  __pragma(pack(pop))
#elif defined(__GNUC__)
#define NANOLOG_PACK_PUSH _Pragma("pack(push, 1)")
#define NANOLOG_PACK_POP _Pragma("pack(pop)")
#else
#define NANOLOG_PACK_PUSH
#define NANOLOG_PACK_POP
#endif

#if _MSC_VER

#ifdef _USE_ATTRIBUTES_FOR_SAL
#undef _USE_ATTRIBUTES_FOR_SAL
#endif

#define _USE_ATTRIBUTES_FOR_SAL 1
#include <sal.h>

#define NANOLOG_PRINTF_FORMAT _Printf_format_string_
#define NANOLOG_PRINTF_FORMAT_ATTR(string_index, first_to_check)

#elif defined(__GNUC__)
#define NANOLOG_PRINTF_FORMAT
#define NANOLOG_PRINTF_FORMAT_ATTR(string_index, first_to_check) \
  __attribute__((__format__(__printf__, string_index, first_to_check)))
#else
#define NANOLOG_PRINTF_FORMAT
#define NANOLOG_PRINTF_FORMAT_ATTR(string_index, first_to_check)
#endif

#endif /* __PORTABILITY_H */

// __attribute__((format(function, string-index, first-to-check)))
// This attribute causes the compiler to check that the supplied arguments are
// in the correct format for the specified function.
// Where function is a printf-style function, such as printf(), scanf(),
// strftime(), gnu_printf(), gnu_scanf(), gnu_strftime(), or strfmon().
// string-index: specifies the index of the string argument in your function 
//               (starting from one). 
// first-to-check: is the index of the first argument to check against the
//                 format string.