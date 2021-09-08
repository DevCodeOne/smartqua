#pragma once

#include <array>
#include <optional>
#include <string_view>

#include "smartqua_config.h"
#include "actions/action_types.h"
#include "drivers/device_types.h"
#include "drivers/devices.h"
#include "utils/utils.h"
#include "utils/event_access_array.h"
#include "storage/store.h"

json_action_result get_devices_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result get_device_info(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result add_device_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result remove_device_action(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result set_device_action(unsigned int index, char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);

using device_collection_operation = collection_operation_result;

static inline constexpr size_t device_uid = 30;

struct add_device : public SmartAq::Utils::ArrayActions::SetValue<device_config, device_uid> { 
    const char *driver_name = nullptr;
};

using remove_single_device = SmartAq::Utils::ArrayActions::RemoveValue<device_config, device_uid>;

using retrieve_device_overview = SmartAq::Utils::ArrayActions::GetValueOverview<device_config, device_uid>;

struct read_from_device {
    unsigned int index = std::numeric_limits<unsigned int>::max();
    device_values read_value;
    
    struct {
        device_operation_result op_result = device_operation_result::failure;
        device_collection_operation collection_result = device_collection_operation::failed;
    } result;
 };

struct write_to_device { 
    unsigned int index = std::numeric_limits<unsigned int>::max();
    device_values write_value;
    
    struct {
        device_operation_result op_result = device_operation_result::failure;
        device_collection_operation collection_result = device_collection_operation::failed;
    } result;
};

struct write_device_options {
    unsigned int index = std::numeric_limits<unsigned int>::max();
    std::string_view jsonSettingValue;

    struct {
        device_operation_result op_result = device_operation_result::failure;
        device_collection_operation collection_result = device_collection_operation::failed;
    } result;
};

struct retrieve_device_info {
    unsigned int index = std::numeric_limits<unsigned int>::max();
    char *output_dst = nullptr;
    size_t output_len = 0;
    
    struct {
        device_operation_result op_result = device_operation_result::failure;
        device_collection_operation collection_result = device_collection_operation::failed;
    } result;
};
template<size_t N, typename ... DeviceDrivers>
class device_settings final {
public:
    static inline constexpr size_t num_devices = N;

    device_settings() = default;
    ~device_settings() = default;

    using event_access_array_type = SmartAq::Utils::EventAccessArray<device_config, device<DeviceDrivers ...>, N, device_uid>;
    using trivial_representation = typename event_access_array_type::TrivialRepresentationType;

    template<typename T>
    using filter_return_type = std::conditional_t<!
        all_unique_v<T,
            add_device,
            remove_single_device,
            write_to_device,
            write_device_options>, trivial_representation, ignored_event>;

    device_settings &operator=(const trivial_representation &new_value);
    
    // Ignore other read and write events
    template<typename T>
    filter_return_type<T> dispatch(T &event) {}

    template<typename T>
    void dispatch(T &event) const {}

    filter_return_type<add_device> dispatch(add_device &event);

    filter_return_type<remove_single_device> dispatch(remove_single_device &event);

    filter_return_type<write_to_device> dispatch(write_to_device &event);

    filter_return_type<write_device_options> dispatch(write_device_options &event);

    void dispatch(read_from_device &event) const;

    void dispatch(retrieve_device_info &event) const;

    void dispatch(retrieve_device_overview &event) const;
private:
    event_access_array_type m_data;
};

template<size_t N, typename ... DeviceDrivers>
device_settings<N, DeviceDrivers ...> &device_settings<N, DeviceDrivers ...>::operator=(const trivial_representation &new_value) {
    m_data.initialize(new_value, [](const auto &trivialValue, auto &currentRuntimeData) {
        currentRuntimeData = create_device<DeviceDrivers ...>(trivialValue);
        return currentRuntimeData.has_value();
    });

    return *this;
}

template<size_t N, typename ... DeviceDrivers>
auto device_settings<N, DeviceDrivers ...>::dispatch(add_device &event) -> filter_return_type<add_device> {
    using ArrayEventType = SmartAq::Utils::ArrayActions::SetValue<device_config, device_uid>;
    return m_data.dispatch(static_cast<ArrayEventType &>(event), 
        [&event](auto &currentDevice, auto &currentTrivialValue, const auto &jsonSettingValue) {
            // First delete possible old device and recreate it
            currentDevice = std::nullopt;
            currentDevice = create_device<DeviceDrivers ...>(event.driver_name, jsonSettingValue, currentTrivialValue);
            return true;
        });
}

template<size_t N, typename ... DeviceDrivers>
auto device_settings<N, DeviceDrivers ...>::dispatch(remove_single_device &event) -> filter_return_type<remove_single_device> {
    return m_data.dispatch(event);
}

template<size_t N, typename ... DeviceDrivers>
auto device_settings<N, DeviceDrivers ...>::dispatch(write_to_device &event) -> filter_return_type<write_to_device> {
    event.result.collection_result = device_collection_operation::index_invalid;

    m_data.invokeOnRuntimeData(event.index, [&event](auto &currentDevice) {
        ESP_LOGI("Device_Settings", "Writing to device ...");
        event.result.op_result = currentDevice.write_value(event.write_value);
        event.result.collection_result = device_collection_operation::ok;
    });

    return m_data.getTrivialRepresentation();
}

template<size_t N, typename ... DeviceDrivers>
auto device_settings<N, DeviceDrivers ...>::dispatch(write_device_options &event) -> filter_return_type<write_to_device> {
    event.result.collection_result = device_collection_operation::index_invalid;

    m_data.invokeOnRuntimeData(event.index, [&event](auto &currentDevice) {
        ESP_LOGI("Device_Settings", "Writing to device ...");
        event.result.op_result = currentDevice.write_options(event.jsonSettingValue);
        event.result.collection_result = device_collection_operation::ok;
    });

    return m_data.getTrivialRepresentation();
}

template<size_t N, typename ... DeviceDrivers>
void device_settings<N, DeviceDrivers ...>::dispatch(read_from_device &event) const {
    event.result.collection_result = device_collection_operation::index_invalid;

    m_data.invokeOnRuntimeData(event.index, [&event](auto &currentDevice) {
        ESP_LOGI("Device_Settings", "Reading from device ...");
        event.result.op_result = currentDevice.read_value(event.read_value);
        event.result.collection_result = device_collection_operation::ok;
    });
}

template<size_t N, typename ... DeviceDrivers>
void device_settings<N, DeviceDrivers ...>::dispatch(retrieve_device_info &event) const {
    event.result.collection_result = device_collection_operation::index_invalid;

    m_data.invokeOnRuntimeData(event.index, [&event](auto &currentDevice) {
        ESP_LOGI("Device_Settings", "Reading from device ...");
        event.result.op_result = currentDevice.get_info(event.output_dst, event.output_len);
        event.result.collection_result = device_collection_operation::ok;
    });
}

template<size_t N, typename ... DeviceDrivers>
void device_settings<N, DeviceDrivers ...>::dispatch(retrieve_device_overview &event) const {
    m_data.dispatch(event, [](auto &out, const auto &name, const auto &trivialValue, auto index, bool firstPrint) -> int {
        const char *format = ", { index : %u, description : %M, driver_name : %M }";
        return json_printf(&out, format + (firstPrint ? 1 : 0), 
            index,
            json_printf_single<std::decay_t<decltype(name)>>, &name,
            json_printf_single<std::decay_t<decltype(trivialValue.device_driver_name)>>, &trivialValue.device_driver_name);
    });
}