#pragma once

#include "drivers/device_types.h"

#include <optional>

template<typename Driver>
struct DriverInfo;

template<typename Driver, typename ThisDriverInfo = DriverInfo<Driver>>
concept HasDriverInfo = requires(Driver &instance)
{
    typename ThisDriverInfo::DeviceConfig;
    sizeof(ThisDriverInfo);
    std::is_same_v<decltype(ThisDriverInfo::Name), const char *>;
    { instance.oneIteration() } -> std::same_as<void>;
};

template<typename Driver>
concept IsDriver = HasDriverInfo<Driver>;

// TODO: Add failsafe, restart and more
template<IsDriver Driver>
class SensorDriverInterface {
public:
    using ThisDriverInfo = DriverInfo<Driver>;

    static constexpr auto name = ThisDriverInfo::Name;

    explicit SensorDriverInterface(Driver driver);
    SensorDriverInterface(SensorDriverInterface &&) noexcept;
    SensorDriverInterface &operator=(SensorDriverInterface &&) noexcept;
    ~SensorDriverInterface();

    SensorDriverInterface(const SensorDriverInterface &) = delete;
    SensorDriverInterface &operator=(const SensorDriverInterface &) = delete;

    static std::optional<SensorDriverInterface> create_driver(const std::string_view &input, DeviceConfig &deviceConfigOut);
    static std::optional<SensorDriverInterface> create_driver(const DeviceConfig *deviceConfig);

    DeviceOperationResult write_value(std::string_view what, const DeviceValues &value) { return DeviceOperationResult::not_supported; }
    DeviceOperationResult update_runtime_data();

    DeviceOperationResult call_device_action(DeviceConfig *config, const std::string_view &action, const std::string_view &json) const { return DeviceOperationResult::not_supported; }
    DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const;
    DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;

private:
    std::mutex mThreadMutex;
    std::optional<Driver> mDriver;

    TaskResourceTracker<MainTaskPool> mTrackedTask;

    static void updateThread(void* instance);
};

template <IsDriver Driver>
SensorDriverInterface<Driver>::SensorDriverInterface(Driver driver) : mDriver(std::move(driver))
{
}

template<IsDriver Driver>
SensorDriverInterface<Driver>::SensorDriverInterface(SensorDriverInterface &&other) noexcept {
    mTrackedTask.invalidate();
    other.mTrackedTask.invalidate();

    mDriver = std::move(other.mDriver);
}

template<IsDriver Driver>
SensorDriverInterface<Driver>& SensorDriverInterface<Driver>::operator=(SensorDriverInterface &&other) noexcept {
    using std::swap;

    mTrackedTask.invalidate();
    other.mTrackedTask.invalidate();

    swap(mDriver, other.mDriver);

    return *this;
}

template <IsDriver Driver>
DeviceOperationResult SensorDriverInterface<Driver>::read_value(std::string_view what, DeviceValues& value) const
{
    if (!mDriver)
    {
        return DeviceOperationResult::failure;
    }

    return mDriver->read_value(what, value);
}

template <IsDriver Driver>
DeviceOperationResult SensorDriverInterface<Driver>::get_info(char* output, size_t output_buffer_len) const
{
    if (!mDriver)
    {
        return DeviceOperationResult::failure;
    }

    return mDriver->get_info(output, output_buffer_len);
}

template<IsDriver Driver>
SensorDriverInterface<Driver>::~SensorDriverInterface() {
}

template <IsDriver Driver>
std::optional<SensorDriverInterface<Driver>> SensorDriverInterface<Driver>::create_driver(const std::string_view& input,
    DeviceConfig& deviceConfigOut)
{
    auto driver = Driver::create_driver(input, deviceConfigOut);

    if (!driver)
    {
        return {};
    }

    return SensorDriverInterface(std::move(*driver));
}

template <IsDriver Driver>
std::optional<SensorDriverInterface<Driver>> SensorDriverInterface<Driver>::create_driver(
    const DeviceConfig* deviceConfig)
{
    auto driver = Driver::create_driver(deviceConfig);

    if (!driver)
    {
        return std::nullopt;
    }

    return SensorDriverInterface(std::move(*driver));
}

template <IsDriver Driver>
DeviceOperationResult SensorDriverInterface<Driver>::update_runtime_data()
{
    if (!mTrackedTask.isActive())
    {
        mTrackedTask = MainTaskPool::postTask(TaskDescription{
            .single_shot = false,
            .func_ptr = updateThread,
            .interval = std::chrono::seconds(5),
            .argument = this,
            .description = "Reading from sensor",
            .last_executed = std::chrono::steady_clock::now()
        });
    }
    return DeviceOperationResult::ok;
}

template<IsDriver Driver>
void SensorDriverInterface<Driver>::updateThread(void *ptr) {
    using namespace std::chrono;

    auto* instance = static_cast<SensorDriverInterface*>(ptr);

    if (instance->mDriver.has_value())
    {
        instance->mDriver->oneIteration();
    }
}
