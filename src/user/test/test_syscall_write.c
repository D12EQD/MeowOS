#include "user/test/test.h"

/*
 * 只读的填充块，用来构造"跨页缓冲区"。
 * 它是 const，落在 .rodata（只读页）里，不违反"用户态没有可写数据段"的限制。
 */
static const char cross_page_blob[4200] = { [0 ... 4199] = 'Z' };

/*
 * write 的回归测试
 *
 * 关注点：返回值、参数校验、跨页缓冲区。
 * 跨页是重点：内核必须按页拷贝，不能只翻译缓冲区首地址然后线性读。
 */
void test_write_suite(TestCtx *t) {
    char buf[64];

    /* ---------------- 返回值 ---------------- */
    test_case(t, "write: 返回值应为实际写入的字节数");
    memset(buf, 0, sizeof buf);
    strcpy(buf, "0123456789\n");
    test_expect(t, "write(1, \"0123456789\\n\", 11) == 11", (i64)write(1, buf, 11), 11);

    test_case(t, "write: count 为 0 时返回 0 且不输出");
    test_expect(t, "write(1, buf, 0) == 0", (i64)write(1, buf, 0), 0);
    test_expect(t, "write(1, NULL, 0) == 0", (i64)write(1, NULL, 0), 0);

    /* ---------------- 非法参数 ---------------- */
    test_case(t, "write: 非法 fd 返回 -1（不能 PANIC）");
    test_expect(t, "write(-1, buf, 4) == -1", (i64)write(-1, buf, 4), -1);
    test_expect(t, "write(0, buf, 4) == -1", (i64)write(0, buf, 4), -1);
    test_expect(t, "write(2, buf, 4) == -1", (i64)write(2, buf, 4), -1);

    test_case(t, "write: 非法用户指针返回 -1");
    test_expect(t, "write(1, NULL, 8) == -1", (i64)write(1, NULL, 8), -1);
    /* 0x2000 现在是静态数据页（可读写），换成仍未被映射的地址 */
    test_expect(t, "write(1, 0x3F000 未映射, 8) == -1", (i64)write(1, (void *)0x3F000, 8), -1);
    test_expect(t, "write(1, 0xffffffff80000000, 8) == -1",
        (i64)write(1, (void *)0xffffffff80000000UL, 8), -1);

    /* ---------------- 长度超过分块大小 ---------------- */
    test_case(t, "write: 长度超过内核分块大小（走多轮拷贝）");
    {
        char big[300];

        memset(big, 'A', sizeof big);
        big[sizeof big - 1] = '\n';
        test_expect(t, "write(1, 300 字节) == 300", (i64)write(1, big, 300), 300);
    }

    /* ---------------- 跨页缓冲区 ---------------- */
    /*
     * 把写入起点放在 .rodata 某页的倒数第 8 字节，
     * 于是前 8 字节在页 N，后 16 字节在页 N+1 —— 24 个字节全是 'Z'。
     * 逐页拷贝的实现会原样输出 24 个 Z；
     * 只翻译首地址的实现会在第 9 个字节开始读到物理页之外的垃圾。
     */
    test_case(t, "write: 跨页缓冲区必须逐页拷贝");
    {
        u64 addr = (u64)cross_page_blob;
        u64 off = (0x1000 - (addr & 0xfff)) - 8;   /* 页尾最后 8 字节 */
        const char *p = cross_page_blob + off;

        /* 起点必须离页尾正好 8 字节，否则这条用例没意义 */
        test_expect(t, "缓冲区起点跨在页边界前 8 字节", (i64)((u64)p & 0xfff), 0xff8);
        test_expect(t, "write(1, 跨页缓冲区, 24) == 24", (i64)write(1, (void *)p, 24), 24);
    }
}
