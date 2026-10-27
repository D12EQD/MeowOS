#define _GNU_SOURCE

#include "main.h"
#include <nob.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>

/* 编译变量 */
#define QEMU "qemu-system-riscv64"
#define LINK_FILE "src/kernel/kernel.ld"

#define KERNEL_ELF  "build/bin/kernel.elf"
#define KERNEL_MAP  "build/kernel.map"
#define KERNEL_SRC  "src/kernel"

#define USER_SRC    "src/user"
#define USER_OBJ    "src/kernel/user.o"     /* 最终存放在 src/kernel/ 文件夹下 */
#define USER_LD     "src/user/user.ld"
#define USER_ELF    "build/bin/user.elf"
#define USER_BIN    "build/bin/user.bin"

/* global variable */
#define C_FLAG_COUNT sizeof(C_FLAG)/sizeof(C_FLAG[0])
const char *C_FLAG[] = {
    "-std=c23",
    "-O2",
    "-Wall",
    "-Wextra",
    "--target=riscv64-unknown-elf",
    "-fno-stack-protector",
    "-ffreestanding",
    "-nostdlib",
    "-Isrc",
    "-mcmodel=medany",
    "-c",
    "-march=rv64gc",
    "-mabi=lp64d", 
};

#define D_FLAG_COUNT sizeof(D_FLAG)/sizeof(D_FLAG[0])
const char *D_FLAG[] = {
    "-g3",
    "-g"
};

#define ASM_FLAG_COUNT sizeof(ASM_FLAG)/sizeof(ASM_FLAG[0])
const char *ASM_FLAG[] = {
    "-march=rv64gc",
    "-mabi=lp64d",
    "-nostdlib",
    "-ffreestanding",
    "-Wall",
    "-Wextra",
    "-O2",
    "-Isrc",
    "-c",
};

#define LD_FLAG_COUNT sizeof(LD_FLAG) / sizeof(LD_FLAG[0])
const char *LD_FLAG[] = {
    "--target=riscv64-unknown-elf",
    "-march=rv64gc",
    "-mabi=lp64d",
    "-fuse-ld=lld",
    "-nostdlib"
};

static Nob_Cmd cmd = {0};

/* my 动态数组结构体 */
typedef struct {
    const char **items;
    size_t count;
    size_t capacity;
} StringArray;

StringArray cc_flag = {0};
StringArray obj_file = {0};
StringArray asm_flag = {0};
char temp[1024];

/* build function */

/*
 * 编译所有需要的文件为目标文件
 * 该函数需要注入进 nob_walk_dir
 */
static bool compile_obj_file(Nob_Walk_Entry entry){
    if (entry.type != NOB_FILE_REGULAR) return true;
    Nob_String_View path = nob_sv_from_cstr(entry.path);

    struct BuildCtx* ctx = (struct BuildCtx* )(entry.data);
    const char * obj_dir =  ctx->obj_dir;
    const char * src_dir =  ctx->src_dir;

    if (nob_sv_ends_with_cstr(path, ".c")){
        Nob_String_View origin = nob_sv_from_cstr(path.data);
        // 去前缀
        if (!nob_sv_chop_prefix(&path, nob_sv_from_cstr(src_dir))) return true;
        // 去后缀
        if (!nob_sv_chop_suffix(&path, nob_sv_from_cstr(".c"))) return true;
        
        // 暂存
        sprintf(temp, "%s%.*s.o", obj_dir, (int)path.count, path.data);

        // 启动编译
        build_add_c_to_obj(&cmd, origin.data, temp);
        build_run(&cmd);
    }else if (nob_sv_ends_with_cstr(path, ".S")){
        Nob_String_View origin = nob_sv_from_cstr(path.data);
        // 去前缀
        if (!nob_sv_chop_prefix(&path, nob_sv_from_cstr(src_dir))) return true;
        // 去后缀
        if (!nob_sv_chop_suffix(&path, nob_sv_from_cstr(".S"))) return true;
        
        // 暂存
        sprintf(temp, "%s%.*s.o", obj_dir, (int)path.count, path.data);

        // 启动编译
        build_add_asm_to_obj(&cmd, origin.data, temp);
        build_run(&cmd);
    }

    return true;
}

