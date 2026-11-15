#include "kernel/lib/def.h"
#include "kernel/lib/string.h"
#include "kernel/mm/pmm.h"
#include "kernel/proc/lock.h"

#include "file.h"
#include "fs.h"
#include "inode.h"
#include "path.h"
#include "pipe.h"


InodeFreeFunc free_inode_table[] = {
    [ INODE_NONE ]  = NULL,  // none 不允许释放
    [ INODE_REG ]   = free_reg_inode,
    [ INODE_DIR ]   = free_dir_inode,
    [ INODE_PIPE ]  = free_pipe_inode,
    [ INODE_IO ]    = NULL,    // io 不允许释放
};


Inode inode_table[FS_MAX_INODE_COUNT];
Inode* root_inode;

void inode_init(){
    memset(inode_table, 0, sizeof(Inode) * FS_MAX_INODE_COUNT);
    for (u64 i = 0; i < FS_MAX_INODE_COUNT; i ++){
        inode_table[i].ino = i;
    }
    root_inode = create_inode(PAGE_SIZE, INODE_DIR);
}

Inode* find_empty_inode(){
    for (u64 i = 0; i < FS_MAX_INODE_COUNT; i ++){
        ASSERT(inode_table[i].ino == i);
        if (inode_table[i].ref == 0) return &inode_table[i];
    }
    return NULL;
}

Inode *find_inode(u64 ino){
    ASSERT(ino < FS_MAX_INODE_COUNT);
    return &inode_table[ino];
}

/* 
 * 引用计数增加 
 * NOTE: 谁持有这个 Inode 就需要增加引用次数
 */
void ref_inode(Inode *inode){
    critical_enter();
    inode->ref ++;
    critical_exit();
}

/* inode 引用计数减少，归零时内部 free */
void unref_inode(Inode *inode){
    critical_enter();
    ASSERT(inode->ref >= 1);
    inode->ref --;
    if (inode->ref == 0){
        ASSERT(inode->free_inode(inode));
    }
    critical_exit();
}

bool free_reg_inode(Inode *inode){
    critical_enter();

    if (!inode) PANIC("inode is NULL");
    if (inode == root_inode){
        PANIC("free root inode error");
    }

    kfree(inode->vaddr);
    u64 id = inode->ino;
 
    // NOTE :清空一切，可能会破坏什么数据
    memset(inode, 0, sizeof(Inode));
    inode->ino = id;

    critical_exit();
    return true;
}

/*
 *  对所有目录项的Inode进行减少引用
 */
bool free_dir_inode(Inode* i){
    critical_enter();
    int count = i->cap / sizeof(DirEntry);
    DirEntry* dir = (DirEntry*)i->vaddr;

    for (int j = 0; j < count; j ++){
        if (dir[j].inode) {
            unref_inode(dir[j].inode);
        }
    }

    kfree(i->vaddr);
    u64 id = i->ino;
    memset(i, 0, sizeof(Inode));
    i->ino = id;

    critical_exit();
    return true;
}

/*
 * 根据文件名在指定目录中寻找文件
 */
DirEntry* lookup_dir_entry(Inode *fa, StringView sv){
    if unlikely (!fa || fa->mode != INODE_DIR)              return NULL;
    if unlikely (!sv.data || sv.count == 0)                 return NULL;
    if unlikely (sv.count >= FILE_NAME_MAX)                 return NULL;
    DirEntry* d = (DirEntry*)fa->vaddr;

    critical_enter();
    for (u64 i = 0; i < fa->cap / sizeof(DirEntry); i ++){
        if (d[i].inode == NULL) continue;
        if (sv_eq(sv_from_cstr(d[i].name), sv)){
            critical_exit();
            return &d[i];
        }
    }
    critical_exit();
    return NULL;
}

/* 
 * 将 son文件 插入 fa目录 下，fname 是文件名
 * 目录文件满了返回 false
 * 对 file 进行一次引用
 */
bool insert_dir_entry(Inode * fa, Inode* son, StringView fname){
    if unlikely (!fa || fa->mode != INODE_DIR)      return false;
    if unlikely (!son)                              return false;
    if unlikely (!fname.data || fname.count == 0)   return false;
    if unlikely (fname.count >= FILE_NAME_MAX)      return false;
    if unlikely (fa->size == fa->cap)               return false;
    if unlikely (lookup_dir_entry(fa, fname))       return false;
    if unlikely (!is_vaild_filename(fname))         return false;

    critical_enter();
    DirEntry* d = (DirEntry*)fa->vaddr;
    u64 count = fa->cap / sizeof(DirEntry);

    for (u64 i = 0; i < count; i ++){
        if (sv_eq(fname, sv_from_cstr(d[i].name))){
            goto failed;
        }
    }

    for (u64 i = 0; i < count; i ++){
        if (d[i].inode == NULL){
            d[i].inode = son;
            sv_to_cstr_buf(fname, d[i].name, FILE_NAME_MAX);
            ref_inode(son); // ref
            fa->size += sizeof(DirEntry);

            critical_exit(); 
            return true;
        }
    }

failed:
    critical_exit(); 
    return false;
}

