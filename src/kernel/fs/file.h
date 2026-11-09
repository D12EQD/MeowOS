#pragma once
#include "kernel/lib/def.h"
#include "kernel/lib/string.h"

#define FS_MAX_INODE_COUNT  64 /* 最多Inode数量 */
#define FS_MAX_FILE_COUNT   64 /* 最大文件数量 */

#undef  FILE_NAME_MAX
#define FILE_NAME_MAX       24 /* 文件目录中文件名字最大长度 */

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
struct FileOp_S;
struct File_S;

/* 目录项：定长数组元素 */
typedef struct DirEntry_S {
    char name[FILE_NAME_MAX];
    struct Inode_S *inode;
} DirEntry;

/* 索引节点 */
typedef struct Inode_S {
    u64 ino;                     // 唯一编号
    void *vaddr;                 // 内核可访问数据区
    u64 size;                    // 数据区容量已经使用字节数
    u64 cap;                     // 数据区容量字节数
    u32 ref;                     // 引用计数：目录项 + 打开实例
    u32 mode;                    // InodeType
} Inode;

typedef struct File_S {
    const struct FileOp_S *op;  // 分派用，不存类型
    struct Inode_S *inode;
    u64 offset;                 // 读写偏移
    u32 flags;                  // O_*
    u32 ref;                    // 打开实例引用计数，初值 1
} File;

typedef struct FileOp_S {
    i64 (*read)(File *f, char *buf, size_t n);
    i64 (*write)(File *f, const char *buf, size_t n);
    i64 (*lseek)(File *f, i64 offset, int whence);
    i64 (*readdir)(File *f, DirEntry *out); // 目录专用
    int (*release)(File *f);
} FileOp;

File *find_empty_file();
Inode *find_empty_inode();
Inode *find_inode(u64 ino);
File *create_file(FileOp *op,Inode *inode,u64 offset,u32 flags);
Inode *create_inode(u64 need_size,u32 mode);
bool insert_dir_entry(Inode *fa,Inode *son,StringView fname);
void clean_dir_entry(Inode *fa_inode,DirEntry *e);
bool remove_dir_entry(Inode *fa,Inode *son);
DirEntry *lookup_dir_entry(Inode *fa,StringView sv);
void free_inode(Inode *inode);
void ref_inode(Inode *inode);
void unref_inode(Inode *inode);
i64 dir_readdir(File *f,DirEntry *out);
bool lookup_parent(StringView path,Inode **fa_inode,StringView *fa_name);
Inode *lookup_inode(Inode *dir,StringView path);
File *open_file(StringView name,u32 flags,FileOp *operation,u32 i_mode);
File *get_file_from_fd(u64 fd);

i64 file_read(File *f,char *buf,u64 n);
i64 file_write(File *f,const char *buf,u64 n);
void free_file(File *file);
void ref_file(File *file);
void unref_file(File *file);
Inode *create_ref_inode(u64 need_size,u32 mode);

/* public */

extern Inode* root_inode;
extern File* root_file;
extern Inode inode_table[];
extern File file_table[];


