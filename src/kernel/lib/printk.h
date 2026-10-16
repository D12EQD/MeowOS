#pragma once
#include "kernel/lib/def.h"

typedef enum {
    K_INFO      = 1 << 0,
    K_WARNING   = 1 << 1,
    K_DEBUG     = 1 << 2,
    K_ERROR     = 1 << 3,
}KernelPrintLevel;

#define pr_info(fmt, ...)   printk(K_INFO,  fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)   printk(K_WARNING, fmt, ##__VA_ARGS__)
#define pr_error(fmt, ...)  printk(K_ERROR, fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...)  printk(K_DEBUG, fmt, ##__VA_ARGS__)

int vsprintf(char *buf, const char *fmt, va_list args);
int sprintf(char *buf, const char *fmt, ...);
int printk(KernelPrintLevel level, const char *fmt, ...);

void set_print_filter(u64 new_level);
void puts(char *s);

#define PANIC(fmt, ...) do { \
    pr_error("Kernel Panic in %s:%d\n", __FILE__, __LINE__);    \
    pr_error("Kernel Panic for the reason" fmt, ##__VA_ARGS__); \
    while (1);                                                  \
} while (0)

