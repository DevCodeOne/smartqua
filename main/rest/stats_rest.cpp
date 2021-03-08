#include "stats_rest.cpp"

#include "ctre.hpp"

static constexpr ctll::fixed_string pattern{R"(\/api\/v1\/stats\/(?<index>[0-9]+)(?:\/(?<what>\w+)|\/)?)"};

esp_err_t do_stats(httpd_req *req) {
    return ESP_OK;
}