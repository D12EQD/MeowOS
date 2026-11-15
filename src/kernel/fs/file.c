#include "kernel/lib/def.h"
#include "kernel/lib/string.h"

#include "kernel/proc/lock.h"
#include "kernel/mm/def.h"
#include "kernel/proc/process.h"

#include "file.h"
#include "fs.h"
#include "path.h"

File file_table[FS_MAX_FILE_COUNT];
File* root_file;

File* find_empty_file(){
    for (u64 i = 0; i < FS_MAX_FILE_COUNT; i ++){
        if (file_table[i].ref == 0) return &file_table[i];
    }
    return NULL;
}

void ref_file(File *file){
    critical_enter();
    file->ref ++;
    critical_exit();
}

void unref_file(File * file){
    critical_enter();
    ASSERT(file->ref != 0);
    if (-- file->ref == 0){
        unref_inode(file->inode); // 对 inode 进行解引用
        *file = (File){0}; // clear
    }
    critical_exit();
}

/* 
 * 打开新文件 
 * returns:
 *      NULL failed
 *      else success
 * NOTE:: 会对 inode 进行引用
 */
File* create_file(FileOp *op, Inode *inode, u64 offset, u32 flags){
    critical_enter();
    File* new_file = find_empty_file();
    if (!new_file) goto failed;

    new_file->op = op;
    new_file->offset = offset;
    new_file->inode = inode;
    new_file->flags = flags;
    new_file->ref = 1;
    ref_inode(inode);

    critical_exit();
    return new_file;
failed:
    critical_exit();
    return NULL;
}

/* 通用文件读取操作 */
i64 read_file(File *f, char *buf, u64 n){
    if (!f) return -1;
    if (!f->inode) return -1;
    if (!(f->flags & O_READ)) return -1;
    if (!n) return 0;

    i64 cost = 0;
    while ((u64)cost < n){
        i64 read_in = f->op->read(f, buf + cost, n - cost);
        if (read_in < 0) return -1;
        if (read_in == 0) break;
        cost += read_in;
    }

    return cost;
}

/* 文件通用写入操作 */
i64 write_file(File *f, const char *buf, u64 n){
    if (!f) return -1;
    if (!f->inode) return -1;
    if (!(f->flags & O_WRITE)) return -1;
    if (!n) return 0;

    i64 cost = 0;
    while ((u64)cost < n){
        i64 write_in = f->op->write(f, buf + cost, n - cost);
        if (write_in < 0) return -1;
        if (write_in == 0) break;
        cost += write_in;
    }

    return cost;
}

/* 
 *  打开文件，如果不存在默认创建文件并且 +1 引用计数
 *  returns : 
 *      sucess : a file 
 *      failed : NULL
 */
File* open_file(StringView name, u32 flags, FileOp* operation, u32 i_mode){
    StringView dir_path, filename;
    Inode *dir_inode, *inode;
    File *file;

    if (name.count >= FILE_NAME_MAX)         return NULL;
    if (!is_vaild_path(name))                return NULL;
    if (!path_get_dir(name, &dir_path))      return NULL;
    if (!path_get_filename(name, &filename)) return NULL;

    dir_inode = lookup_inode(root_inode, dir_path);
    inode = lookup_inode(root_inode, name);

    if (!dir_inode) return NULL;

    critical_enter();

    bool is_create = false;

    if (sv_eq(name, sv_lit("/"))){
        inode = root_inode;
    }else if (!inode){
        if (!((flags & O_CREATE))) goto failed;

        is_create = true;
        flags &= ~O_CREATE;

        /* ref = 0 */
        inode = create_inode(PAGE_SIZE, i_mode);
        if (!inode) goto failed;

        ref_inode(inode); /* ref = 1 */
        if (!insert_dir_entry(dir_inode, inode, filename)){
            goto unref_file_inode; /* ref = 1 -> 0 */
        }

        unref_inode(inode); 
        /* ref = 1 目录项持有 */

    }else{
        if (inode->mode != i_mode) goto failed;
    }

    file = create_file(operation, inode, 0, flags);  /* ref_inode -> +1 */
    if (!file) goto unref_file_inode;

    critical_exit();
    return file;

unref_file_inode:
    if (is_create) unref_inode(inode);

failed:
    critical_exit();
    return NULL;
}

/* 
 *  out 返回目录指针
 *  return: bool
 */
bool dir_readdir(File *f, void *user_out){
    if (!f || f->inode->mode != INODE_DIR) return -1;

    Inode *dir = f->inode;
    u64 n = dir->cap / sizeof(DirEntry);
    DirEntry *d = (DirEntry *)dir->vaddr;

    for (u64 i = f->offset / sizeof(DirEntry); i < n; i ++) {
        if (!d[i].inode) continue;
        f->offset = (i + 1) * sizeof(DirEntry);
        if (copy_to_user(current_process->page_table, user_out, d[i].name, FILE_NAME_MAX) < 0){
            return -1;
        }
        return 0;
    }
    return -1;
}

i64 dir_release(File* f){
    ASSERT(f && f->inode);
    unref_inode(f->inode);
    return 0;
}
