#pragma once

#include <cstdint>
#include <array>
#include <shared_mutex>
#include <string_view>
#include <optional>

#include "driver/ledc.h"

#include "hal/gpio_types.h"

#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "utils/task_pool.h"

enum struct PinType {
    Input, Output, Pwm, Timed, Invalid
};

template<>
struct read_from_json<PinType> {
    static void read(const char *str, int len, PinType &type) {
        BasicStackString<16> input(std::string_view(str, len));

        type = PinType::Invalid;

        if (input == "Input" || input == "input" || input == "in") {
            type = PinType::Input;
        } else if (input == "Output" || input == "output" || input == "out") {
            type = PinType::Output;
        } else if (input == "PWM" || input == "Pwm" || input == "pwm") {
            type = PinType::Pwm;
        } else if (input == "Timed" || input == "timed") {
            type = PinType::Timed;
        }
    }
};

struct PinConfig {
    PinType type;
    TimerConfig timer_conf {};
    uint16_t frequency = 100;
    ledc_channel_t channel = LEDC_CHANNEL_0;
    // TODO: calculate max_value with resolution bits, is wrong value
    uint16_t max_value = (1 << 10);
    uint8_t gpio_num = static_cast<uint8_t>(gpio_num_t::GPIO_NUM_MAX);
    bool fade = false;
    bool invert = false;
};

class PinDriver final {
    public:
        static constexpr char name[] = "pin_driver";

        PinDriver(const PinDriver &) = delete;
        PinDriver(PinDriver &&) noexcept = default;
        ~PinDriver() = default;

        PinDriver &operator=(const PinDriver &) = delete;
        PinDriver &operator=(PinDriver &&) noexcept = default;

        static std::optional<PinDriver> create_driver(const std::string_view input, DeviceConfig&device_conf_out);
        static std::optional<PinDriver> create_driver(const DeviceConfig*config);


        DeviceOperationResult write_value(std::string_view what, const DeviceValues &value);
        DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const;
        DeviceOperationResult call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;

        DeviceOperationResult update_runtime_data();
    private:
        PinDriver(const DeviceConfig*conf, std::shared_ptr<TimerResource> timer, std::shared_ptr<GpioResource> gpio, std::shared_ptr<LedChannel> channel);

        bool adjustPinLevel(const DeviceValues &value, const PinConfig *pinConf);
        bool adjustPwmOutput(const DeviceValues &value, const PinConfig *pinConf);
        bool adjustTimedValue(const DeviceValues value, const PinConfig *pinConf);

        static std::optional<bool> computePinLevel(
            const DeviceValues &value, const PinConfig *pinConf);
        static bool conditionalInvert(bool currentLevel, bool invert);

        static void resetPinTimed(void *instance);
        static void initPinDriverStatics();

        static std::optional<PinDriver> createPwmConfig(const DeviceConfig *config, PinConfig *pinConf,
                                                        std::shared_ptr<GpioResource> gpio);

        using TimedOutputTaskPool = TaskPool<32u>;

        const DeviceConfig*m_conf = nullptr;
        std::shared_ptr<TimerResource> m_timer = nullptr;
        std::shared_ptr<GpioResource> m_gpio = nullptr;
        std::shared_ptr<LedChannel> m_channel = nullptr;
        uint32_t m_current_value = 0;
        TaskResourceTracker<TimedOutputTaskPool> trackedTasked{};

        static inline TaskPool<32u> _timedOutputTasks;
        static inline std::optional<std::jthread> _timedOutputThread;
};