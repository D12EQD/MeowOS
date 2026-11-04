#pragma once
#include "kernel/lib/def.h"

/*
 * 只支持三种文件
 * 标准输入输出文件 和 管道文件
 */

struct File;
struct Inode;
struct FileOP;

typedef enum { 
    FD_NONE, 
    FD_PIPE, 
    FD_INODE, 
    FD_DEVICE 
} FileType;

typedef struct File{
    const struct FileOP *f_op;
    struct Inode *f_inode;
} File;

typedef struct Inode {
    u32  mode;                      // 文件类型 + 权限
    u32  ref;                       // 引用计数
    u64  ino;                       // 唯一编号
    u64  size;                      // 普通文件字节数
    const struct FileOP *f_op;      // 这个节点的默认操作集
} Inode;

typedef struct FileOP{
	u64 (*read) (File *, char *, size_t, u64 *);
	u64 (*write) (File *, const char *, size_t, u64 *);
	int (*open) (Inode *, File *);
	int (*release) (Inode *, File *);
    u64 (*lseek)(File *, u64, int);
} FileOP;

