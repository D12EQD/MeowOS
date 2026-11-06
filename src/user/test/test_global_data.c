#include "user/test/test.h"

/*
 * 用户镜像布局验证
 *
 * user.ld 把静态数据放在 [0x1000, 0x4000)，内核用 PTE_R | PTE_W 映射；
 * 代码放在 [0x4000, ...)，用 PTE_R | PTE_X 映射。
 * 如果起始地址或权限搞错，这里会直接 store page fault（scause=15），
 * 或者读到错位的内容。
 */
static u64 g_bss_counter;                       // .bss  期望全 0
static u64 g_data_inited = 0x1122334455667788;  // .data 期望保留初值
static u8  g_bss_buf[256];                      // .bss  期望全 0

void test_global_data_suite(TestCtx *t){
    test_case(t, "全局变量：.bss 零初始化、.data 保留初值");

    test_expect(t, ".bss 变量初值为 0", (i64)g_bss_counter, 0);
    test_expect(t, ".data 变量保留初值",
                (i64)g_data_inited, (i64)0x1122334455667788);

    // 写回读：验证数据页确实是 PTE_W 且落在正确地址
    g_bss_counter = 0x7eadbeef;
    test_expect(t, "写 .bss 后能读回", (i64)g_bss_counter, (i64)0x7eadbeef);

    u64 sum = 0;
    for (u64 i = 0; i < sizeof(g_bss_buf); i ++) sum += g_bss_buf[i];
    test_expect(t, "256 字节 .bss 缓冲区全为 0", (i64)sum, 0);

    for (u64 i = 0; i < sizeof(g_bss_buf); i ++) g_bss_buf[i] = (u8)i;
    test_expect(t, "写满 256 字节缓冲区后读回末字节", (i64)g_bss_buf[255], 255);

    // 大一点的写：确认数据页没有在页边界处断掉
    static u8 g_page_buf[2 * 4096];             // 跨 2 个数据页
    for (u64 i = 0; i < sizeof(g_page_buf); i ++) g_page_buf[i] = (u8)(i * 7);
    test_expect(t, "跨 2 页的静态缓冲区可写可读",
                (i64)g_page_buf[2 * 4096 - 1], (i64)(u8)((2 * 4096 - 1) * 7));
}
