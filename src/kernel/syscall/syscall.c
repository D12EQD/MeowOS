#include "kernel/arch/def.h"
#include "kernel/lib/def.h"
#include "kernel/fs/pipe.h"
#include "kernel/proc/process.h"

#include "kernel/fs/fs.h"
#include "kernel/fs/reg.h"

#include "kernel/syscall/syscall.h"

#include "kernel/mm/page.h"

static u64 get_arg(int n) {
    Process *p = current_process;
    switch (n) {
        case 0:
            return p->trap_frame_page->a0;
        case 1:
            return p->trap_frame_page->a1;
        case 2:
            return p->trap_frame_page->a2;
        case 3:
            return p->trap_frame_page->a3;
        case 4:
            return p->trap_frame_page->a4;
        case 5:
            return p->trap_frame_page->a5;
    }
    PANIC("get_arg");
    return -1;
}

u64 sys_fork(){
    return kfork("fork");
}

void syscall(){
    i64 num;
    Process *p = current_process;

    num = p->trap_frame_page->a7;
    if (num >= 0 && num < SYS_COUNT) {
        p->trap_frame_page->a0 = syscall_fn_list[num]();
    } else {
        p->trap_frame_page->a0 = -1;
    }

}

u64 sys_getpid(void){
    return current_process->pid;
}

u64 sys_mmap(void){
    return 0;
}

u64 sys_unmmap(void){
    return 0;
}

u64 sys_exit(void) {
    u64 return_val = get_arg(0);
    kexit(return_val);
    return 0;
}

u64 sys_wait(void) { 
    u64 address = get_arg(0);
    return kwait(address);
}

u64 sys_palloc(void){
    u64 page_count = get_arg(0);
    u64 res = kpalloc(page_count);
    return res;
}

u64 sys_pfree(void){
    u64 page_count = get_arg(0);
    kpfree(page_count); // bool
    return true;
}

u64 sys_write(void){
    i64 fd = (i64)get_arg(0);
    void *buf = (void *)get_arg(1);
    size_t len = get_arg(2);

    File *file = get_file_from_fd(fd);
    if unlikely(!file) return -1;

    return write_file(file, buf, len);
}

u64 sys_read(void){
    i64 fd = (i64)get_arg(0);
    void *buf = (void *)get_arg(1);
    size_t len = get_arg(2);

    File *file = get_file_from_fd(fd);
    if unlikely(!file) return -1;

    return read_file(file, buf, len);
}

u64 sys_open(void){
    const char *input_path = (const char *)get_arg(0);
    u32 flags = (u32)get_arg(1);

    critical_enter();

    char file_path[FILE_NAME_MAX];
    i64 len = copy_str_from_user(
        current_process->page_table, file_path,
        (void *)input_path, FILE_NAME_MAX
    );
    if (len <= 0) goto failed;

    int fd = process_find_empty_fd();
    if (is_invaild_fd(fd)) goto failed;

    // 未说明都是 reg 文件
    File *file = open_file(sv_from_cstr(file_path), flags, &reg_fop, INODE_REG);
    if (!file) goto failed;

    current_process->file_table[fd] = file;
    critical_exit();
    return fd;

failed:
    critical_exit();
    return -1;
}

/*
 *  sys_close 负责调用 release 接口 和清空当前文件打开表的项目
 *  不对 file 进行 unref 等操作
 */
u64 sys_close(void){
    i64 fd = (i64)get_arg(0);
    if (is_invaild_fd(fd)) return -1;

    critical_enter();
    File *file = current_process->file_table[fd];

    if (!file) goto failed;

    if (file->op && file->op->release) {
        file->op->release(file);
    }
    current_process->file_table[fd] = NULL;

    critical_exit();
    return 0;

failed:
    critical_exit();
    return -1;
}

u64 sys_unlink(void){
    const char *user_path = (const char *)get_arg(0);
    char path[FILE_NAME_MAX];

    i64 ret = copy_str_from_user(
        current_process->page_table, path,
        (void *)user_path, FILE_NAME_MAX
    );
    if (ret <= 0) return -1;

    return fs_unlink(sv_from_cstr(path)) ? 0 : -1;
}


u64 sys_mkdir(void){
    const char *user_path = (const char *)get_arg(0);
    char path[FILE_NAME_MAX];

    i64 ret = copy_str_from_user(
        current_process->page_table, path,
        (void *)user_path, FILE_NAME_MAX
    );
    if (ret <= 0) return -1;
 
    return fs_mkdir(sv_from_cstr(path)) ? 0 : -1;
}

u64 sys_dup(void){
    i64 fd = (i64)get_arg(0);
    critical_enter();

    File * file = get_file_from_fd(fd);
    if (!file) goto failed;

    i64 new_fd = process_find_empty_fd();
    if (new_fd < 0) goto failed;

    current_process->file_table[new_fd] = dup_file(file);
    
    critical_exit();
    return new_fd;
failed:
    critical_exit();
    return -1;
}

u64 sys_dup2(){
    i64 old_fd = (i64)get_arg(0);
    i64 new_fd = (i64)get_arg(1);

    critical_enter();

    File *old_file = get_file_from_fd(old_fd);
    if (!old_file) goto failed;

    // 相同 fd 情况什么都不做
    if (old_fd == new_fd){
        critical_exit();
        return new_fd;
    }

    File *new_file = get_file_from_fd(new_fd);

    // 如果new_fd被占用，强制关闭
    if (new_file){
        if (new_file->op->release) new_file->op->release(new_file);
    }

    new_file = dup_file(old_file);
        current_process->file_table[new_fd] = new_file;

    critical_exit();
    return new_fd;

failed:
    critical_exit();
    return -1;
}

u64 sys_readdir(){
    i64 fd = get_arg(0);
    void* ret = (void *)get_arg(1);

    critical_enter();
    File* f = get_file_from_fd(fd);

    if (!f) goto failed;
    if (!dir_readdir(f, ret)) goto not_found;

    critical_exit();
    return 1;

not_found:
    critical_exit();
    return 0;

failed:
    critical_exit();
    return -1;
}

u64 sys_pipe(){
    i64 *user_p1 = (i64 *)get_arg(0);
    i64 *user_p2 = (i64 *)get_arg(1);
    if (!user_p1 || !user_p2) goto failed;
 
    critical_enter();

    i64 p1, p2;
    if (!create_pipe_file(&p1, &p2)) goto failed;

    if (copy_to_user(current_process->page_table, user_p1, &p1, sizeof(i64)) < 0) goto failed;
    if (copy_to_user(current_process->page_table, user_p2, &p2, sizeof(i64)) < 0) goto failed;

    critical_exit();
    return 0;
failed:
    critical_exit();
    return -1;
}

u64 sys_lseek(){
    i64 fd = get_arg(0);
    i64 offset = get_arg(1);
    u64 flag = get_arg(2);

    File* file = get_file_from_fd(fd);
    if (!file) return -1;
    if (!file->op->lseek) return -1;

    return file->op->lseek(file, offset, flag);
}
