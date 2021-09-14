#pragma once

#include <cstdint>
#include <cstdlib>
#include <string_view>

bool ensure_path_exists(const char *path, uint32_t mask = 0777);
bool copy_parent_directory(const char *path, char *dst, size_t dst_len);
int64_t loadFileCompletelyIntoBuffer(std::string_view path, char *dst, size_t dst_len);
bool safeWriteToFile(std::string_view path, std::string_view tmpExtension, std::string_view input);