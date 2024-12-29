#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>

#include "stack_string.h"

bool ensure_path_exists(const char *path, uint32_t mask = 0777);

template<size_t DstLength>
requires (DstLength >= 2)
bool parentPath(const char *path, BasicStackString<DstLength> &dst) {
    std::string_view input(path);

    int32_t copy_to_char = 0;

    // If there is no parent directory
    dst = ".";

    // input.size ... 1 the 0-th position is not checked
    for (auto i = input.size() - 1; i > 0; --i) {
        if (input[i] == '/') {
            // Check if previous char is not an escape char
            if (input[i - 1] != '\\') {
                copy_to_char = i - 1;
                break;
            }
        }
    }

    if (copy_to_char == 0) {
        return true;
    }

    if (dst.canHold(copy_to_char + 1)) {
        return false;
    }

    dst.set(path, copy_to_char + 1);

    return true;
}

int64_t loadFileCompletelyIntoBuffer(std::string_view path, void *dst, size_t dst_len);
bool safeWriteToFile(std::string_view path, std::string_view tmpExtension, const void *data, size_t length);
bool safeWriteToFile(std::string_view path, std::string_view tmpExtension, std::string_view input);

template<size_t N>
struct ConstexprPath {
    constexpr explicit ConstexprPath(const char (&path)[N]) {
        std::copy_n(path, N, value);
    }

    char value[N];
};