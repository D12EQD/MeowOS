#pragma once
#include "file.h"
#include "kernel/lib/def.h"

extern FileOp pipe_fop;

bool free_pipe_inode(Inode *inode);
bool create_pipe_file(i64* fd1, i64* fd2);

