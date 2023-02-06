#include <string_view>
#include <optional>

#include "schedule_driver.h"

struct Q30Data final {
    ScheduleDriverData data;
    int onSwitchDevice;
    int fanDevice;
};

// values : 
// fan : 0 - 1050 invert: y
// blue : 0-8192 invert: y
// white : 0-8192 invert: y
class Q30Driver final { 
    public:
        static inline constexpr char name[] = "Q30Driver";

        Q30Driver(const Q30Driver &other) = delete;
        Q30Driver(Q30Driver &&other) = default;
        ~Q30Driver() = default;

        Q30Driver &operator=(const Q30Driver &other) = delete;
        Q30Driver &operator=(Q30Driver &&other) = default;

        static std::optional<Q30Driver> create_driver(const std::string_view &input, device_config &device_conf_out);
        static std::optional<Q30Driver> create_driver(const device_config *config);

        DeviceOperationResult write_value(const device_values &value);
        DeviceOperationResult read_value(device_values &value) const;
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;
        DeviceOperationResult call_device_action(device_config *conf, const std::string_view &action, const std::string_view &json);

        DeviceOperationResult update_runtime_data();
    
    private:

        Q30Driver(std::optional<ScheduleDriver> &&lampDriver, const device_config *config);

        const device_config *mConf;
        std::optional<ScheduleDriver> mLampDriver;

        friend struct read_from_json<Q30Driver>;
};

/*
payload : {
    fan_device : 0,
    on_device : 0,
    lamp_driver:
        channels : { "b" : 0, "w" : 1 },
        schedule: {
            // Will only be used on this specific day
            "monday" : "10-00:r=10,b=5;11-00:r=10,b=10;12-00:r=50,b=40;14-00:r=70,b=70;19-00-r=30,b=50;20-00-r=10,b=20;"
            "repeating" : "10-00:r=10,b=5;11-00 r=10,b=10;12:00 r=50,b=40;14:00 r=70,b=70;19:00 r=30,b=50;20:00 r=10,b=20;"
        }
*/