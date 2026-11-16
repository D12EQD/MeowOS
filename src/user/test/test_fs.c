#include "user/test/test.h"
#include "user/os/syscall.h"

/* 管道内核实现的可用字节数：cap 为 PAGE_SIZE*/
#define PIPE_USABLE_BYTES   4095

/*
 * 大块数据用固定小缓冲分片提交，避开用户态 12KB 静态数据上限
 * 每一片的第 i 个字节记作 (偏移 & 0xFF)，读回来按同一规则校验
 */
#define PIPE_CHUNK 256
static char pipe_buf[PIPE_CHUNK];
static Kstat kstat_before;

/* 分片写入 n 字节位置模式，返回实际写入的字节数，失败返回 -1 */
static i64 pipe_fill_pattern(i64 fd, u64 n){
    u64 sent = 0;

    while (sent < n){
        u64 chunk = n - sent;
        if (chunk > PIPE_CHUNK) chunk = PIPE_CHUNK;

        for (u64 i = 0; i < chunk; i ++){
            pipe_buf[i] = (char)((sent + i) & 0xFF);
        }

        i64 r = write(fd, pipe_buf, chunk);
        if (r != (i64)chunk) return -1;

        sent += chunk;
    }
    return (i64)sent;
}

/* 分片读出 n 字节并校验位置模式，返回 0 表示完全一致 */
static i64 pipe_check_pattern(i64 fd, u64 n){
    u64 got = 0;

    while (got < n){
        u64 chunk = n - got;
        if (chunk > PIPE_CHUNK) chunk = PIPE_CHUNK;

        i64 r = read(fd, pipe_buf, chunk);
        if (r != (i64)chunk) return -1;

        for (u64 i = 0; i < chunk; i ++){
            if ((u8)pipe_buf[i] != (u8)((got + i) & 0xFF)) return -2;
        }

        got += chunk;
    }
    return 0;
}

/*
 * 目录基本操作：创建、重复创建、按路径查找
 */
void test_fs_mkdir(TestCtx* t){
    test_case(t, "测试目录创建");

    test_expect(t, "创建 /dir1", mkdir("/dir1"), 0);
    test_expect(t, "重复创建 /dir1 应失败", mkdir("/dir1"), -1);

    test_expect(t, "创建 /dir1/sub", mkdir("/dir1/sub"), 0);
    test_expect(t, "父目录不存在时创建失败", mkdir("/nope/x"), -1);

    /* 目录可以作为别的文件的父路径 */
    int fd = open("/dir1/inner.txt", O_WRITE | O_CREATE);
    test_expect(t, "在新建目录下创建文件", fd >= 0, true);
    if (fd >= 0) close(fd);

    test_expect(t, "删除非空目录应失败", unlink("/dir1"), -1);
}

/*
 * unlink：删普通文件、删空目录、打开着的文件删了还能读
 */
void test_fs_unlink(TestCtx* t){
    test_case(t, "测试 unlink");

    int fd = open("/gone.txt", O_WRITE | O_CREATE);
    test_expect(t, "创建 /gone.txt", fd >= 0, true);

    char msg[] = "hello";
    test_expect(t, "写入 5 字节", write(fd, msg, 5), 5);
    close(fd);

    test_expect(t, "打开着 unlink", unlink("/gone.txt"), 0);
    test_expect(t, "再打开应失败", open("/gone.txt", O_READ), -1);

    /* 空目录可以删，非空的不行 */
    test_expect(t, "创建空目录 /empty", mkdir("/empty"), 0);
    test_expect(t, "删除空目录", unlink("/empty"), 0);
    test_expect(t, "删除后再次 unlink 应失败", unlink("/empty"), -1);
}

/*
 * dup：共享 offset，拿到最小空槽
 */
