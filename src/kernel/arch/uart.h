#pragma once

#include "kernel/mm/def.h"
#include "kernel/lib/def.h"

void uart_init(void);
void uart_write(char *buf,int n);
void uart_putc_sync(int c);
void uart_intr(void);

