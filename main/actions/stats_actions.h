#pragma once

#include <array>
#include <optional>
#include <limits>
#include <string_view>

#include "smartqua_config.h"
#include "actions/action_types.h"
#include "drivers/stats_driver.h"
#include "storage/store.h"
#include "utils/event_access_array.h"

using stat_collection_operation = collection_operation_result;

json_action_result get_stats_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result add_stat_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result remove_stat_action(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result set_stat_action(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);

static inline constexpr size_t stat_uid = 20;

using set_stat = SmartAq::Utils::ArrayActions::SetValue<single_stat_settings, stat_uid>;
using remove_stat = SmartAq::Utils::ArrayActions::RemoveValue<single_stat_settings, stat_uid>;
using retrieve_stat_overview = SmartAq::Utils::ArrayActions::GetValueOverview<single_stat_settings, stat_uid>;

struct retrieve_stat_info {
    unsigned int index = std::numeric_limits<unsigned int>::max();
    std::optional<single_stat_settings> stat_info;

    struct {
        stat_collection_operation collection_result = stat_collection_operation::failed;
    } result;
};

template<size_t N>
class StatCollection final {
    public:
        // TODO: better calculation, which should also be configurable
        using driver_type = stats_driver<N>;
        using event_access_array_type = SmartAq::Utils::EventAccessArray<single_stat_settings, typename driver_type::stat_type, N, stat_uid>;
        using trivial_representation = typename event_access_array_type::TrivialRepresentationType;
        template<typename T>
        using filter_return_type_t = std::conditional_t<!all_unique_v<
            T, 
            set_stat,
            remove_stat,
            retrieve_stat_info,
            retrieve_stat_overview>,
        trivial_representation, ignored_event>;

        StatCollection() = default;
        ~StatCollection() = default;

        StatCollection &operator=(trivial_representation new_value);

        template<typename T>
        filter_return_type_t<T> dispatch(T &event) {}

        template<typename T>
        void dispatch(T &event) const {}

        filter_return_type_t<set_stat> dispatch(set_stat &event);

        filter_return_type_t<remove_stat> dispatch(remove_stat &event);

        void dispatch(retrieve_stat_info &event) const;

        void dispatch(retrieve_stat_overview &event) const;
    private:
        event_access_array_type m_data;
};

template<size_t N>
StatCollection<N> &StatCollection<N>::operator=(trivial_representation new_value) {
    m_data = new_value;

    // TODO: also do this
    // for (size_t i = 0; i < data.initialized.size(); ++i) {
    //     if (data.initialized[i]) {
    //         m_stats[i] = driver_type::create_stat(&data.stat_settings[i]);
    //     }
    // }

    return *this;
}

template<size_t N>
auto StatCollection<N>::dispatch(set_stat &event) -> filter_return_type_t<set_stat> {
    return m_data.template dispatch(event, [&event](auto &currentStat, auto &currentRuntimeData) {
        // First delete timer and then reinitialize it, with old data, patched with new data
        currentStat = std::nullopt;
        currentStat = driver_type::create_stat(event.jsonSettingValue, currentRuntimeData);
        return currentStat.has_value();
    });
}

template<size_t N>
auto StatCollection<N>::dispatch(remove_stat &event) -> filter_return_type_t<remove_stat> {
    return m_data.dispatch(event);
}

template<size_t N>
void StatCollection<N>::dispatch(retrieve_stat_info &event) const {
    /*
    if (event.index < N && data.initialized[event.index]) {
        event.stat_info = data.stat_settings[event.index];
        event.result.collection_result = stat_collection_operation::ok;
    } else {
        event.result.collection_result = stat_collection_operation::index_invalid;
    }*/
}

template<size_t N>
void StatCollection<N>::dispatch(retrieve_stat_overview &event) const {
    /*unsigned int start_index = 0;
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
    event.result.collection_result = stat_collection_operation::ok;*/
}