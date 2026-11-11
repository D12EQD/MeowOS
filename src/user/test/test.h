#pragma once
#include "user/def.h"
#include "user/lib/lib.h"
#include "user/lib/print.h"
#include "user/os/syscall.h"

/* 用户态测试 */
typedef struct {
    u32 total;
    u32 pass;
    u32 fail;
    u32 note;
} TestCtx;

void test_run_all(void);

void test_ctx_init(TestCtx *t);
void test_case(TestCtx *t, const char *name);
void test_expect(TestCtx *t, const char *what, i64 got, i64 want);
void test_note(TestCtx *t, const char *what);
void test_summary(TestCtx *t);

void test_write_suite(TestCtx *t);
void test_proc_suite(TestCtx *t);
void test_page_alloc_and_free(TestCtx* t);
void test_global_data_suite(TestCtx *t);

void test_fs(TestCtx *t);
