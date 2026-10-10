#pragma once
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char **items;   // 所有路径的字符串数组
    size_t count;   // 当前字符串数量
    size_t cap;     // 最大 count 容量
} PathList;

// 初始化字符串
PathList *pathlist_init(size_t cap);

// NOTE : list->items的内存一定不会释放，所有字符串路径与构建系统的生命周期相等
void pathlist_free(PathList *list);

/* 预订need个字符串，如果还是不够则 cap *= 1.5 */
bool pathlist_reserve(PathList *list, size_t need);


/* 追加一个字符串的副本（拷贝 str_len 字节） */
bool pathlist_add(PathList *list, const char *str, size_t str_len);
