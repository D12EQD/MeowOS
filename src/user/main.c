#include "user/main.h"
#include "user/lib/lib.h"
#include "user/lib/print.h"

const char * msg = "Hello Kernel\n";
const char * son_msg = "I am son\n";

int user_main(){
    u64 val = fork();

    if (val){
        println("I am fa");
        int pid = wait(&val);
        println("wait the pid = %d, return value = %d", pid, val);
    }else{
        println("I am son");
        return 69;
    }

    return 0;
}
