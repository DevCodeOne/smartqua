#pragma once

#include <limits>
#include <chrono>

#include "frozen.h"

#include "utils/json_utils.h"

enum single_stat_operation_result {
    ok, failure
};

struct single_stat_settings {
    static inline constexpr char StorageName[] = "SingleStatSettings";

    std::chrono::minutes stat_interval{5};
    unsigned int device_index{std::numeric_limits<unsigned int>::max()};

    std::chrono::minutes last_checked{0};
};

// TODO: implement
template<>
struct print_to_json<single_stat_settings> {
    static int print(json_out *out, const single_stat_settings &stat) {
        return 0;
    }
};


template<>
struct read_from_json<single_stat_settings> {
    static void read(const char *str, int len, single_stat_settings *values) {
    }
};