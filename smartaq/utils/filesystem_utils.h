#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>

#include "stack_string.h"

bool ensure_path_exists(const char *path, uint32_t mask = 0777);

template<size_t DstLength>
requires (DstLength >= 2)
bool parentPath(const std::string_view &path, BasicStackString<DstLength> &dst) {
    int32_t copy_to_char = 0;

    // If there is no parent directory
    dst = ".";

    // input.size ... 1 the 0-th position is not checked
    for (auto i = path.size() - 1; i > 0; --i) {
        if (path[i] == '/') {
            // Check if previous char is not an escape char
            if (path[i - 1] != '\\') {
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

    dst.set(path.data(), copy_to_char + 1);

    return true;
}

// TODO: Also add version which accepts a pod
int64_t loadFileCompletelyIntoBuffer(std::string_view path, void *dst, size_t dst_len);

bool safeWriteToFile(std::string_view path, std::string_view tmpExtension, const void *data, size_t length);
bool safeWriteToFile(std::string_view path, std::string_view tmpExtension, std::string_view input);

template<typename TrivialType>
requires (std::is_trivial_v<TrivialType> && std::is_standard_layout_v<TrivialType>)
bool safeWritePodToFile(std::string_view path, std::string_view tmpExtension, const TrivialType *value) {
    return safeWriteToFile(path, tmpExtension, reinterpret_cast<const void *>(value), sizeof(TrivialType));
}

template<size_t N>
struct ConstexprPath {
    constexpr explicit ConstexprPath(const char (&path)[N]) {
        std::copy_n(path, N, value);
    }

    static constexpr size_t length = N;
    char value[N];
};