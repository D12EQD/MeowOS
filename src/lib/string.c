#include "lib/def.h"
#include "lib/string.h"

#include "arch/sbi.h"
/* 
 * Console Putchar 
 * use stb_call
 */

size_t strnlen(const char * s, int limit){
    int res;
    for (res = 0; res < limit && s[res]; ++ res);
    return res;
}

size_t strlen(const char *s){
    int res;
    for (res = 0; s[res]; res ++);
    return res;
}

void puts(char *s){
    int idx; char a;
    for (idx = 0, a = s[0]; a; idx ++, a = s[idx]){
        putchar(a);
    }
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
