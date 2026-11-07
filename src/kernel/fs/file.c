#include "kernel/lib/def.h"
#include "kernel/lib/string.h"
#include "kernel/mm/pmm.h"
#include "kernel/fs/file.h"
#include "kernel/proc/lock.h"
#include "kernel/proc/process.h"

/*
 * 没有相对路径，因为进程没有记录路径
 * 所有都看作绝对路径
 * 路径规则见 is_vaild_path
 */

Inode inode_table[FS_MAX_INODE_COUNT];
File file_table[FS_MAX_FILE_COUNT];

// 根文件目录
File* root_file;
Inode* root_inode;


/* 合法路径只能使用英文字母 + 数字 + / */
bool is_vaild_path(StringView path){
    if (path.count > FILE_NAME_MAX) return false;

    int slash = 0;

    if (path.data[0] != '/') return false;

    for (u64 i = 0; i < path.count; i ++){
        char c = path.data[i];
        switch (c) {
            case '/':
                slash ++;
                if (slash == 2) return false;
                break;
            default:
                bool res =  (c >= 'a' && c <= 'z') || 
                            (c >= 'A' && c <= 'Z') || 
                            (c >= '0' && c <= '9');
                if (!res) return false;
                slash = 0;
        } 
    }
    return true;
}

void fs_init(){
    for (u64 i = 0; i < FS_MAX_INODE_COUNT; i ++){
        inode_table[i].ino = i;
    }
    // TODO : init root dir
}

File* find_empty_file(){
    for (u64 i = 0; i < FS_MAX_FILE_COUNT; i ++){
        if (file_table[i].ref == 0) return &file_table[i];
    }
    return NULL;
}

Inode* find_empty_inode(){
    for (u64 i = 0; i < FS_MAX_INODE_COUNT; i ++){
        // TEST : 
        ASSERT(inode_table[i].ino == i);
        if (inode_table[i].ref == 0) return &inode_table[i];
    }
    return NULL;
}

inline Inode *find_inode(u64 ino){
    ASSERT(ino < FS_MAX_INODE_COUNT);
    return &inode_table[ino];
}

