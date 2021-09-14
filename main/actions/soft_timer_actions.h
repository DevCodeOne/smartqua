#pragma once

#include <array>
#include <optional>
#include <limits>

#include "actions/action_types.h"
#include "drivers/soft_timer_types.h"
#include "drivers/soft_timers.h"
#include "drivers/stats_types.h"
#include "storage/store.h"
#include "utils/event_access_array.h"

json_action_result get_timers_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result add_timer_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result remove_timer_action(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result set_timer_action(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);

using timer_collection_operation = collection_operation_result;

static inline constexpr size_t timer_uid = 50;

using set_timer = SmartAq::Utils::ArrayActions::SetValue<single_timer_settings, timer_uid>;
using remove_timer = SmartAq::Utils::ArrayActions::RemoveValue<single_timer_settings, timer_uid>;
using retrieve_timer_overview = SmartAq::Utils::ArrayActions::GetValueOverview<single_timer_settings, timer_uid>;

struct retrieve_timer_info {
    unsigned int index = std::numeric_limits<unsigned int>::max();

    struct {
        timer_collection_operation collection_result = timer_collection_operation::failed;
        std::optional<single_timer_settings> timer_info;
    } result;
};

template<size_t N>
class soft_timer_settings final {
public:
    using driver_type = soft_timer_driver<N>;
    using event_access_array_type = SmartAq::Utils::EventAccessArray<single_timer_settings, typename driver_type::timer_type, N, timer_uid>;
    using trivial_representation = typename event_access_array_type::TrivialRepresentationType;
    template<typename T>
    using filter_return_type = std::conditional_t<
        !all_unique_v<T,
            set_timer,
            remove_timer>, trivial_representation, ignored_event>;
    
    soft_timer_settings() = default;
    ~soft_timer_settings() = default;
    
    soft_timer_settings &operator=(trivial_representation new_value);

    template<typename T>
    filter_return_type<T> dispatch(T &event) {}

    template<typename T>
    void dispatch(T &event) const {}

    filter_return_type<set_timer> dispatch(set_timer &event);

    filter_return_type<remove_timer> dispatch(remove_timer &event);

    void dispatch(retrieve_timer_info &event) const;

    void dispatch(retrieve_timer_overview &event) const;

private:
    event_access_array_type m_data;
};

template<size_t N>
soft_timer_settings<N> &soft_timer_settings<N>::operator=(trivial_representation new_value) {
    m_data.initialize(new_value, [](const auto &trivialValue, auto &currentRuntimeData) {
        currentRuntimeData = driver_type::create_timer(trivialValue);
        return currentRuntimeData.has_value();
    });

    return *this;
}

template<size_t N>
auto soft_timer_settings<N>::dispatch(set_timer &event) -> filter_return_type<set_timer> {
    return m_data.dispatch(event, [](auto &currentTimer, auto &currentTrivialValue, const auto &jsonSettingValue) {
        // First delete possible old timer and recreate it
        currentTimer = std::nullopt;
        currentTimer = driver_type::create_timer(jsonSettingValue, currentTrivialValue);
        return currentTimer.has_value();
    });
}

template<size_t N>
auto soft_timer_settings<N>::dispatch(remove_timer &event) -> filter_return_type<remove_timer> {
    return m_data.dispatch(event);
}


template<size_t N>
void soft_timer_settings<N>::dispatch(retrieve_timer_info &event) const {
    event.result.collection_result = DeviceCollectionOperation::index_invalid;

    m_data.invokeOnRuntimeData(event.index, [&event](auto &currentTimer) {
        if (currentTimer.get_info()) {
            event.result.timer_info = *currentTimer.get_info();
            event.result.collection_result = DeviceCollectionOperation::ok;
        }
    });
}

template<size_t N>
void soft_timer_settings<N>::dispatch(retrieve_timer_overview &event) const {
    m_data.dispatch(event, [](auto &out, const auto &name, const auto &trivialValue, auto index, bool firstPrint) -> int {
        const char *format = ", { index : %u, description : %M }";
        return json_printf(&out, format + (firstPrint ? 1 : 0), index, json_printf_single<std::decay_t<decltype(name)>>, &name);
    });
}