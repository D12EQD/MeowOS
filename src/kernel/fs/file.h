/*
 *  通用文件定义，定义普通字节文件和目录文件
 */
#pragma once

#include "inode.h"
#include "kernel/lib/def.h"
#include "kernel/lib/string.h"

#define FS_MAX_FILE_COUNT   64 /* 最大文件数量 */

/* 文件打开权限 */
#define O_READ      0b00001
#define O_WRITE     0b00010
#define O_RW        O_READ | O_WRITE
#define O_CREATE    0b00100

#define SEEK_SET    0b00001     /* The file offset is set to offset bytes. */
#define SEEK_CUR    0b00010     /* The file offset is set to its current location plus offset bytes. */
#define SEEK_END    0b00100     /* The file offset is set to the size of the file plus offset bytes. */

struct FileOp_S;
struct File_S;
typedef struct File_S {
    const struct FileOp_S *op;  // 分派用，不存类型
    struct Inode_S *inode;
    u64 offset;                 // 读写偏移
    u32 flags;                  // O_*
    u32 ref;                    // 打开实例引用计数，初值 1
} File;

typedef struct FileOp_S {
    // 读写文件内容
    i64 (*read)(File *f, char *user_buf, size_t n);
    i64 (*write)(File *f, const char *user_buf, size_t n);
    i64 (*lseek)(File *f, i64 offset, int whence);
 
    /* 
     * 仅释放文件资源，需要处理引用计数
     * 不对 inode 进行处理 
     */
    i64 (*release)(File *f);

    /* 新增一个引用时调用 */
    i64 (*dup)(File *f);
} FileOp;


#define is_invaild_fd(fd) (( fd ) < 0 || ( fd ) >= PROCESS_MAX_FD_COUNT)

extern File file_table[FS_MAX_FILE_COUNT];
extern File *root_file;

File *find_empty_file();

void ref_file(File *file);
void unref_file(File *file);

File *create_file(FileOp *op,Inode *inode,u64 offset,u32 flags);

i64 read_file(File *f,char *buf,u64 n);
i64 write_file(File *f,const char *buf,u64 n);

File *open_file(StringView name,u32 flags,FileOp *operation,u32 i_mode);

i64 dir_release(File *f);

bool dir_readdir(File *f, void *user_out);


