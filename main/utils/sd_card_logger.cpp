#include "sd_card_logger.h"

#include "utils/sd_filesystem.h"
#include "utils/filesystem_utils.h"
#include "utils/stack_string.h"
#include "utils/utils.h"

bool sd_card_logger::install_sd_card_logger() {
    std::unique_lock instance_guard{_instance_mutex };

    auto previous_function = esp_log_set_vprintf(sd_card_logger::printf_log);

    if (_original_function == nullptr) {
        _original_function = previous_function;
    }

    return true;
}

bool sd_card_logger::uninstall_sd_card_logger() {
    std::unique_lock instance_guard{_instance_mutex };

    esp_log_set_vprintf(_original_function);

    return true;
}

void sd_card_logger::potential_flush(int newly_written) {
    std::unique_lock instance_guard{_instance_mutex};
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

FILE *sd_card_logger::open_current_log() {
    std::unique_lock instance_guard{_instance_mutex };

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

        snprintf(folder_name.data(), folder_name.size() - 1, sd_card_logger::log_path_format, sd_filesystem::mount_point);
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

int sd_card_logger::printf_log(const char *fmt, va_list list) {
    auto current_log_file = open_current_log();

    if (current_log_file == nullptr) {
        return _original_function(fmt, list);
    }

    int written = 0;
    {
        std::unique_lock instance_guard{_instance_mutex };

        // TODO: Option to only log to file or to log to stdio
        _original_function(fmt, list);
        written = vfprintf(_log_output_file, fmt, list);
    }

    potential_flush(written);

    return written; 
} 