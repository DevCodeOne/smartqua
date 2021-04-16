#pragma once

#include <cstdint>
#include <cstdlib>

bool ensure_path_exists(const char *path, uint32_t mask = 0777);
bool copy_parent_directory(const char *path, char *dst, size_t dst_len);
int64_t load_file_completly_into_buffer(const char *path, char *dst, size_t dst_len);