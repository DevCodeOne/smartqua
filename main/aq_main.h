#pragma once

#include <cstdint>

#include "store.h"

enum struct scale_setting_indices { offset, weight, scale, contained_co2 };

struct scale_standard_layout {
    static inline constexpr char name[] = "load_data";

    int32_t offset = 0;
    float scale = 1.0f;
    int32_t contained_co2 = 0;
};

struct scale_event {
    scale_setting_indices type;
    scale_standard_layout data;
};

struct scale_settings {
    using index_type = scale_setting_indices;
    using trivial_represenation = scale_standard_layout;
    template<typename T>
    using filter_return_type = std::conditional_t<std::is_same_v<T, scale_event>, trivial_represenation, ignored_event>;

    scale_settings &operator=(scale_standard_layout init);

    template<typename T>
    filter_return_type<T> dispatch(const T&event) {}

    template<typename T>
    void dispatch(T&event) const {}

    trivial_represenation data;

};

inline scale_settings &scale_settings::operator=(trivial_represenation new_value) {
    data = new_value;
    return *this;
}

template<>
inline auto scale_settings::dispatch<scale_event>(const scale_event &event) -> filter_return_type<scale_event>  {
    switch (event.type) {
        case index_type::contained_co2:
            data.contained_co2 = event.data.contained_co2;
            break;
        case index_type::offset:
            data.offset = event.data.offset;
            break;
        case index_type::scale:
            data.scale = event.data.scale;
            break;
        default:
            break;
    }
    return data;
}

static inline store<single_store<scale_settings, nvs_setting<scale_settings::trivial_represenation>>> global_store;