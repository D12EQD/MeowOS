#define _GNU_SOURCE

#define NOB_IMPLEMENTATION
#define NOB_WARN_DEPRECATED
#define NOB_EXPERIMENTAL_DELETE_OLD

#include <nob.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

/* ==================== 颜色定义 ==================== */
#define ANSI_RED    "\033[31m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_GREEN  "\033[32m"
#define ANSI_RESET  "\033[0m"

/* ==================== 工具链 ==================== */
#define CC     "clang"
#define ASM_CC "riscv64-linux-gnu-gcc"
#define QEMU   "qemu-system-riscv64"

/* ==================== 构建路径 ==================== */
#define LINK_FILE  "src/kernel/kernel.ld"

#define KERNEL_ELF "build/bin/kernel.elf"
#define KERNEL_MAP "build/kernel.map"
#define KERNEL_SRC "src/kernel"

#define USER_SRC   "src/user"
#define USER_OBJ   "src/kernel/user.o"     /* 最终存放在 src/kernel/ 文件夹下 */
#define USER_LD    "src/user/user.ld"
#define USER_ELF   "build/bin/user.elf"
#define USER_BIN   "build/bin/user.bin"

/* ==================== 日志辅助 ==================== */
#define LOC __FILE__ ":"

/* ==================== 数据类型 ==================== */
typedef struct {
    const char **items;
    size_t count;
    size_t capacity;
} StringArray;

typedef struct {
    const char *obj_dir;
    const char *src_dir;
} BuildCtx;

/* ==================== 全局变量 ==================== */
static Nob_Cmd    cmd      = {0};
static StringArray cc_flag  = {0};
static StringArray asm_flag = {0};
static StringArray obj_file = {0};
static char       temp[1024];

