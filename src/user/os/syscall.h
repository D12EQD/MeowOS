#pragma once
#include "user/def.h"

/* 文件打开权限 */
#define O_READ      0b00001
#define O_WRITE     0b00010
#define O_RW        O_READ | O_WRITE
#define O_CREATE    0b00100

#define SEEK_SET    0b00001     /* The file offset is set to offset bytes. */
#define SEEK_CUR    0b00010     /* The file offset is set to its current location plus offset bytes. */
#define SEEK_END    0b00100     /* The file offset is set to the size of the file plus offset bytes. */

#define FILE_NAME_MAX       24 /* 文件目录中文件名字最大长度 */

/* 目录项：定长数组元素 */
typedef struct DirEntry_S {
    char name[FILE_NAME_MAX];
} DirEntry;

// ========================== X define ==========================
#define SYSCALL_LIST(X)             \
    X(READ,     read,    0)         \
    X(WRITE,    write,   1)         \
    X(OPEN,     open,    2)         \
    X(CLOSE,    close,   3)         \
    X(FORK,     fork,    5)         \
    X(WAIT,     wait,    6)         \
    X(EXIT,     exit,    7)         \
    X(GETPID,   getpid,  8)         \
    X(PALLOC,   palloc,  9)         \
    X(PFREE,    pfree,  10)         \
    X(MKDIR,    mkdir,  11)         \
    X(DUP,      dup,    12)         \
    X(DUP2,     dup2,   13)         \
    X(READDIR,  readdir, 14)        \
    X(UNLINK,   unlink, 15)         \
    X(PIPE,     pipe,   16)         \
    X(LSEEK,    lseek,  17)


/* 编号 */
#define __SC_ENUM(upper, lower, num) SYS_##upper = num,
enum SyscallNumber {
    SYSCALL_LIST(__SC_ENUM)
    SYS_COUNT
};
#undef __SC_ENUM

u64 syscall(u64 sysnumber, u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5);
u64 syscall(u64 sysnumber,u64 arg0,u64 arg1,u64 arg2,u64 arg3,u64 arg4,u64 arg5);

u64 getpid(void);
void exit(i64 code);
u64 write(i64 fd,void *buf,u64 buf_len);
u64 read(i64 fd,void *buf,u64 buf_len);
u64 fork();
u64 wait(void *ptr);
void *palloc(u64 page_count);
void pfree(u64 page_count);

i64 close(i64 fd);
i64 open(const char *name, u32 flags);
i64 mkdir(const char *name);
i64 unlink(const char *name);
i64 dup(i64 fd);
i64 dup2(i64 oldfd, i64 newfd);
i64 readdir(i64 fd, DirEntry *out);
i64 lseek(i64 fd, i64 offset, i64 flag);
i64 pipe(i64* fd1, i64* fd2);
