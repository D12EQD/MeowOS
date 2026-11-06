#pragma once
#include "kernel/lib/def.h"

typedef struct {
    size_t      count;
    const char *data;
} StringView;

StringView sv_from_parts(const char *data, size_t count);
StringView sv_from_cstr (const char *cstr);

#define sv_lit(lit) sv_from_parts((lit), sizeof(lit) - 1)

StringView sv_chop_while (StringView *sv, int (*p)(int c));
StringView sv_chop_by_delim(StringView *sv, char delim);
StringView sv_chop_left (StringView *sv, size_t n);
StringView sv_chop_right (StringView *sv, size_t n);
bool sv_chop_prefix  (StringView *sv, StringView prefix);
bool sv_chop_suffix  (StringView *sv, StringView suffix);

StringView sv_trim (StringView sv);
StringView sv_trim_left (StringView sv);
StringView sv_trim_right(StringView sv);

bool sv_eq (StringView a, StringView b);
bool sv_eq_cstr (StringView a, const char *cstr);
bool sv_starts_with (StringView sv, StringView prefix);
bool sv_ends_with (StringView sv, StringView suffix);
bool sv_ends_with_cstr (StringView sv, const char *cstr);

bool sv_to_cstr_buf(StringView sv, char *buf, size_t buf_size);

/* 
 * printf 格式
 * printk(SV_Fmt, SV_Arg(sv))）
 */
#ifndef SV_Fmt
#define SV_Fmt "%.*s"
#endif
#ifndef SV_Arg
#define SV_Arg(sv) (int)(sv).count, (sv).data
#endif

size_t strnlen(const char *s, size_t limit);
size_t strlen (const char *s);
int    strcmp (const char *s1, const char *s2);
char  *strcpy (char *dst, const char *src);

u64    memcpy(void *dst, const void *src, size_t n);
u64    memset(void *buf, char c, size_t n);
i64    memcmp(const void *s1, const void *s2, u64 n);
