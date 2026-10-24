#pragma once
#include "kernel/lib/typedef.h"

#define offsetof(type, member)   __builtin_offsetof(type, member)
#define align_up(value, align)   __builtin_align_up(value, align)
#define is_aligned(value, align) __builtin_is_aligned(value, align)

#define va_list  __builtin_va_list
#define va_start __builtin_va_start
#define va_end   __builtin_va_end
#define va_arg   __builtin_va_arg

#define likely(x)	(__builtin_expect(!!(x), 1))
#define unlikely(x)	(__builtin_expect(!!(x), 0))

#define container_of(ptr, type, member)             \
	_Generic(ptr,                                   \
		const typeof(*(ptr)) *: ((const type *)container_of(ptr, type, member)),\
		default: ((type *)container_of(ptr, type, member))	\
	)

#define __always_inline inline __attribute__((__always_inline__))

#ifndef _ASM
#define _ASM __asm__ __volatile__
#endif

#define max(a, b) (( a ) > ( b ) ? ( a ) : ( b ))
#define min(a, b) (( a ) < ( b ) ? ( a ) : ( b ))

#define __get_bit(b, l, r) (((b) >> (l)) & ((1ULL << ((r) - (l) + 1)) - 1))
#define __set_bit(b, l, r) ((b) |= (((1ULL << ((r) - (l) + 1)) - 1) << (l)))

#define div_ceil(dividend, divisor) (dividend + divisor - 1) / divisor
