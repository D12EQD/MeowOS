#include "kernel/lib/def.h"
#include "kernel/lib/string.h"
#include "kernel/lib/printk.h"
#include "kernel/arch/sbi.h"

void *memcpy(void *dst, const void *src, size_t n) {
    u8 *d = (u8 *) dst;
    const u8 *s = (const u8 *) src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void *memset(void *buf, char c, size_t n) {
    u8 *p = (u8 *) buf;
    while (n--)
        *p++ = c;
    return buf;
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
