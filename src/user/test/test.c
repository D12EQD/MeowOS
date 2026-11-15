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

/* 执行测试函数 */
void test_case(TestCtx *t, const char *name) {
    (void)t;
    printf_imple(ANSI_CYAN "\n[ CASE ] %s" ANSI_RESET "\n", name);
}

/* 判断got是否等于want */
void test_expect(TestCtx *t, const char *what, i64 got, i64 want) {
    t->total ++;

    if (got == want) {
        t->pass ++;
        printf_imple(ANSI_GREEN "[ PASS ]" ANSI_RESET " %s\n", what);
        return;
    }

    t->fail ++;
    printf_imple(
        ANSI_RED "[ FAIL ]" ANSI_RESET " %s  got=%d(0x%x) want=%d(0x%x)\n",
        what, got, (u64)got, want, (u64)want
    );
}

/* 注意输出 */
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

void test_pipe_basic(TestCtx* t){
    test_case(t, "测试管道基本读写");

    i64 rfd, wfd;
    test_expect(t, "创建管道返回 0", pipe(&rfd, &wfd), 0);
    test_expect(t, "读口 fd 合法", rfd >= 0, true);
    test_expect(t, "写口 fd 合法", wfd >= 0, true);
    test_expect(t, "读口和写口不同", rfd != wfd, true);

    char msg[] = "hello pipe";
    test_expect(t, "写入 10 字节", write(wfd, msg, 10), 10);

    char buf[16];
    memset(buf, 0, sizeof(buf));
    test_expect(t, "读出 10 字节", read(rfd, buf, 10), 10);
    test_expect(t, "内容一致", strcmp(buf, "hello pipe"), 0);

    close(rfd);
    close(wfd);
}

void test_pipe_eof(TestCtx* t){
    test_case(t, "测试管道 EOF");

    i64 rfd, wfd;
    pipe(&rfd, &wfd);

    char msg[] = "abc";
    write(wfd, msg, 3);
    close(wfd);

    char buf[8];
    memset(buf, 0, sizeof(buf));
    test_expect(t, "读完剩余 3 字节", read(rfd, buf, 8), 3);
    test_expect(t, "内容正确", strcmp(buf, "abc"), 0);
    test_expect(t, "再读返回 EOF(0)", read(rfd, buf, 8), 0);

    close(rfd);
}

void test_pipe_fork(TestCtx* t){
    test_case(t, "测试管道跨 fork 数据流");

    i64 rfd, wfd;
    pipe(&rfd, &wfd);

    u64 pid = fork();
    if (pid == 0){
        close(rfd);
        char msg[] = "hello from child";
        write(wfd, msg, 16);
        close(wfd);
        exit(0);
    }

    close(wfd);
    char buf[32];
    memset(buf, 0, sizeof(buf));

    test_expect(t, "父读 16 字节", read(rfd, buf, 16), 16);
    test_expect(t, "内容来自子进程", strcmp(buf, "hello from child"), 0);
    test_expect(t, "写端关掉后再读 EOF", read(rfd, buf, 4), 0);

    wait(NULL);
    close(rfd);
}

void test_pipe_epipe(TestCtx* t){
    test_case(t, "测试管道 EPIPE");

    i64 rfd, wfd;
    pipe(&rfd, &wfd);

    close(rfd);

    char msg[] = "x";
    test_expect(t, "读端关闭后写入失败", write(wfd, msg, 1), -1);

    close(wfd);
}

void test_pipe(TestCtx *t){
    test_pipe_basic(t);
    test_pipe_eof(t);
    test_pipe_fork(t);
    test_pipe_epipe(t);
}

void test_run_all(void) {
    TestCtx t;
    test_ctx_init(&t);

    printf_imple(ANSI_BRIGHT_GREEN "\n===== MeowOS syscall test suite =====" ANSI_RESET "\n");

    test_page_alloc_and_free(&t);
    test_global_data_suite(&t);
    test_write_suite(&t);
    test_proc_suite(&t);
    test_fs(&t);
    test_read_suite(&t);
    test_pipe(&t);

    println("OK");

    test_summary(&t);
}
