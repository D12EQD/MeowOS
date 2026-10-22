#include "kernel/proc/process.h"
#include "kernel/proc/lock.h"

#include "kernel/arch/def.h"
#include "kernel/arch/trap.h"

#include "kernel/lib/string.h"
#include "kernel/lib/def.h"
#include "kernel/lib/printk.h"

#include "kernel/mm/def.h"

EXTERN_SYMBOL(trampoline); // 蹦床代码，用于页表映射

Process process_list[PROCESS_MAX_COUNT]; /* 定义进程控制块 */

#define get_cpuid() ({ int id = READ_REG(tp);  id; })

static inline Process* get_current_process(){
    int id = get_cpuid();
    pr_info("cpu id is %d\n", id);
    return &process_list[id];
}

void process_init(){
    for (u64 i = 0; i < PROCESS_MAX_COUNT; i ++) {
        process_list[i].kstack = KERNEL_STACK(i);
        process_list[i].state = PROC_UNUSED;
    }
}

void process_page(){

}

Process *create_process(u64 pc) {

    return NULL;
}
void yield(void){
    (void)0;
}

//
// void schedule(){
//     critical_enter();
//
//     critical_exit();
// }