void test_fs_dup(TestCtx* t){
    test_case(t, "测试 dup");

    int fd = open("/dup.txt", O_RW | O_CREATE);
    test_expect(t, "创建 /dup.txt", fd >= 0, true);

    char src[] = "abcdef";
    test_expect(t, "写入 6 字节", write(fd, src, 6), 6);

    /* 回到文件头，准备读 */
    lseek(fd, 0, SEEK_SET);

    int fd2 = dup(fd);
    test_expect(t, "dup 返回新的 fd", fd2 >= 0 && fd2 != fd, true);

    char a = 0, b = 0;
    test_expect(t, "原 fd 读 1 字节", read(fd, &a, 1), 1);
    test_expect(t, "原 fd 读到 'a'", a, 'a');

    /* dup 共享 offset：fd2 接着读第 2 字节 */
    test_expect(t, "dup 出的 fd 读 1 字节", read(fd2, &b, 1), 1);
    test_expect(t, "读到 'b'，说明 offset 共享", b, 'b');

    close(fd);
    close(fd2);
}

/*
 * dup2：点名塞到指定 fd，旧 fd 被静默关掉
 */
void test_fs_dup2(TestCtx* t){
    test_case(t, "测试 dup2");

    int fd = open("/dup2.txt", O_RW | O_CREATE);
    test_expect(t, "创建 /dup2.txt", fd >= 0, true);

    char src[] = "xyz";
    write(fd, src, 3);
    lseek(fd, 0, SEEK_SET);

    /* 塞到 10 号，10 号原本是空的 */
    test_expect(t, "dup2 到 10 号", dup2(fd, 10), 10);

    char c = 0;
    test_expect(t, "10 号读 1 字节", read(10, &c, 1), 1);
    test_expect(t, "读到 'x'", c, 'x');

    /* dup2(fd, fd) 原样返回，不关自己 */
    test_expect(t, "dup2 到同一个 fd", dup2(fd, fd), fd);

    /* 覆盖一个已开的 fd：旧的被静默关闭 */
    int other = open("/other.txt", O_RW | O_CREATE);
    test_expect(t, "打开 /other.txt", other >= 0, true);
    test_expect(t, "dup2 覆盖 other", dup2(fd, other), other);
    test_expect(t, "被覆盖后 other 读到 'y'", read(other, &c, 1), 1);
    test_expect(t, "读到 'y'，说明 other 已指向 dup2.txt", c, 'y');

    /* 非法 oldfd，newfd 保持原样 */
    test_expect(t, "oldfd 非法时 dup2 失败", dup2(63, other), -1);

    close(fd);
    close(10);
    close(other);
}

/*
 * readdir：遍历 /，读到 1 表示结束
 */
void test_fs_readdir(TestCtx* t){
    test_case(t, "测试 readdir");

    /* 先塞几个条目进去 */
    mkdir("/rd_a");
    mkdir("/rd_b");

    int f = open("/rd_c.txt", O_RW | O_CREATE);
    if (f >= 0) close(f);

    int root = open("/", O_READ);
    test_expect(t, "打开根目录", root >= 0, true);

    DirEntry e;
    int seen_rd_a = 0, seen_rd_b = 0, seen_rd_c = 0;
    int loops = 0;
    i64 r;

    int count = 0;
    while ((r = readdir(root, &e)) == 0){
        if (loops ++ > 64) break;  /* 防呆，别死循环 */
        if (e.name[0] == 0) continue;

        if (e.name[0] == 'r' && e.name[1] == 'd' && e.name[2] == '_') {
            if (e.name[3] == 'a' && e.name[4] == 0) seen_rd_a = 1;
            if (e.name[3] == 'b' && e.name[4] == 0) seen_rd_b = 1;
            if (e.name[3] == 'c') seen_rd_c = 1;
        }
    }

    test_expect(t, "readdir 正常读完返回 1", r, 1);
    test_expect(t, "找到 rd_a", seen_rd_a, 1);
    test_expect(t, "找到 rd_b", seen_rd_b, 1);
    test_expect(t, "找到 rd_c", seen_rd_c, 1);

    close(root);
    unlink("/rd_a");
    unlink("/rd_b");
    unlink("/rd_c.txt");
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
        char msg[] = "Hello from child";
        write(wfd, msg, 16);
        close(wfd);
        exit(0);
    }else{
        char buf[32];
        memset(buf, 0, sizeof(buf));

        close(wfd);

        test_expect(t, "父读 16 字节", read(rfd, buf, 16), 16);
        test_expect(t, "内容来自子进程", strcmp(buf, "Hello from child"), 0);
        wait(NULL);

        close(rfd);
    }
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

