#pragma once

#include <array>
#include <optional>
#include <limits>
#include <string_view>

#include "drivers/stats_types.h"
#include "build_config.h"
#include "actions/action_types.h"
#include "drivers/stats_driver.h"
#include "storage/store.h"
#include "utils/event_access_array.h"

using stat_collection_operation = CollectionOperationResult;

json_action_result get_stats_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result add_stat_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result remove_stat_action(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result set_stat_action(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);

static inline constexpr size_t stat_uid = 20;

using set_stat = SmartAq::Utils::ArrayActions::SetValue<single_stat_settings, stat_uid>;
using remove_stat = SmartAq::Utils::ArrayActions::RemoveValue<single_stat_settings, stat_uid>;
using retrieve_stat_info = SmartAq::Utils::ArrayActions::GetValue<single_stat_settings, stat_uid>;
using retrieve_stat_overview = SmartAq::Utils::ArrayActions::GetValueOverview<single_stat_settings, stat_uid>;

template<size_t N>
class StatCollection final {
    public:
        // TODO: better calculation, which should also be configurable
        using driver_type = stats_driver<N>;
        using event_access_array_type = SmartAq::Utils::EventAccessArray<single_stat_settings, typename driver_type::stat_type, N, stat_uid>;
        using trivial_representation = typename event_access_array_type::TrivialRepresentationType;
        template<typename T>
        using filter_return_type_t = std::conditional_t<!AllUniqueV<
            T, 
            set_stat,
            remove_stat,
            retrieve_stat_info,
            retrieve_stat_overview>,
        trivial_representation, IgnoredEvent>;

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
    m_data.initialize(new_value, [](const auto &trivialValue, auto &currentRuntimeData) {
        currentRuntimeData = driver_type::create_stat(trivialValue);
        return currentRuntimeData.has_value();
    });

    return *this;
}

template<size_t N>
auto StatCollection<N>::dispatch(set_stat &event) -> filter_return_type_t<set_stat> {
    return m_data.dispatch(event, [&event](auto &currentStat, auto &currentTrivialValue, const auto &jsonSettingValue) {
        // First delete timer and then reinitialize it, with old data, patched with new data
        currentStat = std::nullopt;
        currentStat = driver_type::create_stat(jsonSettingValue, currentTrivialValue);
        if (!currentStat.has_value()) {
            Logger::log(LogLevel::Warning, "Couldn't create stat with data %s", jsonSettingValue.data());
        }
        return currentStat.has_value();
    });
}

template<size_t N>
auto StatCollection<N>::dispatch(remove_stat &event) -> filter_return_type_t<remove_stat> {
    return m_data.dispatch(event);
}

template<size_t N>
void StatCollection<N>::dispatch(retrieve_stat_info &event) const {
    m_data.dispatch(event);
}

template<size_t N>
void StatCollection<N>::dispatch(retrieve_stat_overview &event) const {
    m_data.dispatch(event);
}