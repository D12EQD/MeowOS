#pragma once
#include "kernel/lib/def.h"

/*
 * 只支持三种文件
 * 标准输入输出文件和管道文件
 */

struct File;
struct Inode;
struct FileOP;

typedef struct File{
    const struct FileOP *f_op;
    struct Inode *f_inode;
    const char *path;
} File;

typedef struct Inode {
    u32  mode;                      // 文件类型 + 权限
    u32  ref;                       // 引用计数
    u64  ino;                       // 唯一编号
    u64  size;                      // 普通文件字节数
    const struct FileOP *f_op;      // 这个节点的默认操作集
} Inode;

typedef struct FileOP{
	u64 fop_flags;
	u64 (*read) (File *, char *, size_t, u64 *);
	u64 (*write) (File *, const char *, size_t, u64 *);
	int (*open) (Inode *, File *);
	int (*release) (Inode *, File *);
    u64 (*lseek)(File *, u64, int);
} FileOP;


