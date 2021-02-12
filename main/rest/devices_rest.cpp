#include "devices_rest.h"

#include <cstring>
#include <algorithm>

#include "aq_main.h"
#include "utils/http_utils.h"

esp_err_t get_devices(httpd_req *req) {
    auto index = extract_index_from_uri(req->uri);

    if (!index) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not supported right now");
        return ESP_OK;
    }

    std::array<char, 512> buf;
    json_out answer = JSON_OUT_BUF(buf.data(), buf.size());
    int answer_len = 0;

    read_from_device single_device_value{ .index = static_cast<size_t>(*index) };
    global_store.read_event(single_device_value);

    if (single_device_value.read_value.temperature) {
        // TODO: implement following line
        // answer_len = json_printf(&answer, "%M", json_printf_single<std::decay_t<decltype(*single_pwm.data)>>, &(*single_pwm.data));
        answer_len = json_printf(&answer, "{ temperature : %d }", static_cast<int>(single_device_value.read_value.temperature.value() * 1000));
    }

    if (answer_len) {
        send_in_chunks(req, buf, answer_len);
    } else {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "An error happened");
    }

    return ESP_OK;
}

esp_err_t post_device(httpd_req *req) {
    auto index = extract_index_from_uri(req->uri);

    if (!index) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "No index provided");
        return ESP_OK;
    }

    std::array<char, 1024> buf;
    size_t recv_size = std::min(req->content_len, buf.size());

    json_out answer = JSON_OUT_BUF(buf.data(), buf.size());

    if (recv_size == 0) {
        json_printf(&answer, "{ %Q : %s}", "info", "No data provided");
        httpd_resp_sendstr(req, buf.data());

        return ESP_OK;
    }

    if (req->content_len > buf.size()) {
        json_printf(&answer, "{ %Q : %s}", "info", "Data too long");
        httpd_resp_sendstr(req, buf.data());
    
        return ESP_OK;
    }

    httpd_req_recv(req, buf.data(), recv_size);
    json_token token;
    json_token str_token;
    std::array<char, name_length> driver_name{0};

    json_scanf(buf.data(), recv_size, "{ driver_type : %T, driver_param : %T }", &str_token, &token);

    if (str_token.len > name_length) {
        json_printf(&answer, "{ %Q : %s}", "info", "Data too long");
        httpd_resp_sendstr(req, buf.data());
    
        return ESP_OK;
    }

    std::memcpy(driver_name.data(), str_token.ptr, std::min(static_cast<int>(name_length), str_token.len));
    
    add_device to_add {};
    to_add.index = static_cast<size_t>(*index);
    to_add.driver_name = driver_name.data();
    to_add.json_input = token.ptr;
    to_add.json_len = token.len;

    global_store.write_event(to_add);

    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Whatever");
    return ESP_OK;
}

esp_err_t set_device(httpd_req *req) {
    return ESP_OK;
}

esp_err_t remove_device(httpd_req *req) {
    auto index = extract_index_from_uri(req->uri);
    std::array<char, 512> buf;
    json_out answer = JSON_OUT_BUF(buf.data(), buf.size());

    if (!index) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Output index is not range");
        return ESP_OK;
    }

    remove_single_device del_device{ .index = static_cast<size_t>(*index) };
    global_store.write_event(del_device);

    json_printf(&answer, "{ %Q : %Q }", "info", "OK");
    httpd_resp_sendstr(req, buf.data());

    return ESP_OK;
}