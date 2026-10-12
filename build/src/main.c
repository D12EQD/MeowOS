#define _GNU_SOURCE
#include "main.h"
#include "flaglist.h"
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>

/* 编译变量 */
#define QEMU "qemu-system-riscv64"
#define LINK_FILE "build/ld/kernel.ld"

/* output */
#define KERNEL_ELF "build/bin/kernel.elf"
#define KERNEL_MAP "build/kernel.map"

/* global variable */
#define C_FLAG_COUNT sizeof(C_FLAG)/sizeof(C_FLAG[0])
const char *C_FLAG[] = {
    "-std=c23",
    "-O2",
    "-g3",
    "-Wall",
    "-Wextra",
    "--target=riscv64-unknown-elf",
    "-fno-stack-protector",
    "-ffreestanding",
    "-nostdlib",
    "-Isrc",
    "-mcmodel=medany",
};

#define D_FLAG_COUNT sizeof(D_FLAG)/sizeof(D_FLAG[0])
const char *D_FLAG[] = {
    "-g",
};

static Nob_Cmd cmd = {0};
FlagList* cc_flag;
FlagList* obj_file;
char temp[1024];

/* build function */
static bool get_target_file(Nob_Walk_Entry entry){
    if (entry.type != NOB_FILE_REGULAR) return true;
    Nob_String_View path = nob_sv_from_cstr(entry.path);

    
    if (nob_sv_end_with(path, ".c")){
        // TODO : 可能会有asm
        struct BuildCtx* ctx = (struct BuildCtx* )(entry.data);
        const char * obj_dir =  ctx->obj_dir;
        const char * src_dir =  ctx->src_dir;

        Nob_String_View origin = nob_sv_from_cstr(path.data);
        // 去前缀
        if (!nob_sv_chop_prefix(&path, nob_sv_from_cstr(src_dir))) return true;

        // 去后缀
        if (!nob_sv_chop_suffix(&path, nob_sv_from_cstr(".c"))) return true;
        
        // temp 保存 .o 文件
        size_t temp_len = sprintf(temp, "%s%.*s.o\0", obj_dir, (int)path.count, path.data);
        
        // 启动编译
        build_add_obj(&cmd, origin.data, temp);
        build_run(&cmd);
        // printf("%s -> %s\n", origin.data, temp);

        // .o文件保存
        flaglist_add(obj_file, temp, temp_len);
    }

    return true;
}
/*
* 递归遍历目录，将所有 .c 文件编译为 .o
* args:
*   src_dir 源文件目录，一般为src/
*   obj_dir 目标输出目录，一般也是src/
*/
void build_recursive(const char *src_dir, const char *obj_dir) {
    struct BuildCtx ctx = {
        .obj_dir = obj_dir,
        .src_dir = src_dir
    };

    nob_walk_dir(src_dir, get_target_file, .data = &ctx);
}

/* function */
bool my_log(const char *color, const char *message) {
    nob_log(NOB_INFO, "%s============== %s ==============%s\n", color, message, ANSI_RESET);
    return false;
}

void build_add_flaglist(Nob_Cmd *cmd_ptr, FlagList * fl){
    int count = fl->count;
    for (int i = 0; i < count; i ++){
        build_add(cmd_ptr, fl->items[i]);
    }
}

/* 将flag字符串数组加入cmd中 */
void build_add_str(Nob_Cmd *cmd_ptr, const char **flag, size_t count){
    for (size_t i = 0; i < count; i++){
        build_add(cmd_ptr, flag[i]);
    }
}

/* 构造一条编译单个 .c -> .o 的命令 */
void build_add_obj(Nob_Cmd *cmd_ptr, const char *input, const char *output){
    build_add(cmd_ptr, CC);
    build_add_flaglist(cmd_ptr, cc_flag);
    build_add(cmd_ptr, "-c", input, "-o", output);
}

/* 主构建函数 */
void main_build(bool is_debug){
    obj_file = flaglist_init(1024);
    cc_flag = flaglist_init(1024);
    
    // 添加编译标志
    for (int i = 0; i < C_FLAG_COUNT; i ++){
        flaglist_add(cc_flag, C_FLAG[i], strlen(C_FLAG[i]));
    }

    if (is_debug){
        for (int i = 0; i < D_FLAG_COUNT; i ++){
            flaglist_add(cc_flag, D_FLAG[i], strlen(D_FLAG[i]));
        }
    }
    
    // 编译
    my_log(ANSI_GREEN, "Compiling source files...");
    build_recursive("src", "src");
    my_log(ANSI_GREEN, "Linking kernel...");
    
    // 链接阶段
    build_add(&cmd, CC);
    build_add_flaglist(&cmd, cc_flag);
    build_add(&cmd, "-fuse-ld=lld");
    build_add(&cmd, 
        "-Wl,-Map=" KERNEL_MAP, 
        "-Wl,-T" LINK_FILE,
        "-o", KERNEL_ELF
    );

    // 加入目标为输入文件
    build_add_flaglist(&cmd, obj_file);
    build_run(&cmd);
}

void main_qemu(bool is_debug) {
    Nob_Cmd qemu_cmd = {0};
    build_add(&qemu_cmd,
        QEMU, "-machine", "virt", "-bios", "default", "-nographic", "-serial", "mon:stdio", "--no-reboot", 
        "-kernel", KERNEL_ELF
    );

    if (is_debug) build_add(&qemu_cmd, "-s", "-S");
    build_run(&qemu_cmd);
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF_PLUS(argc, argv,
        "build/src/nob.c",
        "build/src/flaglist.c"
    );

    const char *program = nob_shift_args(&argc, &argv);
    const char *subcmd = "build";

    if (argc > 0) {
        subcmd = nob_shift_args(&argc, &argv);
    }

    if (strcmp(subcmd, "build") == 0){
        main_build(false);
    }else if (strcmp(subcmd, "run") == 0){
        main_build(false);
        main_qemu(false);
    }else if (strcmp(subcmd, "debug") == 0){
        main_build(true);
        main_qemu(true);
    }else {
        my_log(ANSI_RED, "unknow command\n");
        return 1;
    }
    my_log(ANSI_GREEN, "nob success");
    return 0;
}
