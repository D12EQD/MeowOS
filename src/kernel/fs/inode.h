#pragma once
#include "kernel/lib/def.h"
#include "path.h"

#define FS_MAX_INODE_COUNT  64 /* 最多Inode数量 */
#define FILE_NAME_MAX       24 /* 文件目录中文件名字最大长度 */

typedef enum {
    INODE_NONE = 0,
    INODE_REG,      // 普通字节文件
    INODE_DIR,      // 目录
    INODE_PIPE,     // 管道
    INODE_IO,       // 控制台 IO，唯一外设
} InodeType;

struct Inode_S;


/* 目录项：定长数组元素 */
typedef struct DirEntry_S {
    char name[FILE_NAME_MAX];
    struct Inode_S *inode;
} DirEntry;

typedef bool (*InodeFreeFunc)(struct Inode_S *);

/* 索引节点 */
typedef struct Inode_S {
    u64 ino;        // 唯一编号
    void *vaddr;    // 内核可访问数据区
    void *private;  // 不同Inode可选区域使用区域
    u64 size;       // 数据区容量已经使用字节数
    u64 cap;        // 数据区容量字节数
    u32 ref;        // 引用计数：目录项 + 打开实例
    u32 mode;       // InodeType
    InodeFreeFunc free_inode;  // free inode 的函数指针
} Inode;


extern Inode *root_inode;

void inode_init();
Inode *find_empty_inode();
Inode *find_inode(u64 ino);
void ref_inode(Inode *inode);
void unref_inode(Inode *inode);
bool free_reg_inode(Inode *inode);
bool free_dir_inode(Inode *i);
DirEntry *lookup_dir_entry(Inode *fa,StringView sv);
bool insert_dir_entry(Inode *fa,Inode *son,StringView fname);
void clean_dir_entry(Inode *fa_inode,DirEntry *e);
bool remove_dir_entry(Inode *fa,Inode *son);
Inode *create_inode(u64 need_size,u32 mode);
Inode *lookup_inode(Inode *dir,StringView path);
bool lookup_parent(StringView path,Inode **fa_inode,StringView *fa_name);

