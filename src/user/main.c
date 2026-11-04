#include "user/main.h"
#include "user/lib/lib.h"
#include "user/lib/print.h"
#include "user/test/test.h"

int user_main(){
    test_run_all();

    // 根进程不能返回（kexit 里没有父进程会 PANIC），
    // 用户态也没有 halt/wfi 可用（wfi 会被当成非法指令），所以只能空转
    println("tests done, halting");
    for (;;) {
        __asm__ volatile ("nop");
    }

    return 0;
}
