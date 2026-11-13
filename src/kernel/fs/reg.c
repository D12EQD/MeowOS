#include "file.h"
#include "reg.h"

#include "kernel/proc/process.h"
#include "kernel/mm/page.h"
#include "kernel/lib/def.h"

/*
 *  读取字节文件，不睡眠进程，如果无法完全读完，直接返回
 */
i64 reg_read(File *f, char *user_buf, size_t n) {
    ASSERT(f && user_buf);
    Inode * inode = f->inode;

    if (!(inode && inode->mode == INODE_REG)) return -1;
    if (n == 0) return 0;

    critical_enter();

    // inode->size - f->offset 有效字节数
    u64 cost = min(n, inode->size - f->offset);

    i64 real_cost = copy_to_user(
        current_process->page_table, 
        (void *)user_buf,
        (void *)inode->vaddr + f->offset, cost
    );
    if (real_cost < 0){
        goto failed;
    }

    f->offset += real_cost;

    critical_exit();
    return real_cost;
failed:
    critical_exit();
    return -1;
}

i64 reg_write(File *f, const char *user_buf, size_t n) {
    ASSERT(f && user_buf);
    Inode *inode = f->inode;

    if (!(inode && inode->mode == INODE_REG)) return -1;
    if (n == 0) return 0;

    critical_enter();
    if (f->offset >= inode->cap) goto failed;

    u64 out = min(n, inode->cap - f->offset);

    // offset 超过 size，中间空洞要显式填零
    if (f->offset > inode->size) {
        memset((u8 *)inode->vaddr + inode->size, 0, f->offset - inode->size);
    }

    i64 real_out = copy_from_user(
        current_process->page_table,
        (void *)((u64)inode->vaddr + f->offset),
        (void *)user_buf, out
    );

    if (real_out < 0) goto failed;

    f->offset += (u64)real_out;
    inode->size = max(inode->size, f->offset);

    critical_exit();
    return real_out;
failed:
    critical_exit();
    return -1;
}

/*
 *  移动文件指针
 */
i64 reg_lseek(File *f, i64 offset, int whence) {
    ASSERT(f);
    Inode *inode = f->inode;
    if (!(inode && inode->mode == INODE_REG)) return -1;

    i64 base;
    switch (whence) {
        case SEEK_SET: base = 0; break;
        case SEEK_CUR: base = (i64)f->offset; break;
        case SEEK_END: base = (i64)inode->size; break;
        default: return -1;
    }

    i64 target = base + offset;
    if (target < 0) return -1;
    if ((u64)target > inode->cap) return -1; /* 超出可写上限 */

    f->offset = (u64)target;
    return target;
}

int reg_release(File *f){
    ASSERT(f);
    unref_file(f);
    return 0;
}

FileOp reg_fop = {
    .lseek = reg_lseek,
    .release = reg_release,
    .read = reg_read,
    .write = reg_write
};