/* ==================== 编译标志 ==================== */
static const char *C_FLAG[] = {
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
#define C_FLAG_COUNT (sizeof(C_FLAG) / sizeof(C_FLAG[0]))

static const char *D_FLAG[] = {
    "-g3",
    "-g",
};
#define D_FLAG_COUNT (sizeof(D_FLAG) / sizeof(D_FLAG[0]))

static const char *ASM_FLAG[] = {
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
#define ASM_FLAG_COUNT (sizeof(ASM_FLAG) / sizeof(ASM_FLAG[0]))

static const char *LD_FLAG[] = {
    "--target=riscv64-unknown-elf",
    "-march=rv64gc",
    "-mabi=lp64d",
    "-fuse-ld=lld",
    "-nostdlib",
};
#define LD_FLAG_COUNT (sizeof(LD_FLAG) / sizeof(LD_FLAG[0]))

bool notice(const char *color, const char *message);

void build_object_file(const char *src_dir, const char *obj_dir);
void build_add_c_to_obj(Nob_Cmd *cmd_ptr, const char *input, const char *output);
void build_add_asm_to_obj(Nob_Cmd *cmd_ptr, const char *input, const char *output);

void main_build_user(const char *input_dir, const char *obj_dir);
void main_build(bool is_debug, int argc, char **argv);
void main_qemu(bool is_debug);
void main_clean(int argc, char **argv);

/* 运行命令，失败时打印错误并触发断言 */
#define build_run(cmd_ptr, ...) \
    assert(nob_cmd_run(cmd_ptr, ##__VA_ARGS__) || (notice(ANSI_RED, LOC "Build Error")))

/* ==================== 工具函数 ==================== */
bool notice(const char *color, const char *message) {
    nob_log(NOB_INFO, "%s============== %s ==============%s\n",
            color, message, ANSI_RESET);
    return false;
}

/* 构造一条编译单个 .c -> .o 的命令 */
void build_add_c_to_obj(Nob_Cmd *cmd_ptr, const char *input, const char *output) {
    nob_cmd_append(cmd_ptr, CC);
    for (size_t i = 0; i < cc_flag.count; i++) {
        nob_cmd_append(cmd_ptr, cc_flag.items[i]);
    }
    nob_cmd_append(cmd_ptr, "-c", input, "-o", output);
}

/* 构造一条编译单个 .S -> .o 的命令 */
void build_add_asm_to_obj(Nob_Cmd *cmd_ptr, const char *input, const char *output) {
    nob_cmd_append(cmd_ptr, ASM_CC);
    for (size_t i = 0; i < asm_flag.count; i++) {
        nob_cmd_append(cmd_ptr, asm_flag.items[i]);
    }
    nob_cmd_append(cmd_ptr, input, "-o", output);
}

/* ==================== 目录遍历回调 ==================== */
static bool compile_obj_file(Nob_Walk_Entry entry) {
    if (entry.type != NOB_FILE_REGULAR) return true;

    Nob_String_View path = nob_sv_from_cstr(entry.path);
    BuildCtx *ctx = entry.data;

    if (nob_sv_ends_with_cstr(path, ".c")) {
        Nob_String_View origin = path;
        if (!nob_sv_chop_prefix(&path, nob_sv_from_cstr(ctx->src_dir))) return true;
        if (!nob_sv_chop_suffix(&path, nob_sv_from_cstr(".c"))) return true;

        snprintf(temp, sizeof(temp), "%s%.*s.o",
                 ctx->obj_dir, (int)path.count, path.data);
        build_add_c_to_obj(&cmd, origin.data, temp);
        build_run(&cmd);
    } else if (nob_sv_ends_with_cstr(path, ".S")) {
        Nob_String_View origin = path;
        if (!nob_sv_chop_prefix(&path, nob_sv_from_cstr(ctx->src_dir))) return true;
        if (!nob_sv_chop_suffix(&path, nob_sv_from_cstr(".S"))) return true;

        snprintf(temp, sizeof(temp), "%s%.*s.o",
                 ctx->obj_dir, (int)path.count, path.data);
        build_add_asm_to_obj(&cmd, origin.data, temp);
        build_run(&cmd);
    }

    return true;
}

static bool nob_find_obj_file(Nob_Walk_Entry entry) {
    if (entry.type != NOB_FILE_REGULAR) return true;

    Nob_String_View path = nob_sv_from_cstr(entry.path);
    if (nob_sv_ends_with_cstr(path, ".o")) {
        nob_da_append(&obj_file, nob_temp_strdup(entry.path));
    }

    return true;
}

/* 遍历目录，将所有 .o 文件加入 obj_file */
void add_obj_file(const char *obj_dir) {
    nob_walk_dir(obj_dir, nob_find_obj_file);
}

/* ==================== 构建函数 ==================== */
/* 递归编译目录下所有 .c / .S 为 .o */
void build_object_file(const char *src_dir, const char *obj_dir) {
    BuildCtx ctx = {
        .obj_dir = obj_dir,
        .src_dir = src_dir,
    };
    nob_walk_dir(src_dir, compile_obj_file, .data = &ctx);
}

/* 构建用户程序：源码 -> .o -> elf -> raw binary */
void main_build_user(const char *input_dir, const char *obj_dir) {
    /* 1. 编译目标文件 */
    build_object_file(input_dir, obj_dir);

    /* 2. 构建 elf */
    add_obj_file(obj_dir);

    nob_cmd_append(&cmd, CC);
    for (size_t i = 0; i < LD_FLAG_COUNT; i++) {
        nob_cmd_append(&cmd, LD_FLAG[i]);
    }
    for (size_t i = 0; i < obj_file.count; i++) {
        nob_cmd_append(&cmd, obj_file.items[i]);
    }
    nob_cmd_append(&cmd, "-Wl,-T", USER_LD, "-o", USER_ELF);
    build_run(&cmd);

    /* 3. 生成 raw binary */
    nob_cmd_append(&cmd,
                   "riscv64-linux-gnu-objcopy",
                   "-O", "binary",
                   USER_ELF, USER_BIN);
    build_run(&cmd);
}

void main_build(bool is_debug, int argc, char **argv) {
    /* 1. 收集编译标志 */
    for (size_t i = 0; i < C_FLAG_COUNT; i++) {
        nob_da_append(&cc_flag, C_FLAG[i]);
    }
    for (size_t i = 0; i < ASM_FLAG_COUNT; i++) {
        nob_da_append(&asm_flag, ASM_FLAG[i]);
    }

    if (is_debug) {
        for (size_t i = 0; i < D_FLAG_COUNT; i++) {
            nob_da_append(&cc_flag, D_FLAG[i]);
        }
        for (size_t i = 0; i < D_FLAG_COUNT; i++) {
            nob_da_append(&asm_flag, D_FLAG[i]);
        }
    }

    notice(ANSI_GREEN, "Compiling source files...");

    /* 2. 编译内核源码 */
    if (argc > 0) {
        Nob_String_View str_src = nob_sv_from_cstr(KERNEL_SRC);
        for (int i = 0; i < argc; i++) {
            if (!nob_file_exists(argv[i])) continue;

            Nob_String_View str_arg = nob_sv_from_cstr(argv[i]);
            if (nob_sv_starts_with(str_arg, str_src)) {
                build_object_file(argv[i], argv[i]);
            }
        }
    } else {
        build_object_file(KERNEL_SRC, KERNEL_SRC);
    }

    /* 3. 编译用户程序 */
    main_build_user(USER_SRC, USER_SRC);

    /* 4. 链接内核 */
    notice(ANSI_GREEN, "Linking kernel...");
    add_obj_file(KERNEL_SRC);

    nob_cmd_append(&cmd, CC);
    for (size_t i = 0; i < LD_FLAG_COUNT; i++) {
        nob_cmd_append(&cmd, LD_FLAG[i]);
    }
    nob_cmd_append(&cmd,
                   "-Wl,-Map=" KERNEL_MAP,
                   "-Wl,-T" LINK_FILE,
                   "-o", KERNEL_ELF);
    for (size_t i = 0; i < obj_file.count; i++) {
        nob_cmd_append(&cmd, obj_file.items[i]);
    }
    build_run(&cmd);
}

void main_qemu(bool is_debug) {
    Nob_Cmd qemu_cmd = {0};
    nob_cmd_append(&qemu_cmd,
                   QEMU,
                   "-machine", "virt",
                   "-bios", "default",
                   "-nographic",
                   "-serial", "mon:stdio",
                   "--no-reboot",
                   "-kernel", KERNEL_ELF);

    if (is_debug) {
        nob_cmd_append(&qemu_cmd, "-s", "-S");
    }
    build_run(&qemu_cmd);
}

void main_clean(int argc, char **argv) {
    (void)argc;
    (void)argv;

    nob_walk_dir("src", nob_find_obj_file);
    if (obj_file.count == 0) {
        nob_log(NOB_WARNING, "no obj file\n");
        return;
    }

    nob_cmd_append(&cmd, "rm");
    for (size_t i = 0; i < obj_file.count; i++) {
        nob_cmd_append(&cmd, obj_file.items[i]);
    }
    build_run(&cmd);
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    nob_shift_args(&argc, &argv);  /* 丢弃程序名 */
    const char *subcmd = "build";

    if (argc > 0) {
        subcmd = nob_shift_args(&argc, &argv);
    }

    if (strcmp(subcmd, "build") == 0) {
        main_build(false, argc, argv);
    } else if (strcmp(subcmd, "run") == 0) {
        main_build(false, argc, argv);
        main_qemu(false);
    } else if (strcmp(subcmd, "debug") == 0) {
        main_build(true, argc, argv);
        main_qemu(true);
    } else if (strcmp(subcmd, "clean") == 0) {
        main_clean(argc, argv);
    } else {
        notice(ANSI_RED, "unknown command");
        return 1;
    }

    notice(ANSI_GREEN, "nob success");
    return 0;
}
