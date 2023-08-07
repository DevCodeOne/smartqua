#pragma once

#include <array>
#include <optional>
#include <string_view>
#include <mutex>
#include <thread>

#include "build_config.h"
#include "actions/action_types.h"
#include "drivers/device_types.h"
#include "drivers/devices.h"
#include "utils/utils.h"
#include "utils/event_access_array.h"
#include "utils/task_pool.h"
#include "storage/store.h"

bool writeDeviceValue(unsigned int index, std::string_view input, const device_values &value, bool deferSaving = false);
std::optional<device_values> readDeviceValue(unsigned int index, std::string_view input);

json_action_result get_devices_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result get_device_info(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result add_device_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result remove_device_action(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
json_action_result set_device_action(unsigned int index, std::string_view input, char *deviceValueInput, size_t deviceValueLen, char *output_buffer, size_t output_buffer_len);
json_action_result set_device_action(unsigned int index, std::string_view input, const device_values &value, char *output_buffer, size_t output_buffer_len);

json_action_result write_device_options_action(unsigned int index, const char *action, char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);

using DeviceCollectionOperation = collection_operation_result;

// TODO: Fix thid
static inline constexpr size_t device_uid = 30;

struct add_device : public SmartAq::Utils::ArrayActions::SetValue<DeviceConfig, device_uid> { 
    std::string_view driver_name;
};

using remove_single_device = SmartAq::Utils::ArrayActions::RemoveValue<DeviceConfig, device_uid>;

using retrieve_device_overview = SmartAq::Utils::ArrayActions::GetValueOverview<DeviceConfig, device_uid>;

struct read_from_device {
    unsigned int index = std::numeric_limits<unsigned int>::max();
    std::string_view what = "";
    device_values read_value;
    
    struct {
        DeviceOperationResult op_result = DeviceOperationResult::failure;
        DeviceCollectionOperation collection_result = DeviceCollectionOperation::failed;
    } result;
 };

struct write_to_device { 
    unsigned int index = std::numeric_limits<unsigned int>::max();
    std::string_view what = "";
    device_values write_value;
    
    struct {
        DeviceOperationResult op_result = DeviceOperationResult::failure;
        DeviceCollectionOperation collection_result = DeviceCollectionOperation::failed;
    } result;
};

struct retrieve_device_info {
    unsigned int index = std::numeric_limits<unsigned int>::max();
    char *output_dst = nullptr;
    size_t output_len = 0;
    
    struct {
        DeviceOperationResult op_result = DeviceOperationResult::failure;
        DeviceCollectionOperation collection_result = DeviceCollectionOperation::failed;
    } result;
};

struct write_device_options {
    unsigned int index = std::numeric_limits<unsigned int>::max();
    std::string_view action;
    std::string_view input;
    char *output_dst = nullptr;
    size_t output_len = 0;
    
    struct {
        DeviceOperationResult op_result = DeviceOperationResult::failure;
        DeviceCollectionOperation collection_result = DeviceCollectionOperation::failed;
    } result;
};


template<size_t N, typename ... DeviceDrivers>
class DeviceSettings final {
public:
    static inline constexpr size_t num_devices = N;

    // TODO: handle registering and unregistering maybe with special resource class
    DeviceSettings() = default;
    ~DeviceSettings() = default;

    using EventAccessArrayType = SmartAq::Utils::EventAccessArray<DeviceConfig, device<DeviceDrivers ...>, N, device_uid>;
    using TrivialRepresentationType = typename EventAccessArrayType::TrivialRepresentationType;

    template<typename T>
    using FilterReturnType = std::conditional_t<!
        all_unique_v<T,
            add_device,
            remove_single_device,
            write_to_device,
            write_device_options>, const TrivialRepresentationType &, ignored_event>;

    DeviceSettings &operator=(const TrivialRepresentationType &new_value);
    
    // Ignore other read and write events
    template<typename T>
    FilterReturnType<T> dispatch(T &event) {}

    template<typename T>
    void dispatch(T &event) const {}

    FilterReturnType<add_device> dispatch(add_device &event);

    FilterReturnType<remove_single_device> dispatch(remove_single_device &event);

    FilterReturnType<write_to_device> dispatch(write_to_device &event);

    FilterReturnType<write_device_options> dispatch(write_device_options &event);

    void dispatch(read_from_device &event) const;

    void dispatch(retrieve_device_info &event) const;

    void dispatch(retrieve_device_overview &event) const;

    static void updateDeviceRuntime(void *instance);
private:
    void initializeUpdater();

    EventAccessArrayType m_data;
    task_pool<max_task_pool_size>::resource_type m_task_resource;
    std::once_flag initialized_updater_flag;

    // TODO: secure with recursive_mutex
};

template<size_t N, typename ... DeviceDrivers>
DeviceSettings<N, DeviceDrivers ...> &DeviceSettings<N, DeviceDrivers ...>::operator=(const TrivialRepresentationType &new_value) {
    m_data.initialize(new_value, [](const auto &trivialValue, auto &currentRuntimeData) {
        currentRuntimeData = create_device<DeviceDrivers ...>(trivialValue);
        return currentRuntimeData.has_value();
    });
    initializeUpdater();

    return *this;
}

// TODO: this should be done in the constructor, also in the destructor, where the pointers are registered and deregistered
// add helper function for that in the task_pool e.g. unregister_resource
template<size_t N, typename ... DeviceDrivers>
void DeviceSettings<N, DeviceDrivers ...>::initializeUpdater() {
    std::call_once(initialized_updater_flag, [this]() {
        this->m_task_resource = task_pool<max_task_pool_size>::post_task(single_task{
            .single_shot = false,
            .func_ptr = &updateDeviceRuntime,
            .interval = std::chrono::seconds(10),
            .argument = reinterpret_cast<void *>(this),
            .description = "Device Updater thread"
        });
    });
}

template<size_t N, typename ... DeviceDrivers>
void DeviceSettings<N, DeviceDrivers ...>::updateDeviceRuntime(void *instance) {
    auto typeInstance = reinterpret_cast<DeviceSettings<N, DeviceDrivers ...> *>(instance);

    if (typeInstance == nullptr) {
        return;
    }

    Logger::log(LogLevel::Info, "Updating runtimedata");
    typeInstance->m_data.invokeOnAllRuntimeData([](auto &currentRuntimeData) {
        currentRuntimeData.update_runtime_data();
    });
}

template<size_t N, typename ... DeviceDrivers>
auto DeviceSettings<N, DeviceDrivers ...>::dispatch(add_device &event) -> FilterReturnType<add_device> {
    using ArrayEventType = SmartAq::Utils::ArrayActions::SetValue<DeviceConfig, device_uid>;
    return m_data.dispatch(static_cast<ArrayEventType &>(event), 
        [&event](auto &currentDevice, auto &currentTrivialValue, const auto &jsonSettingValue) {
            // First delete possible old device and recreate it
            currentDevice = std::nullopt;
            currentDevice = create_device<DeviceDrivers ...>(event.driver_name, jsonSettingValue, currentTrivialValue);
            return currentDevice.has_value();
        });
}

template<size_t N, typename ... DeviceDrivers>
auto DeviceSettings<N, DeviceDrivers ...>::dispatch(remove_single_device &event) -> FilterReturnType<remove_single_device> {
    return m_data.dispatch(event);
}

template<size_t N, typename ... DeviceDrivers>
auto DeviceSettings<N, DeviceDrivers ...>::dispatch(write_to_device &event) -> FilterReturnType<write_to_device> {
    event.result.collection_result = DeviceCollectionOperation::index_invalid;

    m_data.invokeOnRuntimeData(event.index, [&event](auto &currentDevice) {
        Logger::log(LogLevel::Info, "Writing to device ...");
        event.result.op_result = currentDevice.write_value(event.what, event.write_value);
        event.result.collection_result = DeviceCollectionOperation::ok;
    });

    return m_data.getTrivialRepresentation();
}

template<size_t N, typename ... DeviceDrivers>
auto DeviceSettings<N, DeviceDrivers ...>::dispatch(write_device_options &event) -> FilterReturnType<write_to_device> {
    event.result.collection_result = DeviceCollectionOperation::index_invalid;
    event.output_dst = nullptr;
    event.output_len = 0;

    SmartAq::Utils::ArrayActions::SetValue<DeviceConfig, device_uid> setEvent{
        .index = event.index,
    };

    return m_data.dispatch(setEvent, 
        [&event](auto &currentDevice, auto &currentTrivialValue, const auto &jsonSettingValue) {
            if (!currentDevice) {
                Logger::log(LogLevel::Error, "%s %d current device is not valid", __FUNCTION__, event.index);
                event.result.collection_result = DeviceCollectionOperation::index_invalid;
                return currentDevice.has_value();
            }

            Logger::log(LogLevel::Info, "write_device_options for %d device", event.index);
            event.result.op_result = currentDevice->call_device_action(&currentTrivialValue, event.action, event.input);
            event.result.collection_result = DeviceCollectionOperation::ok;
            return currentDevice.has_value();
        });

}

template<size_t N, typename ... DeviceDrivers>
void DeviceSettings<N, DeviceDrivers ...>::dispatch(read_from_device &event) const {
    event.result.collection_result = DeviceCollectionOperation::index_invalid;

    m_data.invokeOnRuntimeData(event.index, [&event](auto &currentDevice) {
        Logger::log(LogLevel::Info, "Reading from device ...");
        event.result.op_result = currentDevice.read_value(event.what, event.read_value);
        event.result.collection_result = DeviceCollectionOperation::ok;
    });
}


template<size_t N, typename ... DeviceDrivers>
void DeviceSettings<N, DeviceDrivers ...>::dispatch(retrieve_device_info &event) const {
    event.result.collection_result = DeviceCollectionOperation::index_invalid;

    m_data.invokeOnRuntimeData(event.index, [&event](auto &currentDevice) {
        Logger::log(LogLevel::Info, "Reading info from device ...");
        event.result.op_result = currentDevice.get_info(event.output_dst, event.output_len);
        event.result.collection_result = DeviceCollectionOperation::ok;
    });
}

template<size_t N, typename ... DeviceDrivers>
void DeviceSettings<N, DeviceDrivers ...>::dispatch(retrieve_device_overview &event) const {
    m_data.dispatch(event, [](auto &out, const auto &name, const auto &trivialValue, auto index, bool firstPrint) -> int {
        const char *format = ", { index : %u, description : %M, driver_name : %M }";
        return json_printf(&out, format + (firstPrint ? 1 : 0), 
            index,
            json_printf_single<std::decay_t<decltype(name)>>, &name,
            json_printf_single<std::decay_t<decltype(trivialValue.device_driver_name)>>, &trivialValue.device_driver_name);
    });
}