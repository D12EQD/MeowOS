#include "user/test/test.h"

/*
 * 测试框架实现
 * 打印格式保持单行，方便脚本 grep：
 *   [ PASS ] xxx
 *   [ FAIL ] xxx  got=0x.. want=0x..
 *   [ NOTE ] xxx
 * 结尾一行汇总：==== TEST SUMMARY pass=.. fail=.. note=.. total=.. ====
 */

void test_ctx_init(TestCtx *t) {
    t->total = 0;
    t->pass = 0;
    t->fail = 0;
    t->note = 0;
}

void test_case(TestCtx *t, const char *name) {
    (void)t;
    printf_imple(ANSI_CYAN "\n[ CASE ] %s" ANSI_RESET "\n", name);
}

void test_expect(TestCtx *t, const char *what, i64 got, i64 want) {
    t->total ++;

    if (got == want) {
        t->pass ++;
        printf_imple(ANSI_GREEN "[ PASS ]" ANSI_RESET " %s\n", what);
        return;
    }

    t->fail ++;
    printf_imple(ANSI_RED "[ FAIL ]" ANSI_RESET " %s  got=%d(0x%x) want=%d(0x%x)\n",
        what, got, (u64)got, want, (u64)want);
}

void test_note(TestCtx *t, const char *what) {
    t->note ++;
    printf_imple(ANSI_YELLOW "[ NOTE ]" ANSI_RESET " %s\n", what);
}

void test_summary(TestCtx *t) {
    printf_imple("\n==== TEST SUMMARY pass=%u fail=%u note=%u total=%u ====\n",
        (u64)t->pass, (u64)t->fail, (u64)t->note, (u64)t->total);

    if (t->fail == 0) {
        printf_imple(ANSI_GREEN "ALL GREEN" ANSI_RESET "\n");
    } else {
        printf_imple(ANSI_RED "FAILED" ANSI_RESET "\n");
    }
}

void test_run_all(void) {
    TestCtx t;
    test_ctx_init(&t);

    printf_imple(ANSI_BRIGHT_GREEN "\n===== MeowOS syscall test suite =====" ANSI_RESET "\n");

    test_write_suite(&t);
    test_proc_suite(&t);

    test_summary(&t);
}
