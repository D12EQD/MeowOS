#define _GNU_SOURCE

#define NOB_IMPLEMENTATION
#define NOB_WARN_DEPRECATED
#define NOB_EXPERIMENTAL_DELETE_OLD

#include <nob.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

/* ================ 颜色 ================ */
#define ANSI_RED    "\033[31m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_GREEN  "\033[32m"
#define ANSI_RESET  "\033[0m"

/* ================ 工具链 ================ */
#define CC      "clang"
#define ASM_CC  "riscv64-linux-gnu-gcc"
#define OBJCOPY "riscv64-linux-gnu-objcopy"
#define QEMU    "qemu-system-riscv64"

/* ================ 路径 ================ */
#define LINK_FILE  "src/kernel/kernel.ld"
#define KERNEL_ELF "build/bin/kernel.elf"
#define KERNEL_MAP "build/kernel.map"
#define KERNEL_SRC "src/kernel"

#define USER_SRC "src/user"
#define USER_LD  "src/user/user.ld"
#define USER_ELF "build/bin/user.elf"
#define USER_BIN "build/bin/user.bin"

#define MMODE_LD    "src/mmode/mmode.ld"
#define MMODE_SRC   "src/mmode"
#define MMODE_BIN   "build/bin/mmode.bin"
#define MMODE_ELF   "build/bin/mmode.elf"
#define OPEN_SBI_LIB "src/mmode/libplatsbi.a"

#define MAX_PROC 4

/* ================ 编译标志 ================ */
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

static const char *D_FLAG[] = { "-g3", "-g" };
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

static const char *MMODE_ASM_FLAG[] = {
    "-march=rv64gc",
    "-mabi=lp64d",
    "-mcmodel=medany",
    "-mno-relax",
    "-mno-save-restore",
    "-mstrict-align",
    "-nostdlib", "-ffreestanding",
    "-Wall", "-Wextra", "-O2",
    "-Isrc",
};

static const char *MMODE_LD_FLAG[] = {
    "--target=riscv64-unknown-elf",
    "-march=rv64gc",
    "-mabi=lp64d",
    "-mno-relax",
    "-fuse-ld=lld", "-nostdlib",
    "-Wl,--build-id=none",
    "-Wl,--gc-sections",
};

/* ================ 类型 ================ */
typedef void (*BuildFn)(Nob_Cmd *cmd, const char *input, const char *output);

typedef struct {
    Nob_String_View suffix;
    BuildFn         build;
} FileRule;

typedef struct {
    FileRule *items;
    size_t    count;
    size_t    capacity;
} FileRules;

typedef struct {
    const char *src_dir;
    const char *obj_dir;
    const char *ld_script;
    Nob_Cmd     extra_libs;
    const char *out_elf;
    const char *out_bin;
    const char *map_path;
} BuildTarget;

/* ================ 全局状态 ================ */
Nob_Cmd    g_cc_flag  = {0};
Nob_Cmd    g_asm_flag = {0};
FileRules  g_rules    = {0};
Nob_Procs  g_procs    = {0};
Nob_Cmd    g_objs     = {0};
Nob_Cmd    g_ld_flag  = {0};

/* ================ 辅助 ================ */
static void notice(const char *color, const char *msg) {
    nob_log(NOB_INFO, "%s============== %s ==============%s", color, msg, ANSI_RESET);
}


/* ================ 注入函数 ================ */
void build_c(Nob_Cmd *cmd, const char *in, const char *out) {
    nob_cmd_append(cmd, CC);
    nob_cmd_extend(cmd, &g_cc_flag);
    nob_cmd_append(cmd, "-c", in, "-o", out);
}

void build_asm(Nob_Cmd *cmd, const char *in, const char *out) {
    nob_cmd_append(cmd, ASM_CC);
    nob_cmd_extend(cmd, &g_asm_flag);
    nob_cmd_append(cmd, "-c", in, "-o", out);
}

/* ================ 链接 / 产物（独立接口） ================ */
void build_link(Nob_Cmd *cmd, const BuildTarget *t, const Nob_Cmd *objs) {
    nob_cmd_append(cmd, CC);
    for (size_t i = 0; i < g_ld_flag.count; i++) {
        nob_cmd_append(cmd, g_ld_flag.items[i]);
    }
    nob_cmd_extend(cmd, objs);
    for (size_t i = 0; i < t->extra_libs.count; i++) {
        nob_cmd_append(cmd, t->extra_libs.items[i]);
    }
    if (t->map_path) {
        nob_cmd_append(cmd, nob_temp_sprintf("-Wl,-Map=%s", t->map_path));
    }
    nob_cmd_append(cmd, "-Wl,-T", t->ld_script, "-o", t->out_elf);
}

void build_objcopy(Nob_Cmd *cmd, const char *elf, const char *bin) {
    nob_cmd_append(cmd, OBJCOPY, "-O", "binary", elf, bin);
}

