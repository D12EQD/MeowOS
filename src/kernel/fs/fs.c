#include "file.h"
#include "fs.h"
#include "path.h"
#include "kernel/fs/io.h"

#include "kernel/proc/process.h"
#include "kernel/mm/def.h"
#include "kernel/lib/printk.h"
#include "kernel/proc/lock.h"

FileOp dir_fop = {
    .lseek = NULL,
    .read = NULL,
    .release = NULL, // TODO : 释放空间
    .readdir = dir_readdir,
    .write = NULL
};

i64 fs_test_read (File *, char *, size_t n){
    pr_debug("read");
    return n;
}

i64 fs_test_write (File *, const char *, size_t n){
    pr_debug("write");
    return n;
}

i64 fs_test_lseek (File *, i64 , int ){
    pr_debug("lseek");
    return 0;
}

i64 fs_test_readdir (File *, DirEntry *){
    pr_debug("readdir");
    return 0;
}

int fs_test_release (File *){
    pr_debug("release");
    return 0;
}

FileOp test_fop = {
    .lseek = fs_test_lseek,
    .read = fs_test_read,
    .release = fs_test_release,
    .readdir = fs_test_readdir,
    .write = fs_test_write
};


void fs_display(void) {
    critical_enter();
    pr_debug("/");
    if (root_inode && root_inode->mode == INODE_DIR) {
        fs_dfs(root_inode, "", 0);
    }
    critical_exit();
}

void fs_dfs(Inode *dir, const char *prefix, int depth) {
    if (!dir || dir->mode != INODE_DIR) return;
    if (depth >= 16) return;

    u64 n = dir->cap / sizeof(DirEntry);
    DirEntry *d = (DirEntry *)dir->vaddr;

    // 有效条目
    u64 valid = 0;
    for (u64 i = 0; i < n; i ++) {
        if (d[i].inode) valid ++;
    }

    u64 seen = 0;
    for (u64 i = 0; i < n; i ++) {
        if (!d[i].inode) continue;
        seen ++;
        bool is_last = (seen == valid);

        pr_cont_debug("%s%s", prefix, is_last ? "└── " : "├── ");

        if (d[i].inode->mode == INODE_DIR) {
            pr_info("%s/", d[i].name);

            char child[16 * 8];
            u64 plen = 0;
            while (prefix[plen]) { child[plen] = prefix[plen]; plen ++; }

            const char *seg = is_last ? "    " : "│   ";
            u64 k = 0;
            while (seg[k]) { child[plen + k] = seg[k]; k ++; }
            child[plen + k] = 0;

            fs_dfs(d[i].inode, child, depth + 1);
        } else {
            pr_info("%s", d[i].name);
        }
    }
}

void fs_init(){
    memset(inode_table, 0, sizeof(Inode) * FS_MAX_INODE_COUNT);
    memset(file_table, 0, sizeof(File) * FS_MAX_FILE_COUNT);

    for (u64 i = 0; i < FS_MAX_INODE_COUNT; i ++){
        inode_table[i].ino = i;
    }

    root_inode = create_inode(PAGE_SIZE, INODE_DIR);
    root_file = create_file(&dir_fop, root_inode, 0, O_RW);

    if (fs_mkdir(sv_lit(IO_DIR)) == false){
        PANIC("create io dir = /dev failed\n");
    }
}


/* 创建目录 */
bool fs_mkdir(StringView path){
    critical_enter();
    Inode* fa_inode;
    Inode* inode;
    StringView fa_path; 
    StringView fname;
    bool ret;
 
    // 只需要在这检查fname
    ret = path_get_filename(path, &fname);
    if (!ret) goto failed;

    // insert
    ret = (lookup_parent(path, &fa_inode, &fa_path));
    if (!ret) goto failed;

    inode = create_ref_inode(PAGE_SIZE, INODE_DIR); // ref = 1
    if (!inode) goto failed;
 
    ret = insert_dir_entry(fa_inode, inode, fname); // ref = 2
    if (!ret) goto free_new_inode;

    unref_inode(inode); // ref = 1

    critical_exit();
    return true;
free_new_inode:
    unref_inode(inode); // ref = 0, free this
failed:
    critical_exit();
    return false;
}

