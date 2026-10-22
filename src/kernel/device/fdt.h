#pragma once
#include "kernel/start.h"

#include "kernel/lib/def.h"
#include "kernel/lib/typedef.h"

/* 句柄: blob 内字节偏移 */
typedef i32 fdt_off_t;
/* 头部: 直接映射 DTB 起始 40 字节, 字段为大端序 */
typedef struct __attribute__((packed)) {
    u32 magic;
    u32 totalsize;
    u32 off_dt_struct;
    u32 off_dt_strings;
    u32 off_mem_rsvmap;
    u32 version;
    u32 last_comp_version;
    u32 boot_cpuid_phys;
    u32 size_dt_strings;
    u32 size_dt_struct;
} FdtHeader;

/* 零拷贝视图: 头部字段已转换为宿主序, blob 保持原始大端 */
typedef struct {
    const u8 *blob;
    u32 total_size;
    u32 off_struct;
    u32 size_struct;
    u32 off_strings;
    u32 size_strings;
    u32 off_rsvmap;
} FdtTree;

void kernel_fdt_init(void *fdt_address, BootInfo * boot_info);
