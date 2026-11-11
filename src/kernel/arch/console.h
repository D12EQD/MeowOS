#pragma once
#include "kernel/arch/sbi.h"

extern int console_chan;
#define CONSOLE_CHAN        ((void *)&console_chan)

void puts(char *s);
void puts_n(char *s, u64 count);

