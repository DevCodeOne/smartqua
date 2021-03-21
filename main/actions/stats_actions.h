#pragma once

#include <array>
#include <optional>
#include <limits>

#include "smartqua_config.h"
#include "actions/action_types.h"
#include "drivers/stats_driver.h"
#include "storage/store.h"

using stat_collection_operation = collection_operation_result;

json_action_result get_stats_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result add_stat_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result remove_stat_action(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result set_stat_action(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);

struct add_stat {
    std::optional<unsigned int> index = std::nullopt;
    std::array<char, name_length> stat_name;
    std::string_view description;
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
    std::string_view description;

    struct {
        stat_collection_operation collection_result = stat_collection_operation::failed; 
        single_stat_operation_result op_result = single_stat_operation_result::failure;
    } result;
};

struct retrieve_stat_info {
    unsigned int index = std::numeric_limits<unsigned int>::max();
    std::optional<single_stat_settings> stat_info;

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
    } result;
};

// TODO: put description into single_stat_settings for the filename later on
template<size_t N>
struct stats_trivial final {
    static inline constexpr char name[] = "stats_trivial";

    std::array<single_stat_settings, N> stat_settings;
    std::array<std::array<char, name_length>, N> description;
    std::array<bool, N> initialized;
};

template<size_t N>
class stat_collection final {
    public:
        // TODO: better calculation, which should also be configurable
        using trivial_representation = stats_trivial<N>;
        using driver_type = stats_driver<N>;
        template<typename T>
        using filter_return_type_t = std::conditional_t<
        !all_unique_v<T, add_stat, remove_stat, update_stat, retrieve_stat_info, retrieve_stat_overview>,
        trivial_representation, ignored_event>;

        stat_collection() = default;
        ~stat_collection() = default;

        stat_collection &operator=(trivial_representation new_value);

        template<typename T>
        filter_return_type_t<T> dispatch(T &event) {}

        template<typename T>
        void dispatch(T &event) const {}

        filter_return_type_t<add_stat> dispatch(add_stat &event);

        filter_return_type_t<remove_stat> dispatch(remove_stat &event);

        filter_return_type_t<update_stat> dispatch(update_stat &event);

        void dispatch(retrieve_stat_info &event) const;

        void dispatch(retrieve_stat_overview &event) const;
    private:
        trivial_representation data;
        std::array<std::optional<typename driver_type::stat_type>, N> m_stats;
};

template<size_t N>
stat_collection<N> &stat_collection<N>::operator=(trivial_representation new_value) {
    data = new_value;
    
    for (size_t i = 0; i < data.initialized.size(); ++i) {
        if (data.initialized[i]) {
            m_stats[i] = driver_type::create_stat(&data.stat_settings[i]);
        }
    }

    return *this;
}

template<size_t N>
auto stat_collection<N>::dispatch(add_stat &event) -> filter_return_type_t<add_stat> {
    if (event.description.size() == 0) {
        return data;
    }

    if (!event.index.has_value()) {
        for (unsigned int i = 0; i < N; ++i) {
            if (!data.initialized[i]) {
                event.index = i;
                break;
            }
        }
    }

    if (event.index.has_value() && *event.index < N && !data.initialized[*event.index]) {
        m_stats[*event.index] = driver_type::create_stat(event.description, data.stat_settings[*event.index]);
        data.initialized[*event.index] = m_stats[*event.index].has_value();
        data.description[*event.index] = event.stat_name;

        if (data.initialized[*event.index]) {
            event.result.result_index = *event.index;
            event.result.collection_result = stat_collection_operation::ok;
        } else {
            event.result.result_index = *event.index;
            event.result.collection_result = stat_collection_operation::failed;
        }
    } else {
        event.result.collection_result = stat_collection_operation::index_invalid;
    }
    return data;
}

template<size_t N>
auto stat_collection<N>::dispatch(remove_stat &event) -> filter_return_type_t<remove_stat> {
    if (event.index < N && data.initialized[event.index]) {
        m_stats[event.index] = std::nullopt;
        data.initialized[event.index] = false;
        event.result.collection_result = stat_collection_operation::ok;
    } else {
        event.result.collection_result = stat_collection_operation::failed;
    }

    return data;
}

template<size_t N>
auto stat_collection<N>::dispatch(update_stat &event) -> filter_return_type_t<update_stat> {
    if (event.index < N && data.initialized[event.index]) {
        // First delete timer and then reinitialize it, with old data, patched with new data
        m_stats[event.index] = std::nullopt;
        data.initialized[event.index] = false;

        m_stats[event.index] = driver_type::create_stat(event.description, data.stat_settings[event.index]);
        data.initialized[event.index] = m_stats[event.index].has_value();

        if (data.initialized[event.index]) {
            event.result.collection_result = stat_collection_operation::ok;
            event.result.op_result = single_stat_operation_result::ok;
        } else {
            event.result.collection_result = stat_collection_operation::failed;
            event.result.op_result = single_stat_operation_result::failure;
        }
    } else {
        event.result.collection_result = stat_collection_operation::index_invalid;
        event.result.op_result = single_stat_operation_result::failure;
    }
    return data;
}

template<size_t N>
void stat_collection<N>::dispatch(retrieve_stat_info &event) const {
    if (event.index < N && data.initialized[event.index]) {
        event.stat_info = data.stat_settings[event.index];
        event.result.collection_result = stat_collection_operation::ok;
    } else {
        event.result.collection_result = stat_collection_operation::index_invalid;
    }
}

template<size_t N>
void stat_collection<N>::dispatch(retrieve_stat_overview &event) const {
    unsigned int start_index = 0;
    if (event.index.has_value()) {
        start_index = *event.index;
    }

    if (start_index > N) {
        event.result.collection_result = stat_collection_operation::index_invalid; 
    }

    const char *format = ", { index : %u, description : %M }";
    json_out out = JSON_OUT_BUF(event.output_dst, event.output_len);

    int written = 0;
    json_printf(&out, "[");
    for (unsigned int index = start_index; index < N; ++index) {
        if (data.initialized[index]) {
            written += json_printf(&out, format + (written == 0 ? 1 : 0), index, 
                json_printf_single<std::decay_t<decltype(data.description[index])>>, &data.description[index]);
        }
    }
    json_printf(&out, " ]");
    event.result.collection_result = stat_collection_operation::ok;
}