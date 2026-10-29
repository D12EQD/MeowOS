#pragma once
#include "kernel/arch/trap.h"
#include "kernel/arch/def.h"
#include "kernel/lib/def.h"

typedef enum {
    K_INFO      = 1 << 0,
    K_WARNING   = 1 << 1,
    K_DEBUG     = 1 << 2,
    K_ERROR     = 1 << 3,
}KernelPrintLevel;

// 换行
#define pr_info(fmt, ...)   printk(K_INFO,  fmt "\n", ##__VA_ARGS__)
#define pr_warn(fmt, ...)   printk(K_WARNING, fmt "\n", ##__VA_ARGS__)
#define pr_error(fmt, ...)  printk(K_ERROR, fmt "\n", ##__VA_ARGS__)
#define pr_debug(fmt, ...)  printk(K_DEBUG, fmt "\n", ##__VA_ARGS__)

// 不换行 continue 的缩写
#define pr_cont_info(fmt, ...)   printk(K_INFO,  fmt, ##__VA_ARGS__)
#define pr_cont_warn(fmt, ...)   printk(K_WARNING, fmt, ##__VA_ARGS__)
#define pr_cont_error(fmt, ...)  printk(K_ERROR, fmt, ##__VA_ARGS__)
#define pr_cont_debug(fmt, ...)  printk(K_DEBUG, fmt, ##__VA_ARGS__)


int vsprintf(char *buf, const char *fmt, va_list args);
int sprintf(char *buf, const char *fmt, ...);
int printk(KernelPrintLevel level, const char *fmt, ...);

void set_print_filter(u64 new_level);
void puts(char *s);
void puts_n(char *s, u64 count);

#define PANIC(fmt, ...) do {                                        \
    interrupt_off();                                                \
    pr_error("Kernel Panic for the reason : " fmt, ##__VA_ARGS__);  \
    while (1);                                                      \
} while (0)

#define ASSERT(cond) do{ if unlikely(!( cond )){ PANIC( "assert error " __FILE__ ":%lu", __LINE__); } }while(0)
