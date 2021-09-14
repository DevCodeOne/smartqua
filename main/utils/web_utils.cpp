#include "web_utils.h"

#include <array>

#include "ctre.hpp"
#include "esp_http_server.h"
#include "esp_log.h"
#include "mbedtls/base64.h"

static constexpr ctll::fixed_string pattern{R"(Basic (\S+))"};

bool handle_authentication(httpd_req *req, std::string_view user, std::string_view password) {
    std::array<char, 256> auth_buf;
    std::array<char, 256> credential_string;
    std::array<unsigned char, 512> credential_string_encoded;
    size_t auth_len = httpd_req_get_hdr_value_len(req, "Authorization");

    auto send_auth_failure = [req]() {
        httpd_resp_set_hdr(req, "WWW-Authenticate", R"(Basic realm="Access to the staging site")");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    };

    // TODO: respond with error
    if (auth_len > auth_buf.size()) {
        send_auth_failure();
        return false;
    }

    auto auth_get_result = httpd_req_get_hdr_value_str(req, "Authorization", auth_buf.data(), auth_buf.size());

    if (auth_get_result != ESP_OK) {
        send_auth_failure();
        return false;
    }

    auto [complete_match, auth_string_encoded] = ctre::match<pattern>(auth_buf.data());

    if (!complete_match || !auth_string_encoded) {
        ESP_LOGI(__PRETTY_FUNCTION__, "Auth line is not well formed %s", auth_buf.data());
        send_auth_failure();
        return false;
    }

    auto credential_string_encoded_length = snprintf(credential_string.data(), credential_string.size(), "%s:%s", user.data(), password.data());
    size_t encoded_bytes = 0;
    mbedtls_base64_encode(
        credential_string_encoded.data(),
        credential_string_encoded.size(),
        &encoded_bytes, 
        reinterpret_cast<unsigned char *>(credential_string.data()),
        credential_string_encoded_length);
    
    std::string_view credential_string_encoded_view(reinterpret_cast<char *>(credential_string_encoded.data()), encoded_bytes);

    ESP_LOGI(__PRETTY_FUNCTION__, "Try to log in using %s %s", credential_string_encoded.data(), auth_string_encoded.data());

    if (auth_string_encoded != credential_string_encoded_view) {
        ESP_LOGW(__PRETTY_FUNCTION__, "Authentication wasn't successfull");
        send_auth_failure();
        return false;
    }

    return true;
}