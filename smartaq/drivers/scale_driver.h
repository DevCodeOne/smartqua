#pragma once

#include <cmath>
#include <cstdint>
#include <mutex>
#include <string_view>
#include <thread>
#include <utility>
#include <optional>

#include "build_config.h"
#include "hx711.h"
#include "../utils/container/sample_container.h"
#include "utils/task_pool.h"
#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "build_config.h"

enum struct LoadcellStatus { uninitialized, success, failure };

struct LoadcellConfig {
    int32_t offset = 10;
    int32_t scale = 100; // * 100_000
    uint8_t sckGPIONum = 12;
    uint8_t doutGPIONum = 13;
};

class LoadCellDriver final {
   public:
        static inline constexpr char name[] = "LoadcellDriver";

        LoadCellDriver(const LoadCellDriver &other) = delete;
        LoadCellDriver(LoadCellDriver &&other) noexcept;
        ~LoadCellDriver() = default;

        LoadCellDriver &operator=(const LoadCellDriver &other) = delete;
        LoadCellDriver &operator=(LoadCellDriver &&other) noexcept;

        static std::optional<LoadCellDriver> create_driver(const std::string_view input, DeviceConfig&deviceConfOut);
        static std::optional<LoadCellDriver> create_driver(const DeviceConfig*config);

        DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const;
        DeviceOperationResult write_value(std::string_view what, const DeviceValues &value) const;
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;
        DeviceOperationResult call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data();

    private:
        LoadCellDriver(const DeviceConfig*conf, std::shared_ptr<GpioResource> doutGPIO, std::shared_ptr<GpioResource> sckGPIO, hx711_t &&dev);
        static int32_t convertToRealValue(int32_t rawValue, int32_t offset, int32_t scale);

        const DeviceConfig *mConf;

        SampleContainer<int32_t, float, max_sample_size> m_values{};
        std::shared_ptr<GpioResource> mDoutGPIO = nullptr;
        std::shared_ptr<GpioResource> mSckGPIO = nullptr;
        hx711_t mDev;
};
