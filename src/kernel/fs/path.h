#pragma once
#include "kernel/lib/string.h"

bool path_get_dir(StringView sv, StringView* out_dir);
bool path_get_filename(StringView sv, StringView* out_fname);
bool is_vaild_path(StringView path);
bool is_vaild_filename(StringView path);

