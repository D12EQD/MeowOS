#include "file.h"
#include "fs.h"
#include "path.h"

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

i64 fs_test_read (File *f, char *buf, size_t n){
    pr_debug("read");
    return n;
}

i64 fs_test_write (File *f, const char *buf, size_t n){
    pr_debug("write");
    return n;
}

i64 fs_test_lseek (File *f, i64 offset, int whence){
    pr_debug("lseek");
    return 0;
}

i64 fs_test_readdir (File *f, DirEntry *out){
    out = NULL;
    return 0;
}

int fs_test_release (File *f){
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
            pr_debug("%s/", d[i].name);

            char child[16 * 8];
            u64 plen = 0;
            while (prefix[plen]) { child[plen] = prefix[plen]; plen ++; }

            const char *seg = is_last ? "    " : "│   ";
            u64 k = 0;
            while (seg[k]) { child[plen + k] = seg[k]; k ++; }
            child[plen + k] = 0;

            fs_dfs(d[i].inode, child, depth + 1);
        } else {
            pr_debug("%s", d[i].name);
        }
    }
}

void fs_init(){
    for (u64 i = 0; i < FS_MAX_INODE_COUNT; i ++){
        inode_table[i].ino = i;
    }
    root_inode = create_inode(PAGE_SIZE, INODE_DIR);
    root_file = create_file(&dir_fop, root_inode, 0, O_RW);
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
