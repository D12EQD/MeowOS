#include "kernel/lib/typedef.h"
#include "kernel/lib/printk.h"
#include "kernel/lib/string.h"
#include "kernel/device/fdt.h"

/* ============================================================
 * FDT 零拷贝只读视图
 *
 * - FdtTree 仅保存 blob 指针 + 头部派生偏移，不建树、不分配
 * - 句柄 = 结构块内字节偏移 (指向 token 本身)
 * - 属性名/值均零拷贝指向 blob，调用者不得修改 DTB 内存
 * ============================================================ */

/* FDT token */
#define FDT_MAGIC       0xd00dfeedU
#define FDT_BEGIN_NODE  0x00000001U
#define FDT_END_NODE    0x00000002U
#define FDT_PROP        0x00000003U
#define FDT_NOP         0x00000004U
#define FDT_END         0x00000009U

/* 错误码 */
#define FDT_OK          0
#define FDT_E_MAGIC    (-1)
#define FDT_E_BOUNDS   (-2)
#define FDT_E_VERSION  (-3)
#define FDT_E_FORMAT   (-4)

#define FDT_OFF_NONE ((fdt_off_t)-1)

static inline u32 fdt_be32(const u8 *p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

static inline u64 fdt_be64(const u8 *p) {
    return ((u64)fdt_be32(p) << 32) | fdt_be32(p + 4);
}

/* 字符串相等比较（不依赖 libc） */
int fdt_streq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

/* 校验 DTB 头部并初始化零拷贝视图。成功返回 FDT_OK。 */
int fdt_init(const void *blob, u32 size, FdtTree *out) {
    if (!blob || !out || size < sizeof(FdtHeader)) return FDT_E_BOUNDS;

    const u8 *b = (const u8 *)blob;
    if (fdt_be32(b) != FDT_MAGIC) return FDT_E_MAGIC;

    u32 total = fdt_be32(b + 4);
    u32 os    = fdt_be32(b + 8);
    u32 ostr  = fdt_be32(b + 12);
    u32 orsv  = fdt_be32(b + 16);
    u32 ver   = fdt_be32(b + 20);
    u32 lcv   = fdt_be32(b + 24);
    u32 sstr  = fdt_be32(b + 32);
    u32 ss    = fdt_be32(b + 36);

    if (total < sizeof(FdtHeader) || total > size) return FDT_E_BOUNDS;
    if (ver < 16 || lcv > 17) return FDT_E_VERSION;

    /* 用 64 位求和，防止 u32 溢出 */
    if ((u64)os + ss > total || (u64)ostr + sstr > total ||
        (u64)orsv + 16 > total || ss < 4 || sstr < 1)
        return FDT_E_BOUNDS;

    if (fdt_be32(b + os) != FDT_BEGIN_NODE) return FDT_E_FORMAT;

    out->blob         = b;
    out->total_size   = total;
    out->off_struct   = os;
    out->size_struct  = ss;
    out->off_strings  = ostr;
    out->size_strings = sstr;
    out->off_rsvmap   = orsv;
    return FDT_OK;
}

/* 节点名长度（不含 '\0'）；-1 表示越界或未找到 '\0' */
static i32 fdt_name_len(const FdtTree *t, u32 noff) {
    u32 end = t->off_struct + t->size_struct;
    if (noff >= end) return -1;
    const u8 *p = t->blob + noff;
    u32 maxlen = end - noff, n = 0;
    while (n < maxlen && p[n]) n++;
    return (n < maxlen) ? (i32)n : -1;
}

/* DFS 前序迭代：cur 传 FDT_OFF_NONE 从头开始，否则传上次返回值。
 * *depth 调用者需保留跨调用状态（root 为 0）。 */
fdt_off_t fdt_next_node(const FdtTree *t, fdt_off_t cur, int *depth) {
    if (!t) return FDT_OFF_NONE;
    const u32 end = t->off_struct + t->size_struct;
    u32 off;
    int d;

    if (cur == FDT_OFF_NONE) {
        off = t->off_struct;
        d = -1;                     /* 首次遇到 BEGIN_NODE 后变为 0 */
    } else {
        u32 noff = (u32)cur + 4;
        if (noff >= end) return FDT_OFF_NONE;
        i32 nlen = fdt_name_len(t, noff);
        if (nlen < 0) return FDT_OFF_NONE;
        off = noff + (((u32)nlen + 4) & ~3U);   /* align_up(nlen+1, 4) */
        d = depth ? *depth : 0;
    }

    while ((u64)off + 4 <= end) {
        u32 tok = fdt_be32(t->blob + off);
        switch (tok) {
        case FDT_BEGIN_NODE:
            if (depth) *depth = ++d;
            return (fdt_off_t)off;
        case FDT_END_NODE:
            d--;
            off += 4;
            break;
        case FDT_PROP: {
            if ((u64)off + 12 > end) return FDT_OFF_NONE;
            u32 plen = fdt_be32(t->blob + off + 4);
            u64 nxt = (u64)off + 12 + (((u64)plen + 3) & ~3ULL);
            if (nxt > end) return FDT_OFF_NONE;
            off = (u32)nxt;
            break;
        }
        case FDT_NOP:
            off += 4;
            break;
        case FDT_END:
        default:
            return FDT_OFF_NONE;
        }
    }
    return FDT_OFF_NONE;
}

/* 节点名（零拷贝，指向 blob）。root 节点返回 ""，非 NULL。 */
const char *fdt_node_name(const FdtTree *t, fdt_off_t node) {
    if (!t || node == FDT_OFF_NONE) return NULL;
    u32 noff = (u32)node + 4;
    if (fdt_name_len(t, noff) < 0) return NULL;
    return (const char *)(t->blob + noff);
}

/* 节点第一个属性偏移；无属性或无效返回 FDT_OFF_NONE */
fdt_off_t fdt_first_prop(const FdtTree *t, fdt_off_t node) {
    if (!t || node == FDT_OFF_NONE) return FDT_OFF_NONE;
    u32 end = t->off_struct + t->size_struct;
    u32 noff = (u32)node + 4;
    if (noff >= end) return FDT_OFF_NONE;

    i32 nlen = fdt_name_len(t, noff);
    if (nlen < 0) return FDT_OFF_NONE;

    u32 off = noff + (((u32)nlen + 4) & ~3U);
    if ((u64)off + 4 > end) return FDT_OFF_NONE;
    return (fdt_be32(t->blob + off) == FDT_PROP) ? (fdt_off_t)off : FDT_OFF_NONE;
}

/* 同一节点内下一个属性偏移；已无属性返回 FDT_OFF_NONE */
fdt_off_t fdt_next_prop(const FdtTree *t, fdt_off_t prop) {
    if (!t || prop == FDT_OFF_NONE) return FDT_OFF_NONE;
    u32 end = t->off_struct + t->size_struct;
    u32 off = (u32)prop;
    if ((u64)off + 12 > end) return FDT_OFF_NONE;

    u32 plen = fdt_be32(t->blob + off + 4);
    u64 nxt = (u64)off + 12 + (((u64)plen + 3) & ~3ULL);
    if (nxt + 4 > end) return FDT_OFF_NONE;
    return (fdt_be32(t->blob + nxt) == FDT_PROP) ? (fdt_off_t)nxt : FDT_OFF_NONE;
}

/* 属性名（零拷贝，指向字符串块） */
const char *fdt_prop_name(const FdtTree *t, fdt_off_t prop) {
    if (!t || prop == FDT_OFF_NONE) return NULL;
    u32 end = t->off_struct + t->size_struct;
    u32 off = (u32)prop;
    if ((u64)off + 12 > end) return NULL;

    u32 nameoff = fdt_be32(t->blob + off + 8);
    if (nameoff >= t->size_strings) return NULL;
    return (const char *)(t->blob + t->off_strings + nameoff);
}

/* 属性值指针（零拷贝，原始大端字节） */
const u8 *fdt_prop_value(const FdtTree *t, fdt_off_t prop) {
    if (!t || prop == FDT_OFF_NONE) return NULL;
    u32 end = t->off_struct + t->size_struct;
    u32 off = (u32)prop;
    if ((u64)off + 12 > end) return NULL;

    u32 plen = fdt_be32(t->blob + off + 4);
    if ((u64)off + 12 + plen > end) return NULL;
    return t->blob + off + 12;
}

/* 属性值长度；0 表示无效 */
u32 fdt_prop_len(const FdtTree *t, fdt_off_t prop) {
    if (!t || prop == FDT_OFF_NONE) return 0;
    u32 end = t->off_struct + t->size_struct;
    u32 off = (u32)prop;
    if ((u64)off + 12 > end) return 0;

    u32 plen = fdt_be32(t->blob + off + 4);
    return ((u64)off + 12 + plen <= end) ? plen : 0;
}

/* 按名字查找节点内的属性（线性扫描） */
fdt_off_t fdt_prop_by_name(const FdtTree *t, fdt_off_t node, const char *name) {
    for (fdt_off_t p = fdt_first_prop(t, node); p != FDT_OFF_NONE; p = fdt_next_prop(t, p))
        if (fdt_streq(fdt_prop_name(t, p), name)) return p;
    return FDT_OFF_NONE;
}

/* 按名字查找父节点的直接子节点 */
fdt_off_t fdt_child_by_name(const FdtTree *t, fdt_off_t parent, const char *name) {
    int depth = 0;
    fdt_off_t cur = FDT_OFF_NONE;
    int pdepth = -1;

    /* 第一遍：找 parent 的深度 */
    while ((cur = fdt_next_node(t, cur, &depth)) != FDT_OFF_NONE)
        if (cur == parent) { pdepth = depth; break; }
    if (pdepth < 0) return FDT_OFF_NONE;

    /* 第二遍：只匹配深度为 pdepth+1 的节点 */
    while ((cur = fdt_next_node(t, cur, &depth)) != FDT_OFF_NONE) {
        if (depth <= pdepth) break;
        if (depth == pdepth + 1 && fdt_streq(fdt_node_name(t, cur), name))
            return cur;
    }
    return FDT_OFF_NONE;
}

/* 读取 u32 属性（要求长度恰为 4），失败返回 def */
u32 fdt_prop_u32(const FdtTree *t, fdt_off_t prop, u32 def) {
    const u8 *v = fdt_prop_value(t, prop);
    return (v && fdt_prop_len(t, prop) == 4) ? fdt_be32(v) : def;
}

/* 读取 u64 属性（要求长度恰为 8），失败返回 def */
u64 fdt_prop_u64(const FdtTree *t, fdt_off_t prop, u64 def) {
    const u8 *v = fdt_prop_value(t, prop);
    return (v && fdt_prop_len(t, prop) == 8) ? fdt_be64(v) : def;
}

/* 属性值是否为单个 NUL 结尾的可打印字符串 */
static inline int fdt_val_is_string(const u8 *v, u32 len) {
    if (!len || v[len - 1] != 0) return 0;
    for (u32 i = 0; i < len - 1; i++)
        if (v[i] && (v[i] < 0x20 || v[i] > 0x7e)) return 0;
    return 1;
}

/* 取字符串属性，含 NULL 拷贝到 buf */
static inline bool fdt_prop_copy_str(
    const FdtTree *t, 
    fdt_off_t node, 
    const char *prop_name, 
    char *buf, 
    u32 buf_size
){
    fdt_off_t p = fdt_prop_by_name(t, node, prop_name);
    if (p == FDT_OFF_NONE) return false;

    const u8 *v = fdt_prop_value(t, p);
    u32 len = fdt_prop_len(t, p);
    if (!v || !len || !fdt_val_is_string(v, len)) return true;

    u32 n = (len < buf_size) ? len : buf_size - 1;
    memcpy(buf, v, n); /* 字符串就是字节流，大小端无关 */
    buf[n] = '\0';
    return false;
}

/* 取 u32 属性 */
static inline bool fdt_prop_get_u32(
    const FdtTree *t, 
    fdt_off_t node, 
    const char *prop_name, 
    u32 *out
){
    fdt_off_t p = fdt_prop_by_name(t, node, prop_name);
    if (p == FDT_OFF_NONE) return false;

    const u8 *v = fdt_prop_value(t, p);
    if (!v || fdt_prop_len(t, p) != 4) return false;

    *out = fdt_be32(v);
    return true;
}

/* 取 u64 属性 */
static inline bool fdt_prop_get_u64(
    const FdtTree *t, 
    fdt_off_t node, 
    const char *prop_name, 
    u64 *out
){
    fdt_off_t p = fdt_prop_by_name(t, node, prop_name);
    if (p == FDT_OFF_NONE) return false;

    const u8 *v = fdt_prop_value(t, p);
    if (!v || fdt_prop_len(t, p) != 8) return false;

    *out = fdt_be64(v);
    return true;
}

void kernel_fdt_init(void *fdt_address, BootInfo *boot_info) {
    FdtTree tree;
    u32 total = fdt_be32((u8 *)fdt_address + 4);
    ASSERT(fdt_init(fdt_address, total, &tree) == FDT_OK);

    int depth = 0;
    fdt_off_t n = fdt_next_node(&tree, FDT_OFF_NONE, &depth);
    while (n != FDT_OFF_NONE) {
        const char *name = fdt_node_name(&tree, n);

        if (fdt_streq(name, "cpus")) {
            if (fdt_prop_get_u32(&tree, n, "timebase-frequency", &(boot_info->timebase_frequency))){
                pr_info("freq is %u", boot_info->timebase_frequency);
            }
        }

        n = fdt_next_node(&tree, n, &depth);
    }
}
