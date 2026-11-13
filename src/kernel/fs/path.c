#include "kernel/fs/file.h"
#include "kernel/fs/path.h"
#include "kernel/proc/process.h"

#define isspace(x) (x == ' ')
#define base_check(ch) ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == '.') || (c == '_'))

bool path_get_dir(StringView sv, StringView* out_dir){
    int pos = -1;
    for (int i = sv.count; i >= 0; i --){
        if (sv.data[i] == '/'){
            pos = i;
            break;
        }
    }

    if (pos == -1){
        return false;
    }
    if (pos == 0) pos = 1; // 根目录

    *out_dir = sv_from_parts(sv.data, pos);
    return true;
}

bool path_get_filename(StringView sv, StringView* out_fname){
    int pos = -1;
    for (int i = sv.count; i >= 0; i --){
        if (sv.data[i] == '/'){
            pos = i;
            break;
        }
    }
    if (pos == -1){
        return false;
    }

    *out_fname = sv_from_parts(sv.data + pos + 1, sv.count - pos - 1);
    return true;
}


/* 文件名只能使用英文字母 + 数字 + _ + . */
bool is_vaild_filename(StringView path){
    if (path.count > FILE_NAME_MAX) return false;
    for (u64 i = 0; i < path.count; i ++){
        char c = path.data[i];
        bool res = base_check(c);
        if (!res) return false;
    }
    return true;
}

/* 合法路径只能使用英文字母 + 数字 + / */
bool is_vaild_path(StringView path){
    if (path.count > FILE_NAME_MAX) return false;

    int slash = 0;

    if (path.data[0] != '/') return false;
    if (path.count == 1) return true; // case : "/"

    for (u64 i = 0; i < path.count; i ++){
        char c = path.data[i];
        switch (c) {
            case '/':
                slash ++;
                if (slash == 2) return false;
                break;
            default:
                bool res = base_check(c);
                if (!res) return false;
                slash = 0;
        } 
    }
    if (path.data[path.count - 1] == '/') return false; // 末尾不可为 /
    return true;
}
