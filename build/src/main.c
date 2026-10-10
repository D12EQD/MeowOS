#include "main.h"
// #include "sub_build.h"
#include "pathlist.h"
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>

// #include "arch/build.h"
// #include "mm/build.h"
// #include "lib/build.h"
// #include "kernel/build.h"
#include "sub_build.h"

/* 编译变量 */
#define QEMU "qemu-system-riscv32"
#define LINK_FILE "build/ld/kernel.ld"

/* output */
#define KERNEL_ELF "build/bin/kernel.elf"
#define KERNEL_MAP "build/kernel.map" 

const char *C_FLAG[] = {
    "-std=c23",
    "-O2",
    "-g3",
    "-Wall",
    "-Wextra",
    "--target=riscv32-unknown-elf",
    "-fno-stack-protector",
    "-ffreestanding",
    "-nostdlib",
    "-Isrc"
};

static Nob_Cmd cmd = {0};

bool my_log(const char *color, const char *message) {
    nob_log(NOB_INFO, "%s============== %s ==============%s\n", color, message, ANSI_RESET);
    return false;
}

/* 将flag字符串数组加入cmd中 */
void build_add_flag(Nob_Cmd *cmd_ptr, const char **flag, size_t count){
    for (size_t i = 0; i < count; i++){
        build_add(cmd_ptr, flag[i]);
    }
}

/* 构造一条编译单个 .c -> .o 的命令 */
void build_add_obj(Nob_Cmd *cmd_ptr, const char *input, const char *output){
    build_add(cmd_ptr, CC);
    build_add_flag(cmd_ptr, C_FLAG, C_FLAG_COUNT);
    build_add(cmd_ptr, "-c", input, "-o", output);
}

/* 对列表里每一对 (input[i], output[i]) 生成 .o 并运行 */
void build_run_obj_list(Nob_Cmd *cmd_ptr, const char **input, const char **output, size_t count){
    my_log(ANSI_GREEN, "build_run_obj_list"); 
    nob_log(NOB_INFO,"The count is %d\n", count);
    for (size_t i = 0; i < count; i++){
        build_add_obj(cmd_ptr, input[i], output[i]);
        build_run(cmd_ptr);
    }
}

#define SOURCE_LIST        \
    X(arch)                \
    X(kernel)              \
    X(lib)                 \
    X(mm)

#define OBJ_PREFIX "build/obj"
#define SRC_PREFIX "src"

void main_build(void){
    PathList* obj_list = pathlist_init(512);
    nob_mkdir_if_not_exists(OBJ_PREFIX);

    #define X(name) nob_mkdir_if_not_exists(OBJ_PREFIX "/" #name);
        SOURCE_LIST
    #undef X

    #define X(name) build_##name(obj_list, OBJ_PREFIX, SRC_PREFIX "/" #name);
        SOURCE_LIST
    #undef X

    
    build_add(&cmd, CC);
    build_add_flag(&cmd, C_FLAG, C_FLAG_COUNT);
    build_add(&cmd, "-fuse-ld=lld");
    
    for (int i = 0; i < obj_list->count; i ++){
        build_add(&cmd, obj_list->items[i]);
    }

    build_add(&cmd, 
        "-Wl,-Map=" KERNEL_MAP, 
        "-Wl,-T" LINK_FILE,
        "-o", KERNEL_ELF
    );

    build_run(&cmd);
}

void main_qemu(bool debug) {
    build_add(&cmd,
        QEMU, "-machine", "virt", "-bios", "default", "-nographic", "-serial", "mon:stdio", "--no-reboot", 
        "-kernel", KERNEL_ELF
    );

    if (debug) build_add(&cmd, "-s", "-S");
    build_run(&cmd);
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    const char *program = nob_shift_args(&argc, &argv);
    const char *subcmd = "build";

    if (argc > 0) {
        subcmd = nob_shift_args(&argc, &argv);
    }

    if (strcmp(subcmd, "build") == 0){
        main_build();
    }else if (strcmp(subcmd, "run") == 0){
        main_build();
        main_qemu(false);
    }else if (strcmp(subcmd, "debug") == 0){
        main_build();
        main_qemu(true);
    }else {
        my_log(ANSI_RED, "unknow command\n");
        return 1;
    }
    my_log(ANSI_GREEN, "nob success");
    return 0;
}
