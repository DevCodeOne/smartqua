#include "filesystem_utils.h"

#include <cstring>
#include <cstdlib>
#include <string_view>
#include <sys/types.h>
#include <sys/stat.h>

#include "ctre.hpp"

#include "utils/utils.h"
#include "utils/logger.h"
#include "utils/stack_string.h"
#include "smartqua_config.h"

static constexpr ctll::fixed_string directory_pattern{R"(\/(\w+))"};

// TODO: again replace with std::string_view, optimizie, first check if folder already exists
bool ensure_path_exists(const char *path, uint32_t mask) {
    stack_string<name_length * 2> pathCopy = path;

    if (std::strlen(path) > decltype(pathCopy)::ArrayCapacity - 1) {
        return false;
    }

    if (int result = mkdir(pathCopy.data(), mask); result == EEXIST) {
        return true;
    }

    // Clear pathCopy to start concatenating the folder names
    pathCopy[0] = '\0';

    // Only use return codes of mkdir, stat is wayy to slow
    bool path_exists = true;
    for (auto directory : ctre::range<directory_pattern>(path)) {
        std::string_view directoryView(directory.get<0>());

        std::strncat(pathCopy.data(), directoryView.data(), directoryView.size());
        int result = mkdir(pathCopy.data(), mask);

        if (result == ENOTDIR) {
            path_exists = false;
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

int64_t loadFileCompletelyIntoBuffer(std::string_view path, char *dst, size_t dst_len) {
    // stack_string is zero terminated, also check for too long filename
    stack_string<name_length * 4> pathCopy = path;
    auto opened_file = std::fopen(pathCopy.data(), "rb");
    DoFinally closeOp( [&opened_file]() {
        std::fclose(opened_file);
    });

    if (opened_file == nullptr) {
        Logger::log(LogLevel::Error, "Couldn't open file %s", pathCopy.data());
        return -1;
    }

    std::fseek(opened_file, 0, SEEK_END);
    const auto file_size = std::ftell(opened_file);

    if (file_size > dst_len) {
        return -1;
    }

    std::rewind(opened_file);
    const int64_t read_size = std::fread(dst, 1, file_size, opened_file);

    if (read_size < dst_len && read_size > 0) {
        dst[read_size] = '\0';
    }

    return read_size;
}

bool safeWriteToFile(std::string_view path, std::string_view tmpExtension, std::string_view input) {
    stack_string<name_length * 4> tmpPathCopy;
    copy_parent_directory(path.data(), tmpPathCopy.data(), decltype(tmpPathCopy)::ArrayCapacity);
    auto pathExists = ensure_path_exists(tmpPathCopy.data());

    if (!pathExists) {
        Logger::log(LogLevel::Error, "Couldn't create path : %.*s", tmpPathCopy.len(), tmpPathCopy.data());
        return false;
    }

    stack_string<name_length * 4> pathCopy = path;

    snprintf(tmpPathCopy.data(), decltype(tmpPathCopy)::ArrayCapacity - 1, "%.*s%.*s",
        path.length(), path.data(), tmpExtension.length(), tmpExtension.data());
    FILE *tmpTargetFile = std::fopen(tmpPathCopy.data(), "w");

    if (tmpTargetFile == nullptr) {
        Logger::log(LogLevel::Error, "Couldn't open file : %s", tmpPathCopy.data());
        return false;
    }

    {
        DoFinally closeOp([&tmpTargetFile]() {
            std::fclose(tmpTargetFile);
        });

        const auto written = std::fwrite(reinterpret_cast<const void *>(input.data()), 1, input.length(), tmpTargetFile);

        if (written != input.length()) {
            Logger::log(LogLevel::Warning, "Didn't write enough bytes %d, %d", written, (int) input.length());
            return false;
        }
    }

    std::remove(pathCopy.data());
    auto rename_result = std::rename(tmpPathCopy.data(), pathCopy.data());

    return rename_result >= 0;
}