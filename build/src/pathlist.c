#include "pathlist.h"
#include <nob.h>
#include <stdlib.h>
#include <string.h>

PathList *pathlist_init(size_t cap) {
    PathList *list = (PathList *)malloc(sizeof(PathList));
    if (!list) {
        nob_log(NOB_ERROR, "pathlist_init: out of memory");
        exit(1);
    }

    list->count = 0;
    list->cap   = cap;
    list->items = cap ? malloc(cap * sizeof(char *)) : NULL;

    if (cap && !list->items) {
        nob_log(NOB_ERROR, "pathlist_init: out of memory");
        exit(1);
    }

    return list;
}

void pathlist_free(PathList *list) {
    if (!list) return;
    // for (size_t i = 0; i < list->count; i++) 
    //     free(list->items[i]);
    free(list->items);
    free(list);
}


bool pathlist_reserve(PathList *list, size_t need) {
    if (need <= list->cap) return true;

    size_t new_cap = list->cap ? list->cap : 4;
    while (new_cap < need) new_cap *= 1.5;

    char **new_items = realloc(list->items, new_cap * sizeof(char *));
    if (!new_items) {
        nob_log(NOB_ERROR, "pathlist_reserve: out of memory");
        return false;
    }
    list->items = new_items;
    list->cap   = new_cap;
    return true;
}

/* 加入字符串到 list 中 可能会自动扩容 */
bool pathlist_add(PathList *list, const char *str, size_t str_len) {
    if (!pathlist_reserve(list, list->count + 1)) return false;

    char *copy = malloc(str_len + 1);
    if (!copy) {
        nob_log(NOB_ERROR, "pathlist_add: out of memory");
        return false;
    }
    memcpy(copy, str, str_len);
    copy[str_len] = '\0';

    list->items[list->count++] = copy;
    return true;
}
