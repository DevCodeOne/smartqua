#include "filesystem_utils.h"

#include <cstring>

#include "esp_vfs.h"
#include "ctre.hpp"

static constexpr ctll::fixed_string directory_pattern{R"(\/(\w+))"};

bool ensure_path_exists(const char *path, uint32_t mask) {
    std::array<char, 256> path_tmp{'\0'};

    if (std::strlen(path) > path_tmp.size() - 1) {
        return false;
    }

    bool path_exists = true;
    for (auto directory : ctre::range<directory_pattern>(path)) {
        std::string_view directory_view(directory.get<0>());
        struct stat tmp_stat{0};

        std::strncat(path_tmp.data(), directory_view.data(), directory_view.size());
        if (stat(path_tmp.data(), &tmp_stat) == -1) {
            path_exists &= mkdir(path_tmp.data(), mask) == 0;
        }
    }

    return path_exists;
}

bool copy_parent_directory(const char *path, char *dst, size_t dst_len) {
    std::string_view input(path);
    int32_t copy_to_char = 0;

    if (dst_len < 2) {
        return false;
    }

    // If there is no parent directory
    std::strncpy(dst, ".", dst_len);

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

    // dst_len is too small to contain the path
    if (dst_len <= copy_to_char + 1) {
        return false;
    }

    // + 1 for terminating \0
    std::strncpy(dst, path, copy_to_char + 1);
    return true;
}