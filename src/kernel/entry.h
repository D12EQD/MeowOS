#pragma once
#include "kernel/lib/printk.h"
#include "kernel/lib/def.h"
#define ASSERT(cond) if (!(cond)) { PANIC("Assert Error"); }

