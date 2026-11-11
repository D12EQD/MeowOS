#include "kernel/lib/def.h"
#include "kernel/lib/string.h"
#include "kernel/mm/pmm.h"
#include "kernel/fs/file.h"
#include "kernel/fs/fs.h" // TEST: 

#include "kernel/proc/lock.h"
#include "kernel/proc/process.h"

#include "kernel/fs/path.h"


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

Inode *find_inode(u64 ino){
    ASSERT(ino < FS_MAX_INODE_COUNT);
    return &inode_table[ino];
}

/* 
 * 打开新文件 
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
    // 默认清空 固定为 1 page
    memset(in->vaddr, 0, PAGE_SIZE);
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
        if (d[i].inode == NULL){
            d[i].inode = son;
            sv_to_cstr_buf(fname, d[i].name, FILE_NAME_MAX);
            ref_inode(son); // ref
            fa->size += sizeof(DirEntry);

            critical_exit(); 
            return true;
        }
    }

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

void free_inode(Inode *inode){
    critical_enter();

    if (inode == root_inode){
        PANIC("free root inode error");
    }

    kfree(inode->vaddr);
    u64 id = inode->ino;
    memset(inode, 0, sizeof(Inode)); // clear ?
    inode->ino = id;

    critical_exit();
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

// --ref，归零时内部 free
void unref_inode(Inode *inode){
    critical_enter();
    ASSERT(inode->ref >= 1);
    inode->ref --;
    if (inode->ref == 0){
        free_inode(inode);
    }
    critical_exit();
}

/* 
 * out is the entry
 * 0 success 
 * -1 failed 
 */
i64 dir_readdir(File *f, DirEntry *out){
    if (!f || f->inode->mode != INODE_DIR) return -1;

    Inode *dir = f->inode;
    u64 n = dir->cap / sizeof(DirEntry);
    DirEntry *d = (DirEntry *)dir->vaddr;

    for (u64 i = f->offset / sizeof(DirEntry); i < n; i ++) {
        if (!d[i].inode) continue;
        *out = d[i];
        f->offset = (i + 1) * sizeof(DirEntry);
        return 0;
    }
    return -1;
}

/*
 * 在根目录寻找 path 的父目录，如果 path 的父目录是根目录也成立
 * returns : 
 *      bool : success or faield
 *      fa_inode : Dir Inode
 *      fa_path : Dir path name
 */
bool lookup_parent(StringView path, Inode **fa_inode, StringView *fa_name){
    critical_enter();

    // 跟目录拒绝
    if (!is_vaild_path(path) || path.count == 1) goto failed;

    StringView dir_path;
    if (!path_get_dir(path, &dir_path))      goto failed;
    if (!path_get_filename(path, fa_name))   goto failed;
    if (fa_name->count == 0 ||
     fa_name->count >= FILE_NAME_MAX)     goto failed;

    Inode *res = lookup_inode(root_inode, dir_path);
    if (!res)                       goto failed;
    if (res->mode != INODE_DIR)     goto failed;   /* /x/y 里 /x 是文件 */

    *fa_inode = res;

    critical_exit();
    return true;
failed:
    critical_exit();
    return false;
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

    if (!inode){
        is_create = true;
        if (!(flags & O_CREATE)) goto failed;
        flags &= ~O_CREATE;

        /* ref = 0 */
        inode = create_inode(PAGE_SIZE, i_mode);
        if (!inode) goto failed;

        ref_inode(inode); /* ref = 1 */
        if (!insert_dir_entry(dir_inode, inode, filename)){
            goto unref_file_inode; /* ref = 1 -> 0 */
        }
        unref_inode(inode); /* ref = 1 目录项持有 */
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

i64 file_read(File *f, char *buf, u64 n){
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
i64 file_write(File *f, const char *buf, u64 n){
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

void free_file(File* file){
    i64 offset = ( (u64)file - (u64)file_table ) / sizeof(File);
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
    // TEST:
    ASSERT(file->ref != 0);
    if (-- file->ref == 0){
        unref_inode(file->inode);
        free_file(file);
    }
    critical_exit();
}

/* 创建Inode并且默认引用一次 */
Inode* create_ref_inode(u64 need_size, u32 mode){
    Inode* inode = create_inode(need_size, mode);
    ref_inode(inode);
    return inode;
}



