#include "logger.h"

#include "smartqua_config.h"
#include "utils/sd_filesystem.h"
#include "utils/filesystem_utils.h"
#include "utils/stack_string.h"
#include "utils/utils.h"

const char *to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:
            return "debug";
        case LogLevel::Info:
            return "info";
        case LogLevel::Warning:
            return "warn";
        case LogLevel::Error:
            return "error";
    }
    return "";
}

bool HttpLogSink::install() { return true; }

bool HttpLogSink::uninstall() { return true; }

bool SdCardSink::install() { return true; }

bool SdCardSink::uninstall() { return true; }

void SdCardSink::potential_flush(int newly_written) {
    _since_last_flush += newly_written;

    if (_log_output_file == nullptr) {
        _since_last_flush = 0;
        return; 
    }

    // TODO: make configurable and maybe add double buffering technique
    if (_since_last_flush > 512) {
        _since_last_flush = 0;

        fflush(_log_output_file);
        fsync(fileno(_log_output_file));
    }
}

FILE *SdCardSink::open_current_log() {
    std::tm timeinfo;
    std::time_t now;

    time(&now);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_year < 100) {
        return nullptr;
    }

    if (_log_output_file == nullptr || std::ftell(_log_output_file) > MAX_LOG_FILESIZE) {
        if (_log_output_file != nullptr) {
            fclose(_log_output_file);
            _log_output_file = nullptr;
        }

        stack_string<name_length> time{};
        // TODO: do this inline in one array
        stack_string<name_length> folder_name{};
        stack_string<name_length * 4> new_log_name{};

        snprintf(folder_name.data(), folder_name.size() - 1, SdCardSink::log_path_format, sd_filesystem::mount_point);
        auto output_folder_exists = ensure_path_exists(folder_name.data());

        if (!output_folder_exists) {
            return nullptr;
        }

        strftime(time.data(), name_length, "%F-%H-%M-%S", &timeinfo);
        snprintf(new_log_name.data(), new_log_name.size() - 1, "%s/log_%s.txt", folder_name.data(), time.data());
  
        _log_output_file = fopen(new_log_name.data(), "ab+");
    }

    return _log_output_file;
}