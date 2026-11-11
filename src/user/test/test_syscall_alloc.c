#include "user/os/syscall.h"
#include "user/test/test.h"

void test_page_alloc_and_free(TestCtx* t){
    test_case(t, "test_page_alloc_and_free");

    int val[] = {1,2,3,4,5,6,7,8,9,10};
    int n = sizeof(val) / sizeof(int);

    for (int i = 0; i < n; i ++){
        void *page = palloc(val[i]);
        memset(page, 1, val[i] * PAGE_SIZE);
    }
    
    for (int i = 0; i < n; i ++){
        pfree(val[i]);
    }

    for (int i = 0; i < n; i ++){
        void *page = palloc(val[i]);
        memset(page, 1, val[i] * PAGE_SIZE);
    }
    
    for (int i = n - 1; i >= 0; i --){
        pfree(val[i]);
    }


    t->pass ++;
}
