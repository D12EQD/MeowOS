#include "user/main.h"
#include "user/lib/lib.h"

const char * msg = "Hello Kernel\n";
const char * son_msg = "I am son\n";

int user_main(){
    u64 val = fork();
    
    if (val){
        write(1, (void *)msg, strlen(msg));
    }else{
        write(1, (void *)msg, strlen(msg));
    }

    while (1);
    return 0;
}
