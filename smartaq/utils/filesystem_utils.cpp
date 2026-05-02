#include "filesystem_utils.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <sys/stat.h>

#include "utils/do_finally.h"
#include "utils/stack_string.h"

// TODO: Move to header, when it is not dependent on esp32
static constexpr size_t max_path_length = 32;

// TODO: again replace with std::string_view, optimize, first check if folder already exists
bool ensurePathExists(const char* path, uint32_t mask)
{
    if (path == nullptr || path[0] == '\0')
    {
        return false;
    }

    if (!BasicStackString<max_path_length>::canHold(path))
    {
        return false;
    }

    BasicStackString<max_path_length> pathCopy{path};
    std::string_view pathView{pathCopy.data()};

    const auto firstDirectoryStart = pathView.find_first_not_of('/');

    if (firstDirectoryStart == std::string_view::npos)
    {
        return true;
    }

    for (auto separator = pathView.find_first_of('/', firstDirectoryStart);
         separator != std::string_view::npos;
         separator = pathView.find_first_of('/', separator + 1))
    {
        pathCopy.data()[separator] = '\0';

        const int result = mkdir(pathCopy.data(), mask);

        pathCopy.data()[separator] = '/';

        if (result == -1 && errno != EEXIST)
        {
            return false;
        }
    }

    const int result = mkdir(pathCopy.data(), mask);

    return result == 0 || errno == EEXIST;
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

// TODO: Add method which writes file with known content to check each mount for validity
FileSystemStatus writeTestFile(const char *path, std::string_view content) {
    FILE *testFile = fopen(path, "w+");
    if (testFile == nullptr) {
        return FileSystemStatus::NoOpen;
    }
    if (fwrite(content.data(), 1, content.length(), testFile) != content.length()) {
        return FileSystemStatus::NoWrite;
    }
    fseek(testFile, 0L, SEEK_SET);
    FileSystemStatus validateStatus = FileSystemStatus::Ok;
    for (auto currentChar: content) {
        const auto read = fgetc(testFile);
        if (read != currentChar) {
            validateStatus = FileSystemStatus::NoValidate;
            break;
        }
    }
    fclose(testFile);

    if (validateStatus != FileSystemStatus::Ok) {
        remove(path);
        return validateStatus;
    }

    return remove(path) == 0 ? FileSystemStatus::Ok : FileSystemStatus::NoRemove;
}

bool safeWriteToFile(std::string_view path, std::string_view tmpExtension, const void *data, size_t length) {
    using PathString = BasicStackString<max_path_length>;

    if (data == nullptr) {
        return false;
    }

    PathString tmpPathCopy;
    parentPath(path, tmpPathCopy);

    if (!ensurePathExists(tmpPathCopy.data())) {
        // Logger::log(LogLevel::Error, "Couldn't create path : %.*s", tmpPathCopy.len(), tmpPathCopy.data());
        return false;
    }

    if (!PathString::canHold(path.data())) {
        return false;
    }

    snprintf(tmpPathCopy.data(), PathString::capacity(), "%.*s%.*s",
             static_cast<int>(path.length()), path.data(), static_cast<int>(tmpExtension.length()),
             tmpExtension.data());
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
