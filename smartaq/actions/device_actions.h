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
#include "utils/container/event_access_array.h"
#include "utils/task_pool.h"
#include "storage/store.h"

using IndexOrName = SmartAq::Utils::IndexOrName;
using IndexOrNameOrEmpty = SmartAq::Utils::IndexOrNameOrEmpty;

bool writeDeviceValue(IndexOrName index, std::string_view input, const DeviceValues &value, bool deferSaving = false);
std::optional<DeviceValues> readDeviceValue(IndexOrName index, std::string_view input);

JsonActionResult get_devices_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
JsonActionResult get_device_info(IndexOrName index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
JsonActionResult add_device_action(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
JsonActionResult remove_device_action(IndexOrName index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
JsonActionResult set_device_action(IndexOrName index, std::string_view what, char *deviceValueInput, size_t deviceValueLen, char *output_buffer, size_t output_buffer_len);
JsonActionResult set_device_action(IndexOrName index, std::string_view what, const DeviceValues &value, char *output_buffer, size_t output_buffer_len);

JsonActionResult write_device_options_action(IndexOrName index, const char *action, char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);

using DeviceCollectionOperation = CollectionOperationResult;

// TODO: Fix this
static inline constexpr size_t device_uid = 30;

struct AddDevice : SmartAq::Utils::ArrayActions::SetValue<DeviceConfig, device_uid> {
    std::string_view driver_name;
};

using RemoveSingleDevice = SmartAq::Utils::ArrayActions::RemoveValue<DeviceConfig, device_uid>;

using RetrieveDeviceOverview = SmartAq::Utils::ArrayActions::GetValueOverview<DeviceConfig, device_uid>;

struct ReadFromDevice {
    IndexOrName index = std::numeric_limits<unsigned int>::max();
    std::string_view what = "";
    DeviceValues read_value;
    
    struct {
        DeviceOperationResult op_result = DeviceOperationResult::failure;
        DeviceCollectionOperation collection_result = DeviceCollectionOperation::failed;
    } result;
 };

struct WriteToDevice {
    IndexOrName index = std::numeric_limits<unsigned int>::max();
    std::string_view what = "";
    DeviceValues write_value;
    
    struct {
        DeviceOperationResult op_result = DeviceOperationResult::failure;
        DeviceCollectionOperation collection_result = DeviceCollectionOperation::failed;
    } result;
};

struct RetrieveDeviceInfo {
    IndexOrName index = std::numeric_limits<unsigned int>::max();
    char *output_dst = nullptr;
    size_t output_len = 0;
    
    struct {
        DeviceOperationResult op_result = DeviceOperationResult::failure;
        DeviceCollectionOperation collection_result = DeviceCollectionOperation::failed;
    } result;
};

struct WriteDeviceOptions {
    IndexOrName index = std::numeric_limits<unsigned int>::max();
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
    static constexpr size_t num_devices = N;

    // TODO: handle registering and unregistering maybe with special resource class
    DeviceSettings() = default;
    ~DeviceSettings() = default;

    using EventAccessArrayType = SmartAq::Utils::EventAccessArray<DeviceConfig, device<DeviceDrivers ...>, N, device_uid>;
    using TrivialRepresentationType = typename EventAccessArrayType::TrivialRepresentationType;

    DeviceSettings &operator=(const TrivialRepresentationType &new_value);
    
    // Ignore other read and write events
    template<typename T>
    IgnoredEvent dispatch(T &) {}

    template<typename T>
    void dispatch(T &) const {}

    const TrivialRepresentationType &dispatch(AddDevice &event);

    const TrivialRepresentationType &dispatch(RemoveSingleDevice &event);

    const TrivialRepresentationType &dispatch(WriteToDevice &event);

    const TrivialRepresentationType &dispatch(WriteDeviceOptions &event);

    void dispatch(ReadFromDevice &event) const;

    void dispatch(RetrieveDeviceInfo &event) const;

    void dispatch(RetrieveDeviceOverview &event) const;

    static void updateDeviceRuntime(void *instance);
private:
    void initializeUpdater();

    EventAccessArrayType m_data;
    MainTaskPool::TaskResourceType m_task_resource;
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
// add helper function for that in the TaskPool e.g. unregister_resource
template<size_t N, typename ... DeviceDrivers>
void DeviceSettings<N, DeviceDrivers ...>::initializeUpdater() {
    std::call_once(initialized_updater_flag, [this]() {
        this->m_task_resource = MainTaskPool::postTask(TaskDescription{
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
    auto typeInstance = static_cast<DeviceSettings *>(instance);
    if (typeInstance == nullptr) {
        return;
    }

    Logger::log(LogLevel::Info, "Updating runtimedata");
    typeInstance->m_data.invokeOnAllRuntimeData([](auto &currentRuntimeData) {
        currentRuntimeData.update_runtime_data();
    });
}

template<size_t N, typename ... DeviceDrivers>
auto DeviceSettings<N, DeviceDrivers ...>::dispatch(AddDevice &event) -> const TrivialRepresentationType & {
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
auto DeviceSettings<N, DeviceDrivers ...>::dispatch(RemoveSingleDevice &event) -> const TrivialRepresentationType & {
    return m_data.dispatch(event);
}

template<size_t N, typename ... DeviceDrivers>
auto DeviceSettings<N, DeviceDrivers ...>::dispatch(WriteToDevice &event) -> const TrivialRepresentationType & {
    event.result.collection_result = DeviceCollectionOperation::index_invalid;

    m_data.invokeOnRuntimeData(event.index, [&event](auto &currentDevice) {
        Logger::log(LogLevel::Info, "Writing to device ...");
        event.result.op_result = currentDevice.write_value(event.what, event.write_value);
        event.result.collection_result = DeviceCollectionOperation::ok;
    });

    return m_data.getTrivialRepresentation();
}

template<size_t N, typename ... DeviceDrivers>
auto DeviceSettings<N, DeviceDrivers ...>::dispatch(WriteDeviceOptions &event) -> const TrivialRepresentationType &{
    event.result.collection_result = DeviceCollectionOperation::index_invalid;
    event.output_dst = nullptr;
    event.output_len = 0;

    IndexOrNameOrEmpty setIndexVariant;
    convertToVariant(event.index, setIndexVariant);
    SmartAq::Utils::ArrayActions::SetValue<DeviceConfig, device_uid> setEvent{
        .index = setIndexVariant,
    };

    return m_data.dispatch(setEvent, 
        [&event, funcName = __FUNCTION__](auto &currentDevice, auto &currentTrivialValue, const auto &jsonSettingValue) {
            if (!currentDevice) {
                Logger::log(LogLevel::Error, "%s %d current device is not valid", funcName, event.index);
                event.result.collection_result = DeviceCollectionOperation::index_invalid;
                return currentDevice.has_value();
            }

            Logger::log(LogLevel::Info, "WriteDeviceOptions for %d device", event.index);
            event.result.op_result = currentDevice->call_device_action(&currentTrivialValue, event.action, event.input);
            event.result.collection_result = DeviceCollectionOperation::ok;
            return currentDevice.has_value();
        });

}

template<size_t N, typename ... DeviceDrivers>
void DeviceSettings<N, DeviceDrivers ...>::dispatch(ReadFromDevice &event) const {
    event.result.collection_result = DeviceCollectionOperation::index_invalid;

    m_data.invokeOnRuntimeData(event.index, [&event](auto &currentDevice) {
        Logger::log(LogLevel::Info, "Reading from device ...");
        event.result.op_result = currentDevice.read_value(event.what, event.read_value);
        event.result.collection_result = DeviceCollectionOperation::ok;
    });
}


template<size_t N, typename ... DeviceDrivers>
void DeviceSettings<N, DeviceDrivers ...>::dispatch(RetrieveDeviceInfo &event) const {
    event.result.collection_result = DeviceCollectionOperation::index_invalid;

    m_data.invokeOnRuntimeData(event.index, [&event](auto &currentDevice) {
        Logger::log(LogLevel::Info, "Reading info from device ...");
        event.result.op_result = currentDevice.get_info(event.output_dst, event.output_len);
        event.result.collection_result = DeviceCollectionOperation::ok;
    });
}

template<size_t N, typename ... DeviceDrivers>
void DeviceSettings<N, DeviceDrivers ...>::dispatch(RetrieveDeviceOverview &event) const {
    m_data.dispatch(event, []<typename NameType>(auto &out, const NameType &name, const auto &trivialValue, auto index, bool firstPrint) -> int {
        auto format = ", { index : %u, description : %M, driver_name : %M }";
        return json_printf(&out, format + (firstPrint ? 1 : 0), 
            index,
            json_printf_single<NameType>, &name,
            json_printf_single<std::decay_t<decltype(trivialValue.device_driver_name)>>, &trivialValue.device_driver_name);
    });
}