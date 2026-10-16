#pragma once
#include <nob.h>
#include <assert.h>

#define ANSI_RED   "\033[31m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_GREEN "\033[32m"
#define ANSI_RESET "\033[0m"

#define CC "clang"
#define ASM_CC "riscv64-linux-gnu-gcc"

#define LOC  __FILE__ ":"

void build_elf_file(const char *obj_dir);
void build_object_file(const char *src_dir,const char *obj_dir);
bool notice(const char *color,const char *message);

void build_add_c_to_obj(Nob_Cmd *cmd_ptr,const char *input,const char *output);
void build_add_asm_to_obj(Nob_Cmd *cmd_ptr, const char *input, const char *output);

void main_build(bool is_debug,int argc,char **argv);
void main_qemu(bool is_debug);
bool notice(const char *color, const char *message);

#define build_run(cmd_ptr, ...) assert(          \
    nob_cmd_run(cmd_ptr, ##__VA_ARGS__) ||   \
    (notice(ANSI_RED, LOC "Build Error"))  \
)

struct BuildCtx{
    const char * obj_dir;
    const char * src_dir;
};