static bool nob_find_obj_file(Nob_Walk_Entry entry){
    if (entry.type != NOB_FILE_REGULAR) return true;
    Nob_String_View path = nob_sv_from_cstr(entry.path);
    
    if (nob_sv_ends_with_cstr(path, ".o")){
        nob_da_append(&obj_file, nob_temp_strdup(entry.path));
    }

    return true;
}


/*
 * 遍历该文件夹下的目录
 * 将所有目标文件加入obj_file
 */
void add_obj_file(const char *obj_dir){
    nob_walk_dir(obj_dir, nob_find_obj_file);
}


/*
* 递归遍历目录，将所有 .c 文件编译为 .o
* args:
*   src_dir 源文件目录
*   obj_dir 目标输出目录
*/
void build_object_file(const char *src_dir, const char *obj_dir) {
    struct BuildCtx ctx = {
        .obj_dir = obj_dir,
        .src_dir = src_dir
    };

    nob_walk_dir(src_dir, compile_obj_file, .data = &ctx);
}

bool notice(const char *color, const char *message) {
    nob_log(NOB_INFO, "%s============== %s ==============%s\n", color, message, ANSI_RESET);
    return false;
}

/* 构造一条编译单个 .c -> .o 的命令 */
void build_add_c_to_obj(Nob_Cmd *cmd_ptr, const char *input, const char *output){
    nob_cmd_append(cmd_ptr, CC);
    for (int i = 0; i < cc_flag.count; i ++){
        nob_cmd_append(cmd_ptr, cc_flag.items[i]);
    }
    nob_cmd_append(cmd_ptr, "-c", input, "-o", output);
}

/* 构建一条编译单个 .S -> .o 的命令 */
void build_add_asm_to_obj(Nob_Cmd *cmd_ptr, const char *input, const char *output){
    nob_cmd_append(cmd_ptr, ASM_CC);
    for (int i = 0; i < asm_flag.count; i ++){
        nob_cmd_append(cmd_ptr, asm_flag.items[i]);
    }
    nob_cmd_append(cmd_ptr, input, "-o", output);
}

/*
 * 用户代码构建函数
 * args:
 *      input_dir 输出文件夹
 *      obj_dir 存放目标文件的文件夹
 *      result_obj_path 最后将所有目标文件导出的最终路径
 */
void main_build_user(const char *input_dir, const char *obj_dir){
    // 1. 编译目标文件
    build_object_file(input_dir, obj_dir);

    // 2. 构建elf
    add_obj_file(obj_dir);
    
    nob_cmd_append(&cmd, CC);
    for (int i = 0; i < LD_FLAG_COUNT; i ++){
        nob_cmd_append(&cmd, LD_FLAG[i]);
    }
    for (int i = 0; i < obj_file.count; i ++){
        nob_cmd_append(&cmd, obj_file.items[i]);
    }

    nob_cmd_append(&cmd, "-Wl,-T", USER_LD, "-o", USER_ELF);
    build_run(&cmd);
    
    // 3. 生成 raw binary -- user.bin
    nob_cmd_append(
        &cmd, 
        "riscv64-linux-gnu-objcopy", 
        "-O", "binary", 
        USER_ELF, USER_BIN
    );
    build_run(&cmd);
}


/* 
 * 主构建函数 
 * args:
 * is_debug: 是否开启编译debug功能
 * argc argv : 指定需要编译的目录
 */
