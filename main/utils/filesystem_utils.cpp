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