#include "user/main.h"
#include "user/lib/lib.h"
#include "user/lib/print.h"
#include "user/test/test.h"

int val[10];

int user_main(){
    println("hello world");

    test_run_all();
    println("tests done, halting");
 
    while (1){
        _ASM("nop");
    }

    return 0;
}