/* ================ 规则表 ================ */
void rules_init(bool is_debug) {
    for (size_t i = 0; i < C_FLAG_COUNT; i++) {
        nob_da_append(&g_cc_flag, C_FLAG[i]);
    }
    for (size_t i = 0; i < ASM_FLAG_COUNT; i++) {
        nob_da_append(&g_asm_flag, ASM_FLAG[i]);
    }
    if (is_debug) {
        for (size_t i = 0; i < D_FLAG_COUNT; i++) {
            nob_da_append(&g_cc_flag, D_FLAG[i]);
            nob_da_append(&g_asm_flag, D_FLAG[i]);
        }
    }

    for (size_t i = 0; i < LD_FLAG_COUNT; i ++){
        nob_da_append(&g_ld_flag, LD_FLAG[i]);
    }

    FileRule rule_c = { .suffix = nob_sv_from_cstr(".c"), .build = build_c };
    nob_da_append(&g_rules, rule_c);

    FileRule rule_S = { .suffix = nob_sv_from_cstr(".S"), .build = build_asm };
    nob_da_append(&g_rules, rule_S);
}

bool flag_replace(Nob_Cmd* cmd, const char *old, const char * new){
    for (size_t i = 0; i < cmd->count; i ++){
        if (strcmp(cmd->items[i], old) == 0){
            cmd->items[i] = new;
            return true;
        }
    }
    return false;
}

BuildFn rule_for(Nob_String_View path) {
    for (size_t i = 0; i < g_rules.count; i++) {
        if (nob_sv_ends_with(path, g_rules.items[i].suffix)) {
            return g_rules.items[i].build;
        }
    }
    return NULL;
}

/* ================ 路径映射 ================ */
char *src_to_obj(const char *src_root, const char *obj_root, const char *src_path) {
    Nob_String_View sv = nob_sv_from_cstr(src_path);
    if (!nob_sv_chop_prefix(&sv, nob_sv_from_cstr(src_root))) return NULL;

    /* 找最后一个 '/' 之后的 '.' */
    size_t i = sv.count;
    while (i > 0 && sv.data[i-1] != '.') {
        if (sv.data[i-1] == '/') return NULL;
        i--;
    }
    if (i == 0) return NULL;

    size_t stem_count = i - 1;
    return nob_temp_sprintf("%s%.*s.o", obj_root, (int)stem_count, sv.data);
}

/* ================ walk 回调 ================ */
bool compile_walk(Nob_Walk_Entry e) {
    if (e.type != NOB_FILE_REGULAR) return true;

    const BuildTarget *t = e.data;
    BuildFn build = rule_for(nob_sv_from_cstr(e.path));
    if (build == NULL) return true;

    char *obj = src_to_obj(t->src_dir, t->obj_dir, e.path);
    if (obj == NULL) {
        nob_log(NOB_ERROR, "cannot map `%s` to obj path", e.path);
        return false;
    }

    Nob_Cmd c = {0};
    build(&c, e.path, obj);
    bool ok = nob_cmd_run(&c, .async = &g_procs, .max_procs = MAX_PROC);
    nob_cmd_free(c);
    return ok;
}

bool collect_obj_walk(Nob_Walk_Entry e) {
    if (e.type != NOB_FILE_REGULAR) return true;
    if (!nob_sv_ends_with_cstr(nob_sv_from_cstr(e.path), ".o")) return true;
    nob_da_append(&g_objs, nob_temp_strdup(e.path));
    return true;
}

/* ================ 嵌入 user.bin ================ */
bool generate_user_blob(void) {
    if (!nob_file_exists(USER_BIN)) {
        nob_log(NOB_ERROR, "`%s` not found, build user first", USER_BIN);
        return false;
    }
    /* 将 raw binary 转成可链接的 .o，符号：
     *   _binary_build_bin_user_bin_start / _end / _size */
    Nob_Cmd c = {0};
    nob_cmd_append(&c,
     OBJCOPY, "-O", "binary",
     USER_ELF, USER_BIN
     );
    bool ok = nob_cmd_run(&c);
    nob_cmd_free(c);
    return ok;
}

/* ================ 构建一个 target ================ */
bool build_target(const BuildTarget *t) {
    size_t mark = nob_temp_save();
    bool ok = true;
    Nob_Cmd link = {0};
    Nob_Cmd oc   = {0};
    bool link_init = false, oc_init = false;

    /* 1. 编译全部源文件（async 并行） */
    if (!nob_walk_dir(t->src_dir, compile_walk, .data = (void*)t)) {
        ok = false; goto defer;
    }
    if (!nob_procs_flush(&g_procs)) { ok = false; goto defer; }

    /* 2. 收集 .o */
    g_objs.count = 0;
    if (!nob_walk_dir(t->obj_dir, collect_obj_walk)) { ok = false; goto defer; }

    /* 3. 链接 */
    build_link(&link, t, &g_objs);
    link_init = true;
    if (!nob_cmd_run(&link)) { ok = false; goto defer; }

    /* 4. 可选 objcopy */
    if (t->out_bin != NULL) {
        build_objcopy(&oc, t->out_elf, t->out_bin);
        oc_init = true;
        if (!nob_cmd_run(&oc)) { ok = false; goto defer; }
    }

defer:
    if (!ok) nob_procs_flush(&g_procs);   /* 回收可能残留的子进程 */
    if (link_init) nob_cmd_free(link);
    if (oc_init) nob_cmd_free(oc);
    nob_temp_rewind(mark);
    return ok;
}

/* ================ 主构建流程 ================ */
bool main_build(bool is_debug, int argc, char **argv) {
    rules_init(is_debug);

    /* 1. user：src/user/.c -> src/user/.o -> user.elf -> user.bin */
    {
        BuildTarget t = {
            .src_dir    = USER_SRC,
            .obj_dir    = USER_SRC,
            .ld_script  = USER_LD,
            .extra_libs = {0},
            .out_elf    = USER_ELF,
            .out_bin    = USER_BIN,
            .map_path   = NULL,
        };
        notice(ANSI_GREEN, "Build user");
        if (!build_target(&t)) return false;
    }

    // /* 2. 嵌入 user.bin -> src/kernel/user.o */
    // notice(ANSI_GREEN, "Embed user.bin into kernel");
    // if (!generate_user_blob()) return false;

    /* 3. kernel：src/kernel/.c -> src/kernel/.o（含 user.o）-> kernel.elf */
    {
        BuildTarget t = {
            .src_dir    = KERNEL_SRC,
            .obj_dir    = KERNEL_SRC,
            .ld_script  = LINK_FILE,
            .extra_libs = {0},
            .out_elf    = KERNEL_ELF,
            .out_bin    = NULL,
            .map_path   = KERNEL_MAP,
        };
        notice(ANSI_GREEN, "Build kernel");
        if (!build_target(&t)) return false;
    }

    /* 4. mmode：src/mmode/.c -> ... -> mmode.elf / mmode.bin */
    {
        BuildTarget t = {
            .src_dir    = MMODE_SRC,
            .obj_dir    = MMODE_SRC,
            .ld_script  = MMODE_LD,
            .extra_libs = {0},
            .out_elf    = MMODE_ELF,
            .out_bin    = MMODE_BIN,
            .map_path   = NULL,
        };
        
        // 换 mapi
        // rule_replace(&g_asm_flag, "-mabi=lp64d", "-mabi=lp64");
        // rule_replace(&g_cc_flag, "-mabi=lp64d", "-mabi=lp64");
        // rule_replace(&g_ld_flag, "-mabi=lp64d", "-mabi=lp64");
        
        nob_cmd_append(&t.extra_libs, OPEN_SBI_LIB); // 加入opeasbi 的静态链接库
        notice(ANSI_GREEN, "Build mmode");
        bool ok = build_target(&t);
        nob_cmd_free(t.extra_libs);
        if (!ok) return false;
    }

    return true;
}

bool main_qemu(bool is_debug) {
    Nob_Cmd c = {0};

    nob_cmd_append(&c, QEMU,
        "-machine", "virt",
        "-bios", MMODE_BIN,
        "-nographic",
        "-serial", "mon:stdio",
        "--no-reboot",
        "-kernel", 
        KERNEL_ELF
    );

    if (is_debug) nob_cmd_append(&c, "-s", "-S");
    bool ok = nob_cmd_run(&c);
    nob_cmd_free(c);
    return ok;
}

bool main_clean(void) {
    g_objs.count = 0;
    if (!nob_walk_dir("src", collect_obj_walk)) return false;

    if (g_objs.count == 0) {
        nob_log(NOB_WARNING, "no obj file to clean");
        return true;
    }
    Nob_Cmd c = {0};
    nob_cmd_append(&c, "rm");
    nob_cmd_extend(&c, &g_objs);
    bool ok = nob_cmd_run(&c);
    nob_cmd_free(c);
    g_objs.count = 0;
    return ok;
}

/* ================ main ================ */
int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    nob_shift_args(&argc, &argv); /* 丢弃程序名 */
    const char *subcmd = "build";
    if (argc > 0) subcmd = nob_shift_args(&argc, &argv);

    bool ok = true;
    if (strcmp(subcmd, "build") == 0) {
        ok = main_build(false, argc, argv);
    } else if (strcmp(subcmd, "run") == 0) {
        ok = main_build(false, argc, argv) && main_qemu(false);
    } else if (strcmp(subcmd, "debug") == 0) {
        ok = main_build(true, argc, argv) && main_qemu(true);
    } else if (strcmp(subcmd, "clean") == 0) {
        ok = main_clean();
    } else {
        notice(ANSI_RED, "unknown command");
        return 1;
    }

    if (!ok) {
        notice(ANSI_RED, "build failed");
        return 1;
    }
    notice(ANSI_GREEN, "nob success");
    return 0;
}
