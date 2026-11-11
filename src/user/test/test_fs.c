#include "user/test/test.h"
#include "user/os/syscall.h"
#include "user/os/file.h"

const int size = 128;
int buffer[128];

void test_fs(TestCtx *t){
    test_case(t, "测试文件系统基本读写能力");

    test_expect(t, "open a file but only read", open("/new.file", O_READ), -1);
    test_expect(t, "open a file but only write", open("/new.file", O_WRITE), -1);

    int fd = open("/new.file", O_READ | O_CREATE);
    test_expect(t, "open a file 并且开启了 create", fd >= 0, true);

    int fd2 = open("/new.file", O_READ | O_WRITE);
    test_expect(t, "打开已经存在的文件", fd2 >= 0, true);
    test_expect(t, "确认fd", fd == 2 && fd2 == 3, true);

    char output = 'a';
    int res = write(fd, &output, 1);

    test_expect(t, "检测输出", res == sizeof(output), true);
}
