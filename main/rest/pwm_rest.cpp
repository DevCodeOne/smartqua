#include <cstdio>

#include "aq_main.h"
#include "rest/pwm_rest.h"
#include "utils/json_utils.h"
#include "utils/http_utils.h"

// TODO: print lightweight version
esp_err_t get_pwm_outputs(httpd_req *req) {
    auto index = extract_index_from_uri(req->uri);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    std::array<char, 2048> buf;
    json_out answer = JSON_OUT_BUF(buf.data(), buf.size());
    int answer_len = 0;

    if (index) {
        retrieve_single_pwm single_pwm{ .index = static_cast<size_t>(*index) };
        global_store.read_event(single_pwm);

        if (single_pwm.data) {
            answer_len = json_printf(&answer, "%M", json_printf_single<std::decay_t<decltype(*single_pwm.data)>>, &(*single_pwm.data));
        }
    } else {
        // TODO: add possebillity to only request overview
        retrieve_overview_pwm<pwm_settings::num_outputs> pwm_overiew{.full = true};
        global_store.read_event(pwm_overiew);
        answer_len = json_printf(&answer, "{ %Q : %M }", "pwm_outputs", json_printf_single<decltype(pwm_overiew.data)>, &pwm_overiew.data);
    }

    if (answer_len) {
        send_in_chunks(req, buf, answer_len);
    } else {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Couldn't find this output");
    }

    return ESP_OK;
}

esp_err_t post_pwm_output(httpd_req *req) {
    auto index = extract_index_from_uri(req->uri);
    std::array<char, 512> buf;
    size_t recv_size = std::min(req->content_len, buf.size());

    json_out answer = JSON_OUT_BUF(buf.data(), buf.size());

    if (!index) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Output index is not range");
        return ESP_OK;
    }

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

    // TODO: maybe initialize from struct ?
    int frequency = 0, timer = 0, channel = 0, max_value = 0, current_value = 0, gpio_num = 0;
    bool fade;
    char description[name_length];

    json_scanf(buf.data(), recv_size, "{ frequency: %d, timer : %d, channel : %d, max_value : %d, current_value : %d, gpio_num : %d, fade : %B, description : %s }", 
        &frequency, &timer, &channel, &max_value, &current_value, &gpio_num, &fade, description);

    add_pwm_output to_add{ 
        .data{
            .frequency = static_cast<uint16_t>(frequency),
            .channel = static_cast<ledc_channel_t>(channel),
            .max_value = static_cast<uint16_t>(max_value),
            .current_value = static_cast<uint16_t>(current_value),
            .gpio_num = static_cast<uint8_t>(gpio_num),
            .fade = fade
        }, 
        .index = static_cast<size_t>(*index),
        .description{""}
    };

    std::strncpy(to_add.description.data(), description, to_add.description.size());

    // TODO: verifiy inputs maybe ?

    global_store.write_event(to_add);

    retrieve_single_pwm result_pwm{ .index = static_cast<size_t>(*index)};
    global_store.read_event(result_pwm);

    if (!result_pwm.data.has_value()) {
        json_printf(&answer, "{ %Q : %Q }", "info", "Couldn't create pwm output");
    } else {
        json_printf(&answer, "{ %Q : %Q, %Q, %M }", "info", "OK", "object", json_printf_single<std::decay_t<decltype(*result_pwm.data)>>, &*result_pwm.data);
    }

    httpd_resp_sendstr(req, buf.data());

    return ESP_OK;
}

esp_err_t delete_pwm_output(httpd_req *req) {
    auto index = extract_index_from_uri(req->uri);
    std::array<char, 512> buf;
    json_out answer = JSON_OUT_BUF(buf.data(), buf.size());

    if (!index) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Output index is not range");
        return ESP_OK;
    }

    remove_pwm_output del_output{ .index = static_cast<size_t>(*index) };
    global_store.write_event(del_output);

    // TODO: check success of delete event

    json_printf(&answer, "{ %Q : %Q }", "info", "OK");
    httpd_resp_sendstr(req, buf.data());

    return ESP_OK;
}

esp_err_t set_pwm_output(httpd_req *req) {
    auto index = extract_index_from_uri(req->uri);
    std::array<char, 512> buf;
    size_t recv_size = std::min(req->content_len, buf.size());
    json_out answer = JSON_OUT_BUF(buf.data(), buf.size());

    if (!index) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Output index is not in range");
    }

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

    int current_value = 0;
    int read = json_scanf(buf.data(), recv_size, "{ current_value : %d }", &current_value);

    if (read == 0) {
        json_printf(&answer, "{ %Q : %s }", "info", "Nothing to set");
        httpd_resp_sendstr(req, buf.data());

        return ESP_OK;
    }

    update_pwm pwm_update{ 
        .index = static_cast<size_t>(*index),
        .type = pwm_setting_indices::current_value, 
        .data = { .current_value = static_cast<uint16_t>(current_value) } 
    };

    // TODO: return changed object or something
    global_store.write_event(pwm_update);
    json_printf(&answer, "{ %Q : %Q }", "info", "OK");
    httpd_resp_sendstr(req, buf.data());

    return ESP_OK;
}