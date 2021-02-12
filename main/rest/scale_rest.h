#pragma once

#include "esp_http_server.h"
#include "esp_log.h"
#include "frozen.h"

#include "storage/store.h"
#include "storage/settings.h"
#include "drivers/scale.h"

esp_err_t get_load(httpd_req_t *req);
esp_err_t set_contained_co2(httpd_req *req);
esp_err_t tare_scale(httpd_req_t *req);

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
    using trivial_representation = scale_standard_layout;
    template<typename T>
    using filter_return_type = std::conditional_t<std::is_same_v<T, scale_event>, trivial_representation, ignored_event>;

    scale_settings &operator=(trivial_representation init);

    template<typename T>
    filter_return_type<T> dispatch(T &event) {}

    template<typename T>
    void dispatch(T &event) const {}

    trivial_representation data;
};

inline scale_settings &scale_settings::operator=(trivial_representation new_value) {
    data = new_value;
    return *this;
}

template<>
inline void scale_settings::dispatch<scale_event>(scale_event &event) const {
    event.data = data;
}

template<>
inline auto scale_settings::dispatch<scale_event>(scale_event &event) -> filter_return_type<scale_event>  {
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

