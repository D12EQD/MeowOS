#include "kernel/fs/file.h"
#include "kernel/mm/page.h"
#include "kernel/proc/process.h"
#include "kernel/arch/sbi.h"
#include "kernel/lib/printk.h"

/* 
 *  io_read 从终端读入 
 *  return:
 *      >= 0 实际读到的字节数（按行缓冲，短读是正常现象）
 *      < 0 failed
 *
 *  注意：console_read 会 sleep 等一整行输入，这里绝不能关中断，
 *  否则中断进不来、没人能 wakeup 本进程。
 */
i64 io_read(File *file, char *buf, size_t n){
    pr_debug("io_read");
    char* kbuf = (char *)( file->inode->vaddr );

    u64 recv_total = 0;

    while (recv_total < n){
        u64 chunk = min(file->inode->cap, n - recv_total);

        i64 recv = console_read(kbuf, (int)chunk);
        if (recv < 0) return -1;
        if (recv == 0) break;

        // 只把真正读到的字节交给用户；按 chunk 拷会把 kbuf 里的旧数据带出去
        i64 cost = copy_to_user(
            current_process->page_table, 
            (void *)((u64)buf + recv_total), 
            kbuf, 
            (u64)recv
        );
        if (cost < 0) return -1;

        recv_total += (u64)recv;

        // 短读说明这次请求被一行输入截断了，按字节流语义直接返回
        if ((u64)recv < chunk) break;
    }

    return (i64)recv_total;
}

/* 
 *  io_write 向终端输出 
 *  returns : 同 io_read
 *
 *  注意：console_write 在 uart 发送寄存器满时会 sleep，
 *  这里同样不能把整个循环包在关中断的临界区里。
 */
i64 io_write(File *file, const char *buf, size_t n){
    // 一个一个页面copy
    char* kbuf = (char *)( file->inode->vaddr );
    u64 send = 0;

    while (send < n){
        u64 chunk = min(file->inode->cap, n - send); // 每次输出全部清空，所以为容积
        i64 cost = copy_from_user(current_process->page_table, kbuf, (void *)((u64)buf + send), chunk);

        if (cost < 0) return -1;
        if (cost == 0) break;

        i64 cost2 = console_write(kbuf, (int)cost);
        if (cost2 != cost) return -1;
        send += (u64)cost;
    }

    return (i64)send;
}

/* io file 没有 lseek 和 release */
FileOp io_fop = {
    .read = io_read,
    .write = io_write,
    .lseek = NULL,
    .release = NULL
};