/*
 *  释放目录项，不检查任何东西，会对inode unref
 */
void clean_dir_entry(Inode* fa_inode, DirEntry* e){
    Inode *found = e->inode;
    e->inode = NULL;
    e->name[0] = 0;
    fa_inode->size -= sizeof(DirEntry);
    unref_inode(found); 
    return;
}

/* 
 * 释放一个文件目录项 并且对其的 inode unref，所以会 free Inode
 * 释放 size
 */
bool remove_dir_entry(Inode *fa, Inode *son){
    if unlikely (!fa || fa->mode != INODE_DIR)              return false;
    if unlikely (!son)                                      return false;

    critical_enter();

    DirEntry* d = (DirEntry*)fa->vaddr;
    for (u64 i = 0; i < fa->cap / sizeof(DirEntry); i ++){
        if (d[i].inode == son){
            clean_dir_entry(fa, &d[i]);
            critical_exit();
            return true;
        }
    }

    critical_exit();
    return false;
}

/* 
 * 创建新Inode 
 * need_size 需要小于 PAGE_SIZE 
 * NOTE: 不会对 新inode->ref 做一次引用，调用者根据需要自己引用
 */
Inode* create_inode(u64 need_size, u32 mode){
    critical_enter();

    if (need_size > PAGE_SIZE) goto failed;

    // 1. find
    Inode* in = find_empty_inode();
    if (!in) goto failed;

    // 2. init
    in->vaddr = kalloc_page(); // 固定为 1 page
    if(!in->vaddr) goto failed;
    // 默认清空
    memset(in->vaddr, 0, PAGE_SIZE);

    in->cap = PAGE_SIZE;
    in->size = 0;
    in->mode = mode;
    in->free_inode = free_inode_table[mode];

    critical_exit();
    return in;

    // free_page:
failed:
    critical_exit();
    return NULL;
}

/* 
 * 在从指定目录开始寻找Inode目录项路径
 * returns:
 *      address of inode
 *      If failed, returns NULL
 */
Inode *lookup_inode(Inode *dir, StringView path){
    if (!dir) return NULL;

    critical_enter(); // 防止中断释放其中的文件

    // 防御一下
    if (!is_vaild_path(path)){
        goto failed;
    }

    // 合法路径并且长度为 1 即 "/" 返回根
    if (path.count == 1){
        dir = root_inode;
        goto success;
    }

    while (path.count){
        sv_chop_left(&path, 1);  // 切掉每段的先导 '/'
        if (path.count == 0) break;

        u64 pos = path.count;
        for (u64 i = 0; i < path.count; i ++){
            if (path.data[i] == '/'){
                pos = i;
                break;
            }
        }

        StringView file_name = sv_from_parts(path.data, pos);

        DirEntry *e = lookup_dir_entry(dir, file_name);
        if (!e) goto failed;

        dir = e->inode;

        sv_chop_left(&path, pos);

        // 如果后面还有
        if (path.count && dir->mode != INODE_DIR) goto failed;
    }

success:
    critical_exit();
    return dir;
failed:
    critical_exit();
    return NULL;
}

/*
 *  在根目录寻找 path 的父目录，如果 path 的父目录是根目录也成立
 *  无法对根目录查找父亲目录
 *  returns : 
 *      bool : success or faield
 *      fa_inode : Dir Inode
 *      fa_path : Dir path name
 */
bool lookup_parent(StringView path, Inode **fa_inode, StringView *fa_name){
    critical_enter();

    // 根目录拒绝
    if unlikely (!is_vaild_path(path) || path.count == 1) goto failed;

    StringView dir_path;
    if unlikely (!path_get_dir(path, &dir_path))      goto failed;
    if unlikely (!path_get_filename(path, fa_name))   goto failed;
    if unlikely (fa_name->count == 0 || fa_name->count >= FILE_NAME_MAX) goto failed;

    Inode *res = lookup_inode(root_inode, dir_path);
    if (!res) goto failed;
    if (res->mode != INODE_DIR)     goto failed;

    *fa_inode = res;

    critical_exit();
    return true;
failed:
    critical_exit();
    return false;
}