/* 删除 Inode 文件 */
bool fs_unlink(StringView path){
    critical_enter();
    bool ret;
    Inode* fa_inode;
    StringView fa_path;
    StringView fname;

    ret = path_get_filename(path, &fname);
    if (!ret) goto failed;
 
    // 内部会check路径合法性
    ret = lookup_parent(path, &fa_inode, &fa_path);
    if (!ret) goto failed;

    DirEntry* e = lookup_dir_entry(fa_inode, fname);
    if (!e) goto failed;

    clean_dir_entry(fa_inode, e);
    critical_exit();
    return true;
failed:
    critical_exit();
    return false;
}

/*
 *  删除空目录
 */
bool fs_rmdir(StringView path){
    critical_enter();
    bool ret;
    Inode * inode;
    Inode* fa_inode;
    StringView fa_path;
    StringView fname;

    ret = path_get_filename(path, &fname);
    if (!ret) goto failed;

    ret = lookup_parent(path, &fa_inode, &fa_path);
    if (!ret) goto failed;

    DirEntry* e = lookup_dir_entry(fa_inode, fname);
    if (!e) goto failed;
    inode = e->inode;

    // check size and this file should be a inode_dir
    if (inode->size != 0 || inode->mode != INODE_DIR){
        goto failed;
    }
 
    clean_dir_entry(fa_inode, e);
    critical_exit();
    return true;
failed:
    critical_exit();
    return false;
}

/* 关闭进程打开的文件描述符 */
bool fs_close(int fd){
    critical_enter();
    if (fd >= PROCESS_MAX_FD_COUNT) goto failed;

    File *f = current_process->file_table[fd];
    if (!f) goto failed;
    f->op->release(f);
    unref_file(f);
    current_process->file_table[fd] = NULL;

    critical_exit();
    return true;
failed:
    critical_exit();
    return false;
}

/* ref_file + 返回同一指针 */
File *dup_file(File *f){
    critical_enter();
    ref_file(f);
    critical_exit();
    return f;
}


/* 
 * 内核的文件系统实现的系统调用接口
 * 注，内核的kwrite kread kopen 所有指针都来自用户，需要转换和判断
 */
i64 kwrite(i64 fd, void *buf, size_t len){
    if (fd < 0) return -1;
    if (fd >= PROCESS_MAX_FD_COUNT) return -1;

    File* file = current_process->file_table[fd];

    if (!file) return -1;

    i64 res = file_write(file, buf, len);
    return res;
}

i64 kread(i64 fd, void *buf, size_t len){
    if (fd < 0) return -1;
    if (fd >= PROCESS_MAX_FD_COUNT) return -1;

    File* file = current_process->file_table[fd];
    if (!file) return -1;

    i64 res = file_read(file, buf, len);
    return res;
}

i64 kopen(const char * input_path, u32 flags){
    // TODO: 正式版需要替换为常规文件
    critical_enter();
    
    char file_path[FILE_NAME_MAX]; 
    copy_str_from_user(current_process->page_table, (void *)file_path, (void *)input_path, FILE_NAME_MAX);
    int fd = -1;

    // 找一个 fd 空槽存放
    for (u64 i = 0; i < PROCESS_MAX_FD_COUNT; i ++){
        if (current_process->file_table[i] == NULL){
            fd = i;
            break;
        }
    }

    if (fd == -1) goto failed;
    File* file = open_file(sv_from_cstr(file_path), flags, &test_fop, INODE_REG);

    if (!file) goto failed;
    current_process->file_table[fd] = file;

    critical_exit();
    return fd;
failed:
    critical_exit();
    return -1;
}
