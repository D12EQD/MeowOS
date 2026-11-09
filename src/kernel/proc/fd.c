#include "kernel/proc/process.h"

i64  fd_alloc(Process *p, File *f);   /* 找空槽，返回 fd 号，满返回 -1 */
File *fd_get(Process *p, i64 fd);     /* fd 越界或空槽返回 NULL */
void fd_close(Process *p, i64 fd);    /* unref_file + 置 NULL */
