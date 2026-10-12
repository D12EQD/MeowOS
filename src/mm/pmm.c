#include "lib/def.h"
#include "lib/bitmap.h"

#include "kernel/kernel.h"

extern char __free_ram[], __free_ram_end[];

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

paddr_t alloc_pages(u64 n) {
    static paddr_t next_paddr = (paddr_t) __free_ram;
    paddr_t paddr = next_paddr;
    next_paddr += n * KERNEL_PAGE_SIZE;

    if (next_paddr > (paddr_t) __free_ram_end) PANIC("out of memory");
    return paddr;
}
