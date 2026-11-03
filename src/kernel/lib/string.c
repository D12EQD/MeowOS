#include "kernel/lib/def.h"
#include "kernel/lib/string.h"
#include "kernel/lib/printk.h"
#include "kernel/arch/sbi.h"

/* 返回拷贝的字节数量 */
u64 memcpy(void *dst, const void *src, size_t n) {
    u8 *d = (u8 *) dst;
    const u8 *s = (const u8 *) src;
    u64 res = 0;
    while (n--){
        *d++ = *s++;
        res ++;
    }
    return res;
}

/* 返回赋值的字节数量 */
u64 memset(void *buf, char c, size_t n) {
    u8 *p = (u8 *) buf;
    u64 res = 0;
    while (n --){
        *p++ = c;
        res ++;
    }
    return res;
}

size_t strnlen(const char * s, size_t limit){
    size_t res;
    for (res = 0; res < limit && s[res]; ++ res);
    return res;
}

size_t strlen(const char *s){
    int res;
    for (res = 0; s[res]; res ++);
    return res;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while (*src)
        *d++ = *src++;
    *d = '\0';
    return dst;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2)
            break;
        s1++;
        s2++;
    }

    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

i64 memcmp(const void *s1, const void *s2, u64 n){
    const char *p1 = s1, *p2 = s2;
    while (n-- > 0) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}


