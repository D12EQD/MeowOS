#include "main.h"

static const char *sources[] = { 
    "pmm"
};

void build_mm(PathList* obj_list, const char *obj_per, const char *src_per){
    Nob_Cmd cmd = {0};

    size_t n = sizeof(sources)/sizeof(sources[0]);

    char c_path[PATH_MAX_LEN], o_path[PATH_MAX_LEN];

    for (size_t i = 0; i < n; ++i) {
        snprintf(c_path, sizeof(c_path), "%s/%s.c", src_per, sources[i]);
        snprintf(o_path, sizeof(o_path), "%s/%s.o", src_per, sources[i]);
        
        build_add_obj(&cmd, c_path, o_path);
        build_run(&cmd);
        pathlist_add(obj_list, o_path, strlen(o_path) + 1);
    }
}
