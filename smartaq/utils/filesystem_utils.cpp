#include "filesystem_utils.h"

#include <cstring>
#include <string_view>
#include <sys/types.h>
#include <sys/stat.h>

#include "ctre.hpp"

#include "utils/do_finally.h"
#include "utils/stack_string.h"

static constexpr ctll::fixed_string directory_pattern{R"(\/(\w+))"};

// TODO: Move to header, when it is not dependent on esp32
static constexpr size_t max_path_length = 32;

// TODO: again replace with std::string_view, optimize, first check if folder already exists
bool ensure_path_exists(const char *path, uint32_t mask) {
    if (!BasicStackString<max_path_length>::canHold(path)) {
        return false;
    }

    BasicStackString<max_path_length> pathCopy{path};

    if (int result = mkdir(pathCopy.data(), mask); result == EEXIST) {
        return true;
    }

    pathCopy.clear();

    // Only use return codes of mkdir, stat is way to slow
    bool path_exists = true;
    for (auto directory : ctre::search_all<directory_pattern>(path)) {
        std::string_view directoryView(directory.get<0>());

        const bool appendResult = pathCopy.append(directoryView);

        if (!appendResult) {
            return false;
        }

        int result = mkdir(pathCopy.data(), mask);

        if (result == ENOTDIR) {
            path_exists = false;
        }
    }

    return path_exists;
}

int64_t loadFileCompletelyIntoBuffer(std::string_view path, void *dst, size_t dst_len) {
    // stack_string is zero terminated, also check for too long filename
    using PathString = BasicStackString<max_path_length>;
    if (!PathString::canHold(path.data())) {
        return -1;
    }

    PathString pathCopy{path};
    auto opened_file = std::fopen(pathCopy.data(), "rb");
    DoFinally closeOp( [&opened_file]() {
        std::fclose(opened_file);
    });

    if (opened_file == nullptr) {
        // Logger::log(LogLevel::Error, "Couldn't open file %s", pathCopy.data());
        return -1;
    }

    std::fseek(opened_file, 0, SEEK_END);
    const auto file_size = std::ftell(opened_file);

    if (file_size > dst_len) {
        return -1;
    }

    std::rewind(opened_file);
    const int64_t read_size = std::fread(dst, 1, file_size, opened_file);

    /*
    if (read_size < dst_len && read_size > 0) {
        dst[read_size] = '\0';
    }
    */

    return read_size;
}

bool safeWriteToFile(std::string_view path, std::string_view tmpExtension, std::string_view input) {
    // Write terminating zero
    return safeWriteToFile(path, tmpExtension, input.data(), input.length() + 1);
}

bool safeWriteToFile(std::string_view path, std::string_view tmpExtension, const void *data, size_t length) {
    using PathString = BasicStackString<max_path_length>;

    if (data == nullptr) {
        return false;
    }

    PathString tmpPathCopy;
    parentPath(path, tmpPathCopy);

    if (!ensure_path_exists(tmpPathCopy.data())) {
        // Logger::log(LogLevel::Error, "Couldn't create path : %.*s", tmpPathCopy.len(), tmpPathCopy.data());
        return false;
    }

    if (!PathString::canHold(path.data())) {
        return false;
    }

    snprintf(tmpPathCopy.data(), decltype(tmpPathCopy)::ArrayCapacity - 1, "%.*s%.*s",
        path.length(), path.data(), tmpExtension.length(), tmpExtension.data());
    FILE *tmpTargetFile = std::fopen(tmpPathCopy.data(), "wb");

    if (tmpTargetFile == nullptr) {
        // Logger::log(LogLevel::Error, "Couldn't open file : %s", tmpPathCopy.data());
        return false;
    }

    {
        DoFinally closeOp([&tmpTargetFile]() {
            std::fclose(tmpTargetFile);
        });

        const auto written = std::fwrite(data, 1, length, tmpTargetFile);

        if (written != length) {
            // Logger::log(LogLevel::Warning, "Didn't write enough bytes %d, %d", written, (int) length);
            return false;
        }
    }

    std::remove(path.data());
    return std::rename(tmpPathCopy.data(), path.data()) >= 0;
}