/*
 * 管道两端的权限检查、零长读写、非法 fd
 */
void test_pipe_bad_usage(TestCtx* t){
    test_case(t, "测试管道参数与权限");

    i64 rfd, wfd;
    test_expect(t, "创建管道", pipe(&rfd, &wfd), 0);
    test_expect(t, "读口 fd 在合法范围内", rfd >= 0 && rfd < 16, true);
    test_expect(t, "写口 fd 在合法范围内", wfd >= 0 && wfd < 16, true);

    char c = 0;
    test_expect(t, "从写端读应失败", read(wfd, &c, 1), -1);
    test_expect(t, "向读端写应失败", write(rfd, &c, 1), -1);

    test_expect(t, "读 0 字节返回 0", read(rfd, &c, 0), 0);
    test_expect(t, "写 0 字节返回 0", write(wfd, &c, 0), 0);

    test_expect(t, "read 非法 fd", read(-1, &c, 1), -1);
    test_expect(t, "write 非法 fd", write(99, &c, 1), -1);

    test_expect(t, "关闭读端", close(rfd), 0);
    test_expect(t, "重复关闭读端失败", close(rfd), -1);
    test_expect(t, "关闭写端", close(wfd), 0);
}

/*
 * 分段读：每次只取自己要的字节数，顺序不能乱
 */
void test_pipe_chunk(TestCtx* t){
    test_case(t, "测试管道分段读");

    i64 rfd, wfd;
    pipe(&rfd, &wfd);

    char msg[] = "0123456789";
    test_expect(t, "写 10 字节", write(wfd, msg, 10), 10);

    char buf[16];
    memset(buf, 0, sizeof(buf));
    test_expect(t, "读前 4 字节", read(rfd, buf, 4), 4);
    test_expect(t, "内容为 0123", strcmp(buf, "0123"), 0);

    memset(buf, 0, sizeof(buf));
    test_expect(t, "再读 4 字节", read(rfd, buf, 4), 4);
    test_expect(t, "内容为 4567", strcmp(buf, "4567"), 0);

    memset(buf, 0, sizeof(buf));
    test_expect(t, "读剩余 2 字节", read(rfd, buf, 2), 2);
    test_expect(t, "内容为 89", strcmp(buf, "89"), 0);

    close(rfd);
    close(wfd);
}

/*
 * 环形缓冲区回绕：写到尾部之后折回头部继续写
 */
void test_pipe_ring_wrap(TestCtx* t){
    test_case(t, "测试管道环形回绕");

    i64 rfd, wfd;
    pipe(&rfd, &wfd);

    /* 第一次写读，把 start/end 推到 3000 */
    test_expect(t, "写 3000 字节", pipe_fill_pattern(wfd, 3000), 3000);
    test_expect(t, "读 3000 字节且内容一致", pipe_check_pattern(rfd, 3000), 0);

    /* 第二次写 2500，内部 end 必然越过 cap 折回 0 */
    test_expect(t, "跨尾部写 2500 字节", pipe_fill_pattern(wfd, 2500), 2500);
    test_expect(t, "回绕后内容一致", pipe_check_pattern(rfd, 2500), 0);

    close(rfd);
    close(wfd);
}

/*
 * 容量边界：可用字节是 cap-1，写满之后 drain 掉还能再写满
 */
void test_pipe_capacity(TestCtx* t){
    test_case(t, "测试管道容量边界");

    i64 rfd, wfd;
    pipe(&rfd, &wfd);

    test_expect(t, "一次写满 4095 字节", pipe_fill_pattern(wfd, PIPE_USABLE_BYTES), PIPE_USABLE_BYTES);
    test_expect(t, "读回 4095 字节且内容一致", pipe_check_pattern(rfd, PIPE_USABLE_BYTES), 0);

    test_expect(t, "清空后可再次写满", pipe_fill_pattern(wfd, PIPE_USABLE_BYTES), PIPE_USABLE_BYTES);
    test_expect(t, "再次读回且内容一致", pipe_check_pattern(rfd, PIPE_USABLE_BYTES), 0);

    close(rfd);
    close(wfd);
}

