#pragma once
#include "kernel/lib/def.h"


#define FS_MAX_INODE_COUNT  128 /* 最多Inode数量 */
#define FS_MAX_FILE_COUNT   128 /* 最大文件数量 */
#define NAME_MAX            24  /* 文件目录中文件名字最大长度 */

typedef enum {
    INODE_NONE = 0,
    INODE_REG,      // 普通字节文件
    INODE_DIR,      // 目录
    INODE_PIPE,     // 管道
    INODE_IO,       // 控制台 IO，唯一外设
} InodeType;

/* 文件打开权限 */
#define O_READ      0b00001
#define O_WRITE     0b00010
#define O_RW        O_READ | O_WRITE
#define O_CREATE    0b00100

struct Inode_S;
struct FileOP_S;
struct File_S;

/* 目录项：定长数组元素 */
typedef struct DirEntry_S {
    char name[NAME_MAX];
    struct Inode_S *inode;
} DirEntry;

/* 索引节点 */
typedef struct Inode_S {
    u64 ino;                     // 唯一编号
    // data
    u8 *vaddr;                   // 内核可访问数据区
    u64 size;                    // 数据区容量已经使用了
    u64 cap;                     // 数据区容量

    u32 ref;                     // 引用计数：目录项 + 打开实例
    u32 mode;                    // InodeType
} Inode;

typedef struct File_S {
    const struct FileOP_S *op;  // 分派用，不存类型
    struct Inode_S *inode;
    u64 offset;                 // 读写偏移
    u32 flags;                  // O_*
    u32 ref;                    // 打开实例引用计数，初值 1
} File;

typedef struct FileOP_S {
    i64 (*read)(File *f, char *buf, size_t n);
    i64 (*write)(File *f, const char *buf, size_t n);
    i64 (*lseek)(File *f, i64 offset, int whence);
    i64 (*readdir)(File *f, DirEntry *out); // 目录专用
    int (*release)(File *f);
} FileOP;
