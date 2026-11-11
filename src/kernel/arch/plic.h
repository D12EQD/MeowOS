#pragma once
#include "kernel/lib/def.h"
#include "kernel/mm/def.h"

void plic_init(void);
void plic_init_hart(void);
int plic_claim(void);
void plic_complete(int irq);