/* 打开新文件 */
File* create_file(FileOP *op, Inode *inode, u64 offset, u32 flags){
    critical_enter();
    File* new_file = find_empty_file();
    if (!new_file) goto failed;

    new_file->op = op;
    new_file->offset = offset;
    new_file->inode = inode;
    new_file->flags = flags;
    new_file->ref = 1;
    ref_inode(inode); // +1 引用

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
Inode* create_inode(u64 need_size, u32 mode){
    critical_enter();

    if (need_size > PAGE_SIZE) goto failed;

    // 1. find
    Inode* in = find_empty_inode();
    if (!in) goto failed;
 
    // 2. init
    in->vaddr = kalloc_page(); // 固定为 1 page
    if(!in->vaddr) goto failed;
    ref_inode(in);
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
    if unlikely (!fa || fa->mode != INODE_DIR)      return false;
    if unlikely (!son)                              return false;
    if unlikely (!sv.data || sv.count == 0)         return false;
    if unlikely (sv.count >= FILE_NAME_MAX)         return false;
    if unlikely (fa->size < fa->cap)                return false;
    if unlikely (find_dir_entry(fa, sv))            return false;

    critical_enter();
    DirEntry* d = (DirEntry*)fa->vaddr;
    u64 count = fa->cap / sizeof(DirEntry);

    for (u64 i = 0; i < count; i ++){
        if (d[i].inode == NULL){
            d[i].inode = son;
            sv_to_cstr_buf(sv, d[i].name, sv.count);
            ref_inode(son);
            fa->size += sizeof(DirEntry);
            critical_exit(); return true;
        }
    }
    critical_exit(); return false;
}

/* 
 * 释放一个文件目录项 
 * 释放 size 
 */
bool remove_dir_entry(Inode *fa, Inode *son){
    if unlikely (!fa || fa->mode != INODE_DIR)              return false;
    if unlikely (!son)                                      return false;

    critical_enter();

    DirEntry* d = (DirEntry*)fa->vaddr;
    for (u64 i = 0; i < fa->size / sizeof(DirEntry); i ++){
        if (d[i].inode == son){
            d[i].inode = NULL;
            d[i].name[0] = 0;
 
            unref_inode(d[i].inode);
            fa->size -= sizeof(DirEntry);
            critical_exit();
            return true;
        }
    }

    critical_exit();
    return false;
}

DirEntry* find_dir_entry(Inode *fa, StringView sv){
    if unlikely (!fa || fa->mode != INODE_DIR)              return NULL;
    if unlikely (!sv.data || sv.count == 0)                 return NULL;
    if unlikely (sv.count >= FILE_NAME_MAX)                 return NULL;

    DirEntry* d = (DirEntry*)fa->vaddr;

    for (u64 i = 0; i < fa->size / sizeof(DirEntry); i ++){
        if (sv_eq(sv_from_cstr(d[i].name), sv)){
            return &d[i];
        }
    }
    return NULL;
}

void free_inode(Inode *inode){
    critical_enter();

    kfree(inode->vaddr);
    u64 id = inode->ino;
    memset(inode, 0, sizeof(Inode)); // clear ?
    inode->ino = id;

    critical_exit();
}

/* 引用计数增加 */
inline void ref_inode(Inode *inode){
    critical_enter();
    inode->ref ++;
    critical_exit();
}

// --ref，归零时内部 free
inline void unref_inode(Inode *inode){
    critical_enter();
    ASSERT(inode->ref >= 1);
    inode->ref --;
    if (inode->ref == 0){
        free_inode(inode);
    }
    critical_exit();
}

/* 
 * 在从指定目录开始寻找Inode目录项路径，返回Inode 
 */
Inode *lookup_inode(Inode *dir, StringView path){
    critical_enter(); // 防止中断释放其中的文件

    // 防御一下
    if (!is_vaild_path(path)) return NULL;
    sv_chop_left(&path, 1); // cut "/"

    while (path.count){
        i64 pos = -1;
        for (u64 i = 0; i < path.count; i ++){
            if (path.data[i] == '/'){
                pos = i;
                break;
            }
        }

        StringView file_name;
        if (pos == -1){
            // 说明就是path就是整个文件名
            file_name = path;
            path.count = 0;
        }else{
            file_name = sv_from_parts(path.data, pos);
            sv_chop_left(&path, pos + 1);
        }
        
        // find dir entry
        DirEntry * e = find_dir_entry(dir, file_name);
        if (e == NULL) goto failed;

        dir = e->inode;
        if (path.count && dir->mode != INODE_DIR) goto failed;
    }

    critical_exit();
    return dir;
failed:
    critical_exit();
    return NULL;
}

/* 打开文件，如果不存在默认创建文件 */
File* open_file(StringView name, u32 flags, FileOP* operation, u32 i_mode){
    if (name.count >= FILE_NAME_MAX)    return NULL;
    if (!is_vaild_path(name))           return NULL;

    Inode * inode = lookup_inode(root_inode, name);
    if (!inode){
        inode = create_inode(PAGE_SIZE, i_mode);
        if (!inode) goto failed;
    }else{
        if (inode->mode != i_mode) goto failed;
    }

    File * file = create_file(operation, inode, 0, flags);
    return file;
failed:
    return NULL;
}


/* 递归打印目录树 debug */
static void fs_dfs(Inode *dir, const char *prefix, int depth) {
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

void fs_display(void) {
    critical_enter();
    pr_debug("/");
    if (root_inode && root_inode->mode == INODE_DIR) {
        fs_dfs(root_inode, "", 0);
    }
    critical_exit();
}

File * get_file_from_fd(u64 fd){
    ASSERT(fd < FS_MAX_FILE_COUNT);
    if unlikely (file_table[fd].inode == NULL) return NULL;
    return &file_table[fd];
}

void free_file(File* file){
    int offset = (u64)file - (u64)file_table;
    ASSERT(offset >= 0 && offset < FS_MAX_FILE_COUNT);
    memset(file, 0, sizeof(File));
}

void ref_file(File *file){
    critical_enter();
    file->ref ++;
    critical_exit();
}

void unref_file(File * file){
    critical_enter();
    if (-- file->ref == 0){
        unref_inode(file->inode);
        free_file(file);
    }
    critical_exit();
}
