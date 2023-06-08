#include "stats_rest.h"

#include <array>
#include <cstdint>

#include "ctre.hpp"
#include "frozen.h"

#include "utils/web_utils.h"
#include "utils/logger.h"
#include "actions/stats_actions.h"
#include "build_config.h"

static constexpr ctll::fixed_string pattern{R"(\/api\/v1\/stats\/(?<index>[0-9]+)(?:\/(?<what>\w+)|\/)?)"};

esp_err_t do_stats(httpd_req *req) {
    Logger::log(LogLevel::Info, "Handle uri %s", req->uri);
    std::array<char, 2048> buf;
    json_action_result result{};
    auto [complete_match, index, what] = ctre::match<pattern>(req->uri);

    if (req->content_len > buf.size()) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Input too long");
    }

    httpd_req_recv(req, buf.data(), req->content_len);

    if (index) {
        auto index_value = std::atoi(index.to_view().data());
        
        if (req->method == HTTP_GET) {
            result = get_stats_action(index_value, buf.data(), req->content_len, buf.data(), buf.size());
        } else if (req->method == HTTP_PUT) {
            result = add_stat_action(index_value, buf.data(), req->content_len, buf.data(), buf.size());
        } else if (req->method == HTTP_DELETE) {
            result = remove_stat_action(index_value, buf.data(), req->content_len, buf.data(), buf.size());
        } else if (req->method == HTTP_PATCH) {
            result = set_stat_action(index_value, buf.data(), req->content_len, buf.data(), buf.size());
        }

    } else if (!index && (req->method == HTTP_POST || req->method == HTTP_GET)) {
        if (req->method == HTTP_GET) {
            result = get_stats_action(std::nullopt, buf.data(), req->content_len, buf.data(), buf.size());
        } else if (req->method == HTTP_POST) {
            result = add_stat_action(std::nullopt, buf.data(), req->content_len, buf.data(), buf.size());
        }
    }

    if (result.answer_len) {
        send_in_chunks(req, buf, result.answer_len);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "An error happened");
    }

    return ESP_OK;
}