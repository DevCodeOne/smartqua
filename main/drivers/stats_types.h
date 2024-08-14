#pragma once

#include <limits>
#include <chrono>

#include "frozen.h"

#include "utils/json_utils.h"

enum single_stat_operation_result {
    ok, failure
};

struct SingleStatSettings {
    static inline constexpr char StorageName[] = "SingleStatSettings";

    std::chrono::minutes stat_interval{5};
    unsigned int device_index{std::numeric_limits<unsigned int>::max()};

    std::chrono::minutes last_checked{0};
};

// TODO: implement
template<>
struct print_to_json<SingleStatSettings> {
    static int print(json_out *out, const SingleStatSettings &stat) {
        return 0;
    }
};


template<>
struct read_from_json<SingleStatSettings> {
    static void read(const char *str, int len, SingleStatSettings *values) {
    }
};