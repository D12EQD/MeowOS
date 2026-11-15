#include "user/test/test.h"
#include "user/os/syscall.h"

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

void test_fs_pipe(TestCtx* t){
    return;
}

void test_fs(TestCtx *t){
    test_fs_mkdir(t);
    test_fs_unlink(t);
    test_fs_dup(t);
    test_fs_dup2(t);
    test_fs_readdir(t);
}