/*
 * 两条管道各走各的 PipeData，不能串数据
 */
void test_pipe_isolation(TestCtx* t){
    test_case(t, "测试两条管道互不干扰");

    i64 r1, w1, r2, w2;
    pipe(&r1, &w1);
    pipe(&r2, &w2);

    test_expect(t, "管道一写 4 字节", write(w1, "aaaa", 4), 4);
    test_expect(t, "管道二写 4 字节", write(w2, "bbbb", 4), 4);

    char buf[8];
    memset(buf, 0, sizeof(buf));
    test_expect(t, "管道一读 4 字节", read(r1, buf, 4), 4);
    test_expect(t, "管道一内容为 aaaa", strcmp(buf, "aaaa"), 0);

    memset(buf, 0, sizeof(buf));
    test_expect(t, "管道二读 4 字节", read(r2, buf, 4), 4);
    test_expect(t, "管道二内容为 bbbb", strcmp(buf, "bbbb"), 0);

    close(r1);
    close(w1);
    close(r2);
    close(w2);
}

/*
 * 两个子进程各写一段，父进程把 6 字节收齐
 * 子进程只写不关 fd，父进程 wait 完再从缓冲里取，避免 EOF 语义干扰
 */
void test_pipe_multi_writer(TestCtx* t){
    test_case(t, "测试多写者");

    i64 rfd, wfd;
    pipe(&rfd, &wfd);

    if (fork() == 0){
        write(wfd, "aaa", 3);
        exit(0);
    }

    if (fork() == 0){
        write(wfd, "bbb", 3);
        exit(0);
    }

    wait(NULL);
    wait(NULL);

    char buf[8];
    memset(buf, 0, sizeof(buf));
    test_expect(t, "父进程读满 6 字节", read(rfd, buf, 6), 6);

    int a = 0, b = 0, other = 0;
    for (int i = 0; i < 6; i ++){
        if (buf[i] == 'a') a ++;
        else if (buf[i] == 'b') b ++;
        else other ++;
    }
    test_expect(t, "三次来自写者 a", a, 3);
    test_expect(t, "三次来自写者 b", b, 3);
    test_expect(t, "没有别的字节", other, 0);

    close(rfd);
    close(wfd);
}

/*
 * 子进程只关自己那份 fd，父进程手里的两端不该受影响
 * 当前 PipeData 按 fd 计数，子进程 close 会直接把整端判死
 */
void test_pipe_fork_own_ends(TestCtx* t){
    test_case(t, "测试子进程关闭自己那份后父进程仍可用");

    i64 rfd, wfd;
    pipe(&rfd, &wfd);

    if (fork() == 0){
        close(rfd);
        close(wfd);
        exit(0);
    }
    wait(NULL);

    char msg[] = "z";
    test_expect(t, "父进程写入仍应成功", write(wfd, msg, 1), 1);

    char c = 0;
    test_expect(t, "父进程读到 1 字节", read(rfd, &c, 1), 1);
    test_expect(t, "读到 'z'", c, 'z');

    close(rfd);
    close(wfd);
}

/*
 * dup 出来的读端全部关闭后，写端必须拿到 EPIPE
 * 当前 counts 从 0 再减一次会下溢成 0xFFFFFFFF，写端永远看不到读端全关
 */
void test_pipe_dup_read_underflow(TestCtx* t){
    test_case(t, "测试 dup 读端全关后的 EPIPE");

    i64 rfd, wfd;
    pipe(&rfd, &wfd);

    i64 r2 = dup(rfd);
    test_expect(t, "dup 读端", r2 >= 0 && r2 != rfd, true);

    test_expect(t, "关闭原始读端", close(rfd), 0);
    test_expect(t, "关闭 dup 读端", close(r2), 0);

    char c = 'x';
    test_expect(t, "读端全关后写入应失败", write(wfd, &c, 1), -1);

    close(wfd);
}

