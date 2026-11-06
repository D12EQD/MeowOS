#include "kernel/lib/def.h"
#include "kernel/lib/string.h"
#include "kernel/mm/pmm.h"
#include "kernel/fs/file.h"
#include "kernel/proc/lock.h"

/*
 * If I is a system file descriptors, I = file_table[I].
 * If I is a user file descriptors, I = file_table[I] too.
 * It is not safe,
 */
Inode inode_table[FS_MAX_INODE_COUNT];
File file_table[FS_MAX_INODE_COUNT];

File root_dir; // 根文件

File* find_empty_file(){
    for (u64 i = 0; i < FS_MAX_FILE_COUNT; i ++) if (file_table[i].ref == 0) return &file_table[i];
    return NULL;
}

Inode* find_empty_inode(){
    for (u64 i = 0; i < FS_MAX_INODE_COUNT; i ++) if (inode_table[i].ref == 0) return &inode_table[i];
    return NULL;
}

/* 创建新文件 */
File* create_file(
    FileOP *op, Inode *inode,
    u64 offset, u32 flags
){
    critical_enter();
    File* new_file = find_empty_file();
    if (!new_file) goto failed;
    if (inode->ino >= 0 && inode->ino <= FS_MAX_INODE_COUNT - 1){
        if (&inode_table[inode->ino] != inode) goto failed;
    }else{
        goto failed;
    }

    new_file->op = op;
    new_file->offset = offset;
    new_file->inode = inode;
    new_file->flags = flags;
    new_file->ref = 1;

    critical_exit();
    return new_file;
failed:
    critical_exit();
    return NULL;
}

/* 
 * 创建新Inode 
 * need_size 需要小于 PAGE_SIZE 
 */
Inode* create_inode(
    u64 need_size, u32 mode
){
    critical_enter();

    if (need_size > PAGE_SIZE) goto failed;
    
    // 1. find
    Inode* in = find_empty_inode();
    if (!in) goto failed;
    ASSERT(in->ino >= 0 && in->ino <= FS_MAX_INODE_COUNT - 1);
    
    // 2. init
    in->vaddr = kalloc_page(); // 固定为 1 page
    if(!in->vaddr) goto failed;
    in->ref = 1;
    in->cap = PAGE_SIZE;
    in->size = 0;
    in->mode = mode;

    critical_exit();
    return in;

// free_page:
failed:
    critical_exit();
    return NULL;
}

/* 将 son 插入 fa 下 */
bool insert_dir_entry(Inode * fa, Inode* son, StringView sv){
    critical_enter();
    ASSERT(fa->mode == INODE_DIR);
    DirEntry* d = (DirEntry*)fa->vaddr;
    for (u64 i = 0; i < fa->size / sizeof(DirEntry); i ++){
        if (d[i].inode == NULL){
            d[i].inode = son;
            sv_to_cstr_buf(sv, d[i].name, sv.count);
            critical_exit();
            return true;
        }
    }
    critical_exit();
    return false;
}

/* 释放一个文件目录项 不释放inode */
bool remove_dir_entry(Inode *inode){
    critical_enter();
    DirEntry* d = (DirEntry*)inode->vaddr;
    for (u64 i = 0; i < inode->size / sizeof(DirEntry); i ++){
        if (d[i].inode == inode){
            d[i].inode = NULL;
            d[i].name[0] = 0;
            critical_exit();
            return true;
        }
    }
    critical_exit();
    return false;
}

DirEntry* find_dir_entry(StringView sv){

}

void free_inode(Inode *inode){
    kfree(inode->vaddr);
    u64 id = inode->ino;
    memset(inode, 0, sizeof(Inode)); // clear ?
    inode->ino = id;
}
