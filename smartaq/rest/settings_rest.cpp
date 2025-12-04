#include "settings_rest.h"

#include "ctre.hpp"

static constexpr ctll::fixed_string pattern{R"(\/api\/v1\/settings\/(?<index>[0-9]+)(?:\/(?<what>[\w\-]+)|\/)?)"};

esp_err_t do_settings(httpd_req *req) {
    return ESP_OK;
}
