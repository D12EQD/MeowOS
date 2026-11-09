#pragma once
#include "file.h"

extern FileOp test_fop;

void fs_display(void);
void fs_dfs(Inode *dir,const char *prefix,int depth);
void fs_init();
bool fs_mkdir(StringView path);
bool fs_unlink(StringView path);
bool fs_rmdir(StringView path);
bool fs_close(int fd);
File *dup_file(File *f);


