#pragma once

#include "drivers/device_types.h"

#include <optional>

template<typename Driver>
struct DriverInfo;

template<typename Driver>
class DriverInterface {
public:
    using ThisDriverInfo = DriverInfo<Driver>;

    using ThisDeviceConfig = typename ThisDriverInfo::DeviceConfig;
    static constexpr auto name = ThisDriverInfo::Name;

    DriverInterface(const DeviceConfig *conf);
    DriverInterface(DriverInterface &&) noexcept;
    DriverInterface &operator=(DriverInterface &&) noexcept;
    ~DriverInterface();

    DriverInterface(const DriverInterface &) = delete;
    DriverInterface &operator=(const DriverInterface &) = delete;

    static std::optional<Driver> create_driver(const std::string_view &data, DeviceConfig &deviceConfigOut) { return std::nullopt; }
    static std::optional<Driver> create_driver(const DeviceConfig *deviceConfig) { return std::nullopt; }

    DeviceOperationResult write_value(std::string_view what, const DeviceValues &value) { return DeviceOperationResult::not_supported; }
    DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const { return DeviceOperationResult::not_supported; }
    DeviceOperationResult get_info(char *output, size_t output_buffer_len) const { return DeviceOperationResult::ok; }
    DeviceOperationResult call_device_action(DeviceConfig *config, const std::string_view &action, const std::string_view &json) const { return DeviceOperationResult::not_supported; }
    DeviceOperationResult update_runtime_data() { return DeviceOperationResult::ok; }
protected:

    void startThread();

    const DeviceConfig *mConf;
private:
    std::jthread mUpdateThread;
    std::mutex mThreadMutex;

    void startThreadImpl();
    static void updateThread(std::stop_token token, std::mutex *lockedLock, DriverInterface *instance);

};

template<typename Driver>
DriverInterface<Driver>::DriverInterface(const DeviceConfig *conf) : mConf(conf) { }

template<typename Driver>
DriverInterface<Driver>::DriverInterface(DriverInterface &&other) noexcept : mConf(other.mConf) {
    if (other.mUpdateThread.joinable()) {
        other.mUpdateThread.join();
    }

    if (mUpdateThread.joinable()) {
        mUpdateThread.join();
    }
    other.mConf = nullptr;
}

template<typename Driver>
DriverInterface<Driver> & DriverInterface<Driver>::operator=(DriverInterface &&other) noexcept {
    using std::swap;

    if (mUpdateThread.joinable()) {
        mUpdateThread.join();
    }

    if (other.mUpdateThread.joinable()) {
        other.mUpdateThread.join();
    }

    swap(mConf, other.mConf);

    return *this;
}

template<typename Driver>
DriverInterface<Driver>::~DriverInterface() {
    if (mUpdateThread.joinable()) {
        mUpdateThread.join();
    }
}

template<typename Driver>
void DriverInterface<Driver>::startThread() {
    startThreadImpl();
}

template<typename Driver>
void DriverInterface<Driver>::startThreadImpl() {
    if (mThreadMutex.try_lock()) {
        mUpdateThread = std::jthread(DriverInterface<Driver>::updateThread, &mThreadMutex, this);
    }
}

template<typename Driver>
void DriverInterface<Driver>::updateThread(std::stop_token token, std::mutex *threadRunMutex, DriverInterface *instance) {
    using namespace std::chrono;
    std::lock_guard threadMutex(*threadRunMutex, std::adopt_lock);
    while (!token.stop_requested()) {
        const auto startTime = std::chrono::steady_clock::now();
        Driver::oneIteration(static_cast<Driver *>(instance));
        // TODO: check time for execution
        const auto iterationTime = std::chrono::steady_clock::now() - startTime;

        if (iterationTime < 500ms) {
            std::this_thread::sleep_for(500ms - iterationTime);
        }
    }
}
