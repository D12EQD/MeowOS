#include "kernel/proc/process.h"
#include "kernel/lib/string.h"
#include "kernel/lib/def.h"
#include "kernel/lib/printk.h"

struct process process_list[PROCESS_MAX_COUNT]; /* 定义进程控制块 */
struct process *current_proc = NULL;            /* 当前运行的进程 */
struct process *idle_proc = NULL;               /* 当前空闲进程 */

struct process *create_process(u64 pc) {
    struct process *proc = NULL;
    int i;
    for (i = 0; i < PROCESS_MAX_COUNT; i++) {
        if (process_list[i].state == PROC_UNUSED) {
            proc = &process_list[i];
            break;
        }
    }

    if (!proc){
        pr_info("no free process slots");
        return NULL;
    }
    
    return NULL;
}
