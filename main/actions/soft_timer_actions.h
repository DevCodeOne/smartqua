#pragma once

#include <array>
#include <optional>
#include <limits>

#include "actions/action_types.h"
#include "drivers/soft_timer_types.h"
#include "drivers/soft_timers.h"
#include "storage/store.h"

json_action_result get_timers_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result add_timer_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result remove_timer_action(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result set_timer_action(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);

using timer_collection_operation = collection_operation_result;

struct add_timer {
    std::optional<unsigned int> index = std::nullopt;
    std::array<char, name_length> timer_name;
    std::string_view description;

    struct {
        timer_collection_operation collection_result = timer_collection_operation::failed;
        std::optional<unsigned int> result_index = std::nullopt;
    } result;
};

struct remove_timer {
    unsigned int index = std::numeric_limits<unsigned int>::max();

    struct {
        timer_collection_operation collection_result = timer_collection_operation::failed;
    } result;
};

struct update_timer {
    unsigned int index = std::numeric_limits<unsigned int>::max();
    std::string_view description;

    struct {
        timer_collection_operation collection_result = timer_collection_operation::failed;
        single_timer_operation_result op_result = single_timer_operation_result::failure;
    } result;
};

struct retrieve_timer_info {
    unsigned int index = std::numeric_limits<unsigned int>::max();
    std::optional<single_timer_settings> timer_info;

    struct {
        timer_collection_operation collection_result = timer_collection_operation::failed;
    } result;
};

struct retrieve_timer_overview {
    std::optional<unsigned int> index = std::nullopt;
    char *output_dst = nullptr;
    size_t output_len = 0;

    struct {
        timer_collection_operation collection_result = timer_collection_operation::failed;
    } result;
};

template<size_t N>
struct timer_settings_trivial {
    static inline constexpr char name[] = "timers";

    std::array<single_timer_settings, N> timers;
    std::array<std::array<char, name_length>, N> description;
    std::array<bool, N> initialized;
};

template<size_t N>
class soft_timer_settings final {
public:
    using trivial_representation = timer_settings_trivial<N>;
    using driver_type = soft_timer_driver<N>;
    template<typename T>
    using filter_return_type = std::conditional_t<!
    all_unique_v<T,
    add_timer, remove_timer, update_timer>, trivial_representation, ignored_event>;
    
    soft_timer_settings() = default;
    ~soft_timer_settings() = default;
    
    soft_timer_settings &operator=(trivial_representation new_value);

    template<typename T>
    filter_return_type<T> dispatch(T &event) {}

    template<typename T>
    void dispatch(T &event) const {}

    filter_return_type<add_timer> dispatch(add_timer &event);

    filter_return_type<remove_timer> dispatch(remove_timer &event);

    filter_return_type<update_timer> dispatch(update_timer &event);

    void dispatch(retrieve_timer_info &event) const;

    void dispatch(retrieve_timer_overview &event) const;

private:
    trivial_representation data;
    std::array<std::optional<typename driver_type::timer_type>, N> m_soft_timers;
};

template<size_t N>
soft_timer_settings<N> &soft_timer_settings<N>::operator=(trivial_representation new_value) {
    data = new_value;

    for (size_t i = 0; i < data.initialized.size(); ++i) {
        if (data.initialized[i]) {
            m_soft_timers[i] = driver_type::create_timer(&data.timers[i]);
        }
    }
    return *this;
}

template<size_t N>
auto soft_timer_settings<N>::dispatch(add_timer &event) -> filter_return_type<add_timer> {
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
        m_soft_timers[*event.index] = driver_type::create_timer(event.description, data.timers[*event.index]);
        data.initialized[*event.index] = m_soft_timers[*event.index].has_value();
        data.description[*event.index] = event.timer_name;

        if (data.initialized[*event.index]) {
            event.result.result_index = *event.index;
            event.result.collection_result = timer_collection_operation::ok;
        } else {
            event.result.result_index = *event.index;
            event.result.collection_result = timer_collection_operation::failed;
        }
    } else {
        event.result.collection_result = timer_collection_operation::index_invalid;
    }
    return data;
}

template<size_t N>
auto soft_timer_settings<N>::dispatch(remove_timer &event) -> filter_return_type<remove_timer> {
    if (event.index < N && data.initialized[event.index]) {
        m_soft_timers[event.index] = std::nullopt;
        data.initialized[event.index] = false;
        event.result.collection_result = timer_collection_operation::ok;
    } else {
        event.result.collection_result = timer_collection_operation::index_invalid;
    }
    return data;
}

template<size_t N>
auto soft_timer_settings<N>::dispatch(update_timer &event) -> filter_return_type<update_timer> {
    if (event.index < N && data.initialized[event.index]) {
        // First delete timer and then reinitialize it, with old data, patched with new data
        m_soft_timers[event.index] = std::nullopt;
        data.initialized[event.index] = false;

        m_soft_timers[event.index] = driver_type::create_timer(event.description, data.timers[event.index]);
        data.initialized[event.index] = m_soft_timers[event.index].has_value();

        if (data.initialized[event.index]) {
            event.result.collection_result = timer_collection_operation::ok;
            event.result.op_result = single_timer_operation_result::ok;
        } else {
            event.result.collection_result = timer_collection_operation::failed;
            event.result.op_result = single_timer_operation_result::failure;
        }
    } else {
        event.result.collection_result = timer_collection_operation::index_invalid;
        event.result.op_result = single_timer_operation_result::failure;
    }
    return data;
}

template<size_t N>
void soft_timer_settings<N>::dispatch(retrieve_timer_info &event) const {
    if (event.index < N && data.initialized[event.index]) {
        event.timer_info = data.timers[event.index];
        event.result.collection_result = timer_collection_operation::ok;
    } else {
        event.result.collection_result = timer_collection_operation::index_invalid;
    }
}

template<size_t N>
void soft_timer_settings<N>::dispatch(retrieve_timer_overview &event) const {
    unsigned int start_index = 0;
    if (event.index.has_value()) {
        start_index = *event.index;
    }

    if (start_index > N) {
        event.result.collection_result = timer_collection_operation::index_invalid; 
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
    event.result.collection_result = timer_collection_operation::ok;
}