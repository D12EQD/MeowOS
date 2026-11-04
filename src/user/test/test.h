#pragma once
#include "user/def.h"
#include "user/lib/lib.h"
#include "user/lib/print.h"
#include "user/os/syscall.h"

/*
 * 用户态系统调用测试框架
 *
 * 注意 1：内核 first_user_init() 把整个用户镜像映射成 PTE_R | PTE_X，
 *         没有可写数据段。所以本目录下禁止使用全局变量和函数内 static 变量，
 *         状态一律放在栈上的 TestCtx 里（写 .data/.bss 会 page fault -> 内核 PANIC）。
 *
 * 注意 2：PROCESS_MAX_COUNT = 16，0 号是 idle、1 号是根进程，
 *         最多还能同时存在 14 个用户进程；fork 之后必须 wait 回收。
 *
 * 注意 3：根进程没有父进程，从 user_main 返回会走进 kexit 的 PANIC 分支，
 *         所以测试跑完只能停在死循环里（用户态没有 halt/wfi 之类的手段）。
 */
typedef struct {
    u32 total;
    u32 pass;
    u32 fail;
    u32 note;
} TestCtx;

void test_run_all(void);

void test_ctx_init(TestCtx *t);
void test_case(TestCtx *t, const char *name);
void test_expect(TestCtx *t, const char *what, i64 got, i64 want);
void test_note(TestCtx *t, const char *what);
void test_summary(TestCtx *t);

void test_write_suite(TestCtx *t);
void test_proc_suite(TestCtx *t);

/*
 * user/os/syscall.h 只封装了 fork/read/write/wait，
 * 这里补上测试要用到的 getpid/exit（用裸 syscall 走一遍编号分发）。
 */
static inline u64 sys_getpid_raw(void) {
    return syscall(SYS_GETPID, 0, 0, 0, 0, 0, 0);
}

/* 没有返回值：sys_exit 一旦成功就不回来了 */
static inline void sys_exit_raw(i64 code) {
    syscall(SYS_EXIT, (u64)code, 0, 0, 0, 0, 0);
}
