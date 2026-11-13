#include "user/test/test.h"

/* 统计 v 在数组里出现的次数，用于校验回收的 pid 集合（不引入可写全局变量） */
static u32 count_of(const u64 *arr, u32 n, u64 v) {
    u32 c = 0;
    for (u32 i = 0; i < n; i ++) {
        if (arr[i] == v) c ++;
    }
    return c;
}

/* fork / wait / exit / getpid 的回归测试 */
void test_proc_suite(TestCtx *t) {
    u64 pid;
    u64 got;
    i64 st;

    /* ---------------- 基本语义 ---------------- */
    test_case(t, "fork/getpid: 父返回子 pid、子返回 0");
    test_expect(t, "根进程 getpid() == 1", (i64)getpid(), 1);

    pid = fork();
    if (pid == 0) {
        exit(69);
    }

    test_expect(t, "fork() 在父进程中返回合法 pid", (i64)(pid > 1), 1);
    st = -1;

    // BUG: ???
    got = wait(&st);

    test_expect(t, "wait() 返回的 pid 等于 fork() 的返回值", (i64)got, (i64)pid);
    test_expect(t, "子进程里 fork() 返回 69", st, 69);

    /* 子进程 pid 一致性 */
    test_case(t, "getpid: 子进程自己的 pid 与 fork 返回值一致");
    pid = fork();
    if (pid == 0) {
        exit((i64)getpid());   /* 用退出码把 getpid() 带回去 */
    }
    st = 0;
    got = wait(&st);
    test_expect(t, "wait() 拿到的 pid == 子进程 getpid()", (i64)got, st);

    test_case(t, "fork: 三个兄弟进程，pid 与退出码一一对应");
    {
        u64 pids[3];
        u64 reaped[3];
        u32 n = 3;
        u32 i;

        for (i = 0; i < n; i ++) {
            pids[i] = fork();
            /* 退出码 = 自己的 pid，父进程据此判断配对是否正确 */
            if (pids[i] == 0) {
                exit((i64)getpid());
            }
        }

        test_expect(t, "三次 fork 拿到三个互不相同的 pid",
            (i64)(pids[0] > 1 && pids[1] > 1 && pids[2] > 1 &&
                  pids[0] != pids[1] && pids[1] != pids[2] && pids[0] != pids[2]), 1);

        int paired = 1;
        for (i = 0; i < n; i ++) {
            st = 0;
            reaped[i] = wait(&st);
            if ((i64)reaped[i] == -1) paired = 0;
            /* 退出码必须等于该子进程自己的 pid，否则说明僵尸被认错 */
            if (st != (i64)reaped[i]) paired = 0;
        }
        test_expect(t, "每次 wait 拿到的退出码都等于该子进程的 pid", paired, 1);

        int one_to_one = 1;
        for (i = 0; i < n; i ++) {
            if (count_of(reaped, n, pids[i]) != 1) one_to_one = 0;
        }
        test_expect(t, "三个孩子各被回收一次，没有漏也没有重复", one_to_one, 1);
    }

    /* ---------------- 退出码透传 ---------------- */
    test_case(t, "wait: 退出码透传");
    pid = fork();
    if (pid == 0) exit(42);
    st = 0;
    got = wait(&st);
    test_expect(t, "退出码 42 被正确回传", st, 42);

    test_case(t, "wait: 64 位退出码不被截断");
    pid = fork();
    if (pid == 0) exit(0xdeadbeef);
    st = 0;
    got = wait(&st);
    test_expect(t, "退出码 0xdeadbeef 完整回传", st, 0xdeadbeef);

    /* ---------------- wait(NULL) ---------------- */
    test_case(t, "wait: 传入 NULL 时不写用户内存，仍返回 pid");
    pid = fork();
    if (pid == 0) exit(7);
    got = wait(NULL);
    test_expect(t, "wait(NULL) 返回子进程 pid", (i64)got, (i64)pid);

    /* ---------------- 地址空间隔离 ---------------- */
    test_case(t, "fork: 父子地址空间必须隔离");
    {
        volatile i64 mark = 0x1111;

        pid = fork();
        if (pid == 0) {
            /* 子进程应该看到 fork 前的数据，改动不能影响父进程 */
            if (mark != 0x1111) exit(1);
            mark = 0x2222;
            exit(0);
        }

        st = -1;
        wait(&st);
        test_expect(t, "子进程能读到父进程 fork 前的数据", st, 0);
        test_expect(t, "父进程的栈变量未被写穿", mark, 0x1111);
    }

    /* ---------------- 嵌套 fork ---------------- */
    test_case(t, "fork: 三层进程树，孙子的退出码经两层 wait 回传");
    pid = fork();
    if (pid == 0) {
        u64 g = fork();
        if (g == 0) exit(7);

        i64 gst = 0;
        u64 gt = wait(&gst);
        if ((i64)gt == -1) exit(1);   /* 拿不到孙子 */
        if (gst != 7) exit(2);        /* 退出码不对 */
        exit((i64)getpid());  /* 孩子把自己的 pid 作为退出码 */
    }
    st = 0;
    got = wait(&st);
    test_expect(t, "孩子拿回孙子退出码 7 后正常退出", st, (i64)pid);

    /* ---------------- 孤儿进程过继 ---------------- */
    /*
     * A 生了 B 之后不等 B 就退出，abandon_child() 会把 B 过继给 process_list[1]，
     * 于是根进程可以用第二次 wait 回收 B。
     */
    test_case(t, "fork: 子进程先退出，孙子被过继给根进程");
    pid = fork();
    if (pid == 0) {
        u64 b = fork();
        if (b == 0) exit(99);
        exit(5);              /* A 不回收 B，直接退出 */
    }
    st = 0;
    wait(&st);
    test_expect(t, "第一次 wait 拿到子进程 A 的退出码 5", st, 5);

    st = 0;
    wait(&st);
    test_expect(t, "第二次 wait 回收被过继的孙子退出码 99", st, 99);

    /* ---------------- 非法 wait 地址 ---------------- */
    test_case(t, "wait: 非法用户地址不回收子进程，可以重试");
    pid = fork();
    if (pid == 0) exit(9);
    got = wait((void *)0xdead0000UL);
    test_expect(t, "wait(非法地址) 返回 -1", (i64)got, -1);

    st = 0;
    got = wait(&st);
    test_expect(t, "重试 wait() 仍能回收子进程", (i64)got, (i64)pid);
    test_expect(t, "退出码 9 没有丢失", st, 9);

    /* ---------------- 槽位耗尽 ---------------- */
    test_case(t, "fork: 槽位耗尽时返回 -1，回收后恢复");
    {
        u64 kids[16];
        u32 n = 0;
        u64 p = 0;

        while (n < 16) {
            p = fork();
            if ((i64)p == -1) break;
            if (p == 0) exit(0);
            kids[n ++] = p;
        }

        test_expect(t, "槽位耗尽后 fork() 返回 -1 而不是崩溃", (i64)p, -1);
        test_expect(t, "同时最多 14 个用户进程（16 - idle - 根进程）", (i64)n, 14);

        for (u32 i = 0; i < n; i ++) {
            wait(NULL);
        }

        p = fork();
        if (p == 0) exit(0);
        test_expect(t, "全部回收后 fork() 恢复正常", (i64)(p > 1), 1);
        wait(NULL);
    }

    /* ---------------- 无子进程时 wait ---------------- */
    /*
     * kwait() 找不到孩子时会 kkill(自己)，is_killed 在下一次 trap 返回前被检查，
     * 于是进程直接 kexit(-1)，根本回不到用户态。
     * 正确语义应该只是 wait() 返回 -1，进程继续跑。
     */
    test_case(t, "wait: 没有子进程时应只返回 -1，不能杀死调用者");
    pid = fork();
    if (pid == 0) {
        u64 r = wait(NULL);
        if (r == (u64)-1) {
            println("child: wait() 没有子进程 -> -1");
            exit(55);         /* 期望能走到这里 */
        }
        exit(56);
    }
    st = 0;
    got = wait(&st);
    test_expect(t, "子进程收到 -1 后仍能继续执行到 exit(55)", st, 55);
    test_expect(t, "父进程正确回收该子进程", (i64)got, (i64)pid);
}
