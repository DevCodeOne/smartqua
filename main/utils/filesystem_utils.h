#pragma once

#include <cstdint>
#include <cstdlib>
#include <string_view>

bool ensure_path_exists(const char *path, uint32_t mask = 0777);
bool copy_parent_directory(const char *path, char *dst, size_t dst_len);
int64_t load_file_completly_into_buffer(std::string_view path, char *dst, size_t dst_len);
bool safe_write_to_file(std::string_view path, std::string_view tmpExtension, std::string_view input);