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
    Inode * in = create_inode(PAGE_SIZE, INODE_PIPE);

    if (!in) goto create_inode_failed;
    ref_inode(in);
    // ref = 1

    File* f1 = create_file(&pipe_fop, in, 0, O_READ);
    File* f2 = create_file(&pipe_fop, in, 0, O_WRITE);
    if (!f1 || !f2) goto create_file_failed;
    // ref = 3
    unref_inode(in);
    // ref = 2
 
    critical_enter();
    
    i64 new_fd1 = -1, new_fd2 = -1;

    new_fd1 = process_find_empty_fd();
    if (new_fd1 < 0) goto find_fd_failed;
    current_process->file_table[new_fd1] = f1;

    new_fd2 = process_find_empty_fd();
    if (new_fd2 < 0) goto find_fd_failed;
    current_process->file_table[new_fd2] = f2;

    *fd1 = new_fd1;
    *fd2 = new_fd2;

    // 申请空pipe data
    PipeData* data = get_pipe_data(in); 
    memset(in->private, 0, sizeof(PipeData));

    data->readers = 1;
    data->writers = 1;

    in->private = (void *)data;

    critical_exit();
    return true;
find_fd_failed:
    if (new_fd1 >= 0) current_process->file_table[new_fd1] = NULL;
    if (new_fd2 >= 0) current_process->file_table[new_fd2] = NULL;
create_file_failed:
    if (f1) unref_file(f1);
    if (f2) unref_file(f2);
    // ref = 1
create_inode_failed:
    if (in) unref_inode(in);
    // ref = 0
    critical_exit();
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
            sleep_prepare(in);
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
        wakeup(in);
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
            sleep_prepare(in);
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
            (char *)user_buf + sent,
            first
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

/*
 *  关闭一端：减计数，叫醒另一端
 */
i64 pipe_release(File *f){
    Inode *in = f->inode;
    if (!in || in->mode != INODE_PIPE) return -1;

    PipeData *pd = get_pipe_data(in);

    critical_enter();
    
    if (f->flags & O_READ)  pd->readers--;
    if (f->flags & O_WRITE) pd->writers--;
    if (f->flags & O_READ)  wakeup(&pd->writers);
    if (f->flags & O_WRITE) wakeup(&pd->readers);
    wakeup(in);

    if (pd->readers + pd->writers == 0){
        unref_file(f);
    }

    critical_exit();
    return 0;
}

bool free_pipe_inode(Inode *in){
    free_reg_inode(in);
    return true;
}

FileOp pipe_fop = {
    .read = pipe_read,
    .write = pipe_write,
    .release = pipe_release,
    .lseek = NULL // 应该为NULL？
};
