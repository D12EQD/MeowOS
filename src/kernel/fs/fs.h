#pragma once
#include "file.h"

// io 设备位置
#define IO_DIR      "/dev"
#define IO_STDIN    IO_DIR"/stdin"
#define IO_STDOUT   IO_DIR"/stdout"

extern FileOp test_fop;

void fs_display(void);
void fs_dfs(Inode *dir,const char *prefix,int depth);
void fs_init();
bool fs_mkdir(StringView path);
bool fs_unlink(StringView path);
bool fs_rmdir(StringView path);
bool fs_close(int fd);
File *dup_file(File *f);

i64 kwrite(i64 fd, void *buf, size_t len);
i64 kread(i64 fd, void *buf, size_t len);
i64 kopen(const char * filepath, u32 flags);
i64 kunlink(const char * user_path);

i64 fs_test_write (File *, const char *, size_t n);
