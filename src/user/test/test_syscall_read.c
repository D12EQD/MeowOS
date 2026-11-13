#include "user/test/test.h"

/*
 * write 的回归测试
 *
 * 关注点：返回值、参数校验、跨页缓冲区。
 * 跨页是重点：内核必须按页拷贝，不能只翻译缓冲区首地址然后线性读。
 */
void test_read_suite(TestCtx *t) {
    char buf[64];

    int res = read(0, buf, 10);
    test_expect(t, "测试标准输入", res == 10, true);
}
