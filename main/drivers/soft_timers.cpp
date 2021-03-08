#include "soft_timers.h"

soft_timer::soft_timer(single_timer_settings *timer) : m_timer(timer) { }

soft_timer::soft_timer(soft_timer && other) : m_timer(other.m_timer) { other.m_timer = nullptr; };

soft_timer::~soft_timer() { 
    soft_timer_driver::remove_timer(m_timer);
}

soft_timer &soft_timer::operator=(soft_timer &&other) { 
            using std::swap;
            std::swap(m_timer, other.m_timer);
            return *this;
};

std::optional<soft_timer> create_timer(single_timer_settings *timer_settings) {
    if (!soft_timer_driver::add_timer(timer_settings)) {
        return std::nullopt;
    }

    return std::optional<soft_timer>(soft_timer{timer_settings});
}

std::optional<soft_timer> create_timer(std::string_view input, single_timer_settings &timer_settings) {
    json_token token{};
    uint32_t weekday_mask = timer_settings.weekday_mask;
    // hhu doesn't work
    json_scanf(input.data(), input.size(), "{ time_of_day : %u, enabled : %B, weekday_mask : %u, device_index : %u, payload : %T }", 
        &timer_settings.time_of_day, 
        &timer_settings.enabled,
        &weekday_mask,
        &timer_settings.device_index,
        &token);

    timer_settings.weekday_mask = static_cast<uint8_t>(weekday_mask);
    
    if (token.len + 1 > static_cast<int>(timer_settings.payload.size())) {
        ESP_LOGI("Soft_timer_driver", "Payload was too large %d : %d", token.len, timer_settings.payload.size());
        return std::nullopt;
    }

    std::strncpy(timer_settings.payload.data(), token.ptr, token.len);
    *(timer_settings.payload.data() + token.len) = '\0';

    return create_timer(&timer_settings);
}