#include "user/user_main.h"


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

u64 syscall(u64 sysnumber, u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {
    register u64 a0 __asm__("a0") = arg0;
    register u64 a1 __asm__("a1") = arg1;
    register u64 a2 __asm__("a2") = arg2;
    register u64 a3 __asm__("a3") = arg3;
    register u64 a4 __asm__("a4") = arg4;
    register u64 a5 __asm__("a5") = arg5;
    register u64 a7 __asm__("a7") = sysnumber;   // syscall 号放 a7

    __asm__ volatile (
        "ecall"
        : "+r"(a0)
        : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a7)
        : "memory"
    );

    return a0;
}

u64 write(i64 fd, void* buf, u64 buf_len){
    return syscall(SYS_WRITE, (u64)fd, (u64)buf, buf_len, 0, 0, 0);
}

u64 read(i64 fd, void* buf, u64 buf_len){
    return syscall(SYS_READ, (u64)fd, (u64)buf, buf_len, 0, 0, 0);
}

const char * msg = "Hello Kernel\n";

int user_main(){
    write(1, (void *)msg, strlen(msg));
    while (1);
    return 0;
}
