#pragma once
#include "kernel/lib/def.h"

void console_putc(int c);
int console_write(void* src, int n);
int console_read(void* dst, int n);

void console_intr(int c);
void console_init(void);

