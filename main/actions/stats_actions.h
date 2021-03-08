#pragma once

#include <array>
#include <optional>
#include <limits>

#include "smartqua_config.h"
#include "actions/action_types.h"
#include "storage/store.h"

enum stat_collection_operation = collection_operation_result;

json_action_result get_stats_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result add_stat_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result remove_stat_action(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result set_stat_action(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);

struct add_stat {
    std::optional<unsigned int> index = std::nullopt;
    std::array<char, name_length> stat_name;
    // TODO: add missing struct here to add

    struct {
        stat_collection_operation collection_result = stat_collection_operation::failed;
        std::optional<unsigned int> result_index = std::nullopt;
    } result;
};

struct remove_stat {
    unsigned int index = std::numeric_limits<unsigned int>::max();

    struct {
        stat_collection_operation collection_result = stat_collection_operation::failed;
    } result;
};

struct update_stat {
    unsigned int index = std::numeric_limits<unsigned int>::max();

    struct {
        stat_collection_operation collection_result = stat_collection_operation::failed; 
    } result;
};

struct retrieve_stat_info {
    unsigned int index = std::numeric_limits<unsigned int>::max();
    // TODO: add struct

    struct {
        stat_collection_operation collection_result = stat_collection_operation::failed;
    } result;
};

struct retrieve_stat_overview {
    std::optional<unsigned int> index = std::nullopt;
    char *output_dst = nullptr;
    size_t output_len = 0;

    struct {
        stat_collection_operation collection_result = stat_collection_operation::failed;
    }
};