void main_build(bool is_debug, int argc, char ** argv){
    // 1. 添加编译标志
    for (int i = 0; i < C_FLAG_COUNT; i ++) nob_da_append(&cc_flag, C_FLAG[i]);
    for (int i = 0; i < ASM_FLAG_COUNT; i ++) nob_da_append(&asm_flag, ASM_FLAG[i]);

    if (is_debug){
        for (int i = 0; i < D_FLAG_COUNT; i ++) nob_da_append(&cc_flag, D_FLAG[i]);
        for (int i = 0; i < D_FLAG_COUNT; i ++) nob_da_append(&cc_flag, D_FLAG[i]);
    }

    
    // object file and code file from the same dir

    notice(ANSI_GREEN, "Compiling source files...");

    if (argc > 0){
        const char * dir = KERNEL_SRC;
        // 如果有输入参数，则根据输入参数的目录编译
        Nob_String_View str_src = nob_sv_from_cstr(dir);
        for (int i = 0; i < argc; i ++){
            if (nob_file_exists(argv[i]) == false) continue;

            Nob_String_View str_arg = nob_sv_from_cstr(argv[i]);
            
            if (nob_sv_starts_with(str_arg, str_src)){
                build_object_file(argv[i], argv[i]);
            }
        }
    }else{
        build_object_file(KERNEL_SRC, KERNEL_SRC);
    }
    
    // 同时编译用户代码为目标文件
    main_build_user(USER_SRC, USER_SRC);

    notice(ANSI_GREEN, "Linking kernel...");

    // 2. 编译
    add_obj_file(KERNEL_SRC);

    // 3. 链接阶段
    nob_cmd_append(&cmd, CC);
    for (int i = 0; i < LD_FLAG_COUNT; i ++) nob_cmd_append(&cmd, LD_FLAG[i]);
    nob_cmd_append(&cmd, 
        "-Wl,-Map=" KERNEL_MAP, 
        "-Wl,-T" LINK_FILE,
        "-o", KERNEL_ELF
    );

    // 4. 加入目标为输入文件
    for (int i = 0; i < obj_file.count; i ++){
        nob_cmd_append(&cmd, obj_file.items[i]);
    }
    build_run(&cmd);
}

void main_qemu(bool is_debug) {
    Nob_Cmd qemu_cmd = {0};
    nob_cmd_append(&qemu_cmd,
        QEMU, "-machine", "virt", "-bios", "default", "-nographic", "-serial", "mon:stdio", "--no-reboot", 
        "-kernel", KERNEL_ELF
    );

    if (is_debug) nob_cmd_append(&qemu_cmd, "-s", "-S");
    build_run(&qemu_cmd);
}

void main_clean(int argc, char **argv){
    struct BuildCtx ctx = {
        .obj_dir = "src",
        .src_dir = NULL
    };

    nob_walk_dir("src", nob_find_obj_file, .data = &ctx); 
    if (obj_file.count == 0){
        nob_log(NOB_WARNING, "no obj file\n");
        return;
    }

    nob_cmd_append(&cmd, "rm");
    for (int i = 0; i < obj_file.count; i ++){
        nob_cmd_append(&cmd, obj_file.items[i]);
    } 
    build_run(&cmd);
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF_PLUS(argc, argv,
        "build/src/nob.c"
    );

    const char *program = nob_shift_args(&argc, &argv);
    const char *subcmd = "build";

    if (argc > 0) {
        subcmd = nob_shift_args(&argc, &argv);
    }

    if (strcmp(subcmd, "build") == 0){
        main_build(false, argc, argv);
    }else if (strcmp(subcmd, "run") == 0){
        main_build(false, argc, argv);
        main_qemu(false);
    }else if (strcmp(subcmd, "debug") == 0){
        main_build(true, argc, argv);
        main_qemu(true);
    }else if (strcmp(subcmd, "clean") == 0){
        main_clean(argc, argv);
    }else{
        notice(ANSI_RED, "unknow command\n");
        return 1;
    }
    notice(ANSI_GREEN, "nob success");
    return 0;
}