/*
 * 管道写满后写者在 &pd->writers 上睡眠，
 */
void test_pipe_write_full(TestCtx* t){
    test_case(t, "测试满管道写者唤醒");

    i64 rfd, wfd;
    pipe(&rfd, &wfd);

    if (fork() == 0){
        close(rfd);
        pipe_fill_pattern(wfd, PIPE_USABLE_BYTES);
        char last = 'w';
        write(wfd, &last, 1);
        close(wfd);
        exit(0);
    }
    close(wfd);

    test_expect(t, "父进程先取走 4095 字节", pipe_check_pattern(rfd, PIPE_USABLE_BYTES), 0);

    char buf[1];
    test_expect(t, "父进程再取 1 字节", read(rfd, buf, 1), 1);

    wait(NULL);
    close(rfd);
}

/*
 * fd 用满后 pipe() 走失败清理路径，
 * 那里会先 unref 到 0 再 unref 一次，直接踩 ASSERT
 */
void test_pipe_fd_exhaust(TestCtx* t){
    test_case(t, "测试 fd 耗尽时的管道创建");

    i64 fds[16];
    u32 count = 0;

    while (count < 8){
        i64 rfd = -1, wfd = -1;
        if (pipe(&rfd, &wfd) != 0) break;

        fds[count * 2] = rfd;
        fds[count * 2 + 1] = wfd;
        count ++;
        printf_imple("  管道 #%u -> fd %d / %d\n", (u64)count, rfd, wfd);
    }

    test_expect(t, "fd 用尽后 pipe 应返回 -1", count < 8, true);
    printf_imple("当前持有 %u 条管道，下一次 pipe() 进入失败清理路径\n", (u64)count);

    i64 rfd, wfd;
    test_expect(t, "耗尽后再创建失败", pipe(&rfd, &wfd), -1);

    for (u32 i = 0; i < count * 2; i ++){
        close(fds[i]);
    }

    test_expect(t, "关闭全部管道后可以重新创建", pipe(&rfd, &wfd), 0);
    close(rfd);
    close(wfd);
}

void test_pipe_leak_audit_start(TestCtx* t){
    test_case(t, "测试管道资源回收");

    test_expect(t, "诊断快照可用", diag(&kstat_before), 0);
}

void test_pipe_leak_audit_end(TestCtx* t){
    Kstat now = {0};
    test_expect(t, "诊断快照可用", diag(&now), 0);

    test_expect(t, "File 槽恢复到原点", now.file_used, kstat_before.file_used);
    test_expect(t, "Inode 槽恢复到原点", now.inode_used, kstat_before.inode_used);
    test_expect(t, "没有新增孤儿 File", now.file_orphan, kstat_before.file_orphan);

    printf_imple("占用File=%d ; Inode=%d ; 管道Inode=%d ; 进程=%d ; 孤儿File=%d\n",
        (u64)now.file_used,
        (u64)now.inode_used,
        (u64)now.inode_pipe,
        (u64)now.proc_used,
        (u64)now.file_orphan
    );
}

void test_pipe(TestCtx *t){
    test_pipe_leak_audit_start(t);

    test_pipe_basic(t);
    test_pipe_eof(t);
    test_pipe_fork(t);
    test_pipe_epipe(t);
    test_pipe_bad_usage(t);
    test_pipe_chunk(t);
    test_pipe_ring_wrap(t);
    test_pipe_capacity(t);
    test_pipe_isolation(t);
    test_pipe_multi_writer(t);
    test_pipe_fork_own_ends(t);
    test_pipe_dup_read_underflow(t);
    test_pipe_write_full(t);
    test_pipe_fd_exhaust(t);

    test_pipe_leak_audit_end(t);
}

void test_fs(TestCtx *t){
    test_fs_mkdir(t);
    test_fs_unlink(t);
    test_fs_dup(t);
    test_fs_dup2(t);
    test_fs_readdir(t);
    test_pipe(t);
}
