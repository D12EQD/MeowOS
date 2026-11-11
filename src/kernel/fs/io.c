#include "kernel/fs/file.h"
#include "kernel/mm/page.h"
#include "kernel/proc/process.h"
#include "kernel/arch/sbi.h"
#include "kernel/lib/printk.h"

#define KERNEL_IO_BUFFER_SIZE 256

/*
 *
 */
i64 io_read(File *, char *buf, size_t n){
    // char kbuf[KERNEL_IO_BUFFER_SIZE];
    //
    // u64 i;
    // for (i = 0; i < min(KERNEL_IO_BUFFER_SIZE, n); i ++){
    //     auto res = sbi_console_getchar();
    //
    //     if (res.error >= 0){
    //         kbuf[i]= (char)res.error;
    //     }else{
    //         // TODO : 
    //         sleep(); 
    //     }
    //
    // }
    //
    // copy_to_user(current_process->page_table, (u64)buf, kbuf, i);

    return -1;
}

/*
 *  io_write 向终端输出
 */
i64 io_write(File *file, const char *buf, size_t n){
    // 一个一个页面copy
    critical_enter();

    char* kbuf = (char *)( file->inode->vaddr );
    u64 send = 0;

    while (send < n){
        u64 chunk = min(file->inode->cap, n - send); // 每次输出全部清空，所以为容积
        i64 cost = copy_from_user(current_process->page_table, kbuf, (void *)((u64)buf + send), chunk);

        if (cost < 0){
            critical_exit();
            return -1;
        }

        if (cost == 0) break;

        puts_n(kbuf, cost);
        send += (u64)cost;
    }

    critical_exit();
    return n;
}

i64 io_lseek(File *f, i64 offset, int whence){

}

int io_release(File *f){

}


FileOp io_fop = {
    .read = io_read,
    .write = io_write,
    .lseek = io_lseek,
    .release = io_release
};


