#include "kernel/mm/pmm.h"
#include "kernel/proc/process.h"

#include "pipe.h"
#include "file.h"


/*  
 *  环形状缓冲区，默认为Inode区域私人数据
 *  Inode 的 size 没有用，由 start end 接管处理
 *  if start == end     : 空
 *  if end + 1 == start : 满
 */
typedef struct{
    u64 start;
    u64 end;
    u32 readers;    /* 读端引用计数 */
    u32 writers;    /* 写端引用计数 */
}PipeData;

PipeData pipe_data_array[FS_MAX_INODE_COUNT];

PipeData* get_pipe_data(Inode *in){
    return &pipe_data_array[in->ino];
}

/*
 *  创建单向管道
 *  returns:
 *      fd1 读口
 *      fd2 写口
 *      bool
 */
bool create_pipe_file(i64* fd1, i64* fd2){
    if (!fd1 || !fd2) return false;

    Inode *in = create_inode(PAGE_SIZE, INODE_PIPE);
    if (!in) return false;

    ref_inode(in);

    File *f1 = create_file(&pipe_fop, in, 0, O_READ);
    if (!f1) goto unref_inode_failed;

    File *f2 = create_file(&pipe_fop, in, 0, O_WRITE);
    if (!f2) goto unref_f1_failed;

    /* 两个 File 各持一份 inode 引用，临时引用可以还掉，此时 ref == 2 */
    unref_inode(in);

    /* PipeData 在 fd 可见之前初始化，读者写者拿到的就是干净的计数 */
    PipeData *data = get_pipe_data(in);
    memset(data, 0, sizeof(PipeData));
    data->readers = 1;
    data->writers = 1;
    in->private = (void *)data;

    i64 new_fd1 = -1;
    i64 new_fd2 = -1;

    critical_enter();

    new_fd1 = process_find_empty_fd();
    if (new_fd1 < 0) goto no_fd_failed;
    current_process->file_table[new_fd1] = f1;

    new_fd2 = process_find_empty_fd();
    if (new_fd2 < 0) goto one_fd_failed;
    current_process->file_table[new_fd2] = f2;

    *fd1 = new_fd1;
    *fd2 = new_fd2;

    critical_exit();
    return true;

one_fd_failed:
    current_process->file_table[new_fd1] = NULL;
no_fd_failed:
    critical_exit();
    unref_file(f2);
    unref_file(f1);
    return false;

unref_f1_failed:
    unref_file(f1);

unref_inode_failed:
    unref_inode(in);
    return false;
}


/*
 * 从管道读
 * returns:
 *   > 0  实际读到的字节数
 *   0    EOF（writers == 0 且缓冲空）
 *   -1   失败
 */
i64 pipe_read(File *f, char *user_buf, size_t n){
    if (!f) return -1;
    Inode *in = f->inode;
    if (!in || in->mode != INODE_PIPE) return -1;
    if (!(f->flags & O_READ)) return -1;
    if (n == 0) return 0;

    PipeData *pd = get_pipe_data(in);
    u64 cap = in->cap;
    u64 got = 0;

    while (got < n) {
        critical_enter();

        while (pd->start == pd->end) {
            if (pd->writers == 0) {
                critical_exit();
                return (i64)got;
            }
            sleep_prepare(&pd->readers);
            sleep();
        }

        u64 avail = (pd->end - pd->start + cap) % cap;
        u64 want = n - got;
        if (want > avail) want = avail;

        u64 first = cap - pd->start;
        if (first > want) first = want;

        i64 r = copy_to_user(
            current_process->page_table,
            (char *)user_buf + got,
            (char *)in->vaddr + pd->start,
            first
        );

        if (r != (i64)first) {
            critical_exit();
            return -1;
        }

        if (want > first) {
            r = copy_to_user(
                current_process->page_table,
                (char *)user_buf + got + first,
                (char *)in->vaddr,
                want - first
            );

            if (r != (i64)(want - first)) {
                critical_exit();
                return -1;
            }
        }

        pd->start = (pd->start + want) % cap;
        wakeup(&pd->writers);
        critical_exit();

        got += want;
    }
    return (i64)got;
}

/*
 * 向管道写
 * returns:
 *   >= 0  实际写入的字节数
 *   -1    失败（读端全关且没写出任何字节时）
 */
i64 pipe_write(File *f, const char *user_buf, size_t n){
    Inode *in = f->inode;
    if (!in || in->mode != INODE_PIPE) return -1;
    if (!(f->flags & O_WRITE)) return -1;
    if (n == 0) return 0;

    PipeData *pd = get_pipe_data(in);
    u64 cap = in->cap;
    u64 sent = 0;
    u64 ret = 0;

    while (sent < n) {
        critical_enter();

        /* 读端全关 */
        if (pd->readers == 0) {
            ret = sent > 0 ? (i64)sent : -1;
            goto failed;
        }

        u64 used = (pd->end - pd->start + cap) % cap;
        while (used >= cap - 1) {
            sleep_prepare(&pd->writers);
            sleep();
            if (pd->readers == 0) {
                ret = sent > 0 ? (i64)sent : -1;
                goto failed;
            }
            used = (pd->end - pd->start + cap) % cap;
        }

        u64 free = cap - 1 - used;
        u64 want = n - sent;
        if (want > free) want = free;

        u64 first = cap - pd->end;
        if (first > want) first = want;

        i64 r = copy_from_user(current_process->page_table,
            (char *)in->vaddr + pd->end,
            (char *)user_buf + sent, first
        );

        if (r != (i64)first){
            ret = -1;
            goto failed;
        }

        if (want > first) {
            r = copy_from_user(current_process->page_table,
                (char *)in->vaddr,
                (char *)user_buf + sent + first,
                want - first
            );
            if (r != (i64)(want - first)){
                ret = -1;
                goto failed;
            }
        }

        pd->end = (pd->end + want) % cap;
        wakeup(&pd->readers);
        critical_exit();

        sent += want;
    }
    return (i64)sent;

failed:
    critical_exit();
    return ret;
}

/* 关闭一端：减计数，叫醒另一端 */
i64 pipe_release(File *f){
    Inode *in = f->inode;
    if (!in || in->mode != INODE_PIPE) return -1;

    PipeData *pd = get_pipe_data(in);

    critical_enter();
    
    if (f->flags & O_READ){
        pd->readers--;
        wakeup(&pd->writers);
    }

    if (f->flags & O_WRITE){
        pd->writers--;
        wakeup(&pd->readers);
    }

    unref_file(f);

    critical_exit();
    return 0;
}

bool free_pipe_inode(Inode *in){
    free_reg_inode(in);
    return true;
}

i64 pipe_dup(File *f){
    critical_enter();
    f->ref ++;
    if (!f->inode) goto failed;

    PipeData* pd = (PipeData*)f->inode->private;
    if (!pd) goto failed;

    if (f->flags & O_READ)      pd->readers ++;
    if (f->flags & O_WRITE)     pd->writers ++;
    critical_exit();
    return 0;
failed:
    critical_exit();
    return -1;
}

FileOp pipe_fop = {
    .read       = pipe_read,
    .write      = pipe_write,
    .release    = pipe_release,
    .lseek      = NULL, // 应该为NULL？
    .dup        = pipe_dup
};
