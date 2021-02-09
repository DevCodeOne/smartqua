#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "esp_http_server.h"
#include "esp_log.h"
#include "frozen.h"

#include "store.h"
#include "pwm.h"
#include "utils.h"
#include "smartqua_config.h"
#include "json_utils.h"

esp_err_t get_pwm_outputs(httpd_req *req);
esp_err_t post_pwm_output(httpd_req *req);
esp_err_t set_pwm_output(httpd_req *req);

// TODO: add name length

enum struct pwm_setting_indices {
    frequency, timer, channel, max_value, gpio_num, fade
};

template<size_t N>
struct pwm_setting_trivial {
    static inline constexpr char name[] = "pwm_config";

    std::array<pwm_config, N> data;
    std::array<std::array<char, name_length>, N> description;
    std::array<bool, N> initialized;
};

template<size_t N>
struct print_to_json<pwm_setting_trivial<N>> {
    static int print(json_out *out, pwm_setting_trivial<N> &setting) {
        return json_printf(out, "%M", json_printf_array<decltype(setting.data)>, &setting.data);
    }
};

template<>
struct print_to_json<pwm_config> {
    static int print(json_out *out, pwm_config &config) {
        return json_printf(out, "{ %Q : %d, %Q : %d, %Q : %d, %Q : %d, %Q : %d, %Q : %B }",
        "frequency", config.frequency, "timer", static_cast<int>(config.timer),
        "channel", static_cast<int>(config.channel),
        "max_value", static_cast<int>(config.max_value),
        "gpio_num", static_cast<int>(config.gpio_num),
        "fade", config.fade);
    }
};

struct update_pwm {
    size_t index = std::numeric_limits<size_t>::max();
    pwm_setting_indices type;
    pwm_config data;
};

struct add_pwm_output {
    pwm_setting_indices type;
    pwm_config data;
    std::array<char, name_length> description;
};

struct remove_pwm_output {
    size_t index = std::numeric_limits<size_t>::max();
};

struct retrieve_single_pwm {
    size_t index = std::numeric_limits<size_t>::max();
    std::optional<pwm_config> data{std::nullopt};
};

template<size_t N>
struct retrieve_overview_pwm {
    pwm_setting_trivial<N> data;
    bool full = false;
};

class pwm_settings {
public:
    static inline constexpr size_t num_outputs = 15;

    using index_type  = pwm_setting_indices;
    using trivial_representation = pwm_setting_trivial<num_outputs>;
    template<typename T>
    using filter_return_type = std::conditional_t<!
        all_unique_v<T, 
            retrieve_single_pwm,
            update_pwm,
            add_pwm_output,
            remove_pwm_output>, trivial_representation, ignored_event>;

    pwm_settings &operator=(trivial_representation new_value);

    template<typename T>
    filter_return_type<T> dispatch(T &event) {}

    template<typename T>
    void dispatch(T &event) const {}

private:
    trivial_representation data;
    std::array<std::optional<pwm>, num_outputs> outputs;
};

// TODO: add other events
template<>
inline void pwm_settings::dispatch(retrieve_single_pwm &event) const {
    if (event.index < num_outputs) {
        event.data = data.data[event.index];
    }
}

inline pwm_settings &pwm_settings::operator=(trivial_representation new_value) {
    data = new_value;

    for (size_t i = 0; i < data.initialized.size(); ++i) {
        if (data.initialized[i]) {
            outputs[i] = create_pwm_output(data.data[i]);
        }
    }
    return *this;
}