#pragma once
#include <nob.h>
#include <assert.h>
#include "pathlist.h"

#define ANSI_RED   "\033[31m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_GREEN "\033[32m"
#define ANSI_RESET "\033[0m"

#define CC "clang"
#define STR(x) #x
#define LOC     __FILE__ ":" STR(__LINE__) ": "
#define build_add nob_cmd_append

void build_add_flag(Nob_Cmd *cmd_ptr, const char ** flag, size_t count);
void build_add_obj(Nob_Cmd *cmd_ptr, const char *c_input, const char *o_output);
void build_run_obj_list(Nob_Cmd *cmd_ptr, const char **c_input, const char **o_output, size_t count);
bool my_log(const char *color, const char *message);

#define build_run(cmd_ptr, ...) assert(          \
    nob_cmd_run(cmd_ptr, ##__VA_ARGS__) ||   \
    (my_log(ANSI_RED, LOC "Build Error"))  \
)

#define C_FLAG_COUNT ( sizeof(C_FLAG) / sizeof(C_FLAG[0]) )
#define PATH_MAX_LEN 1024
