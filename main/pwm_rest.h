#pragma once

#include <array>

#include "esp_http_server.h"
#include "esp_log.h"
#include "frozen.h"

#include "settings.h"
#include "pwm.h"

esp_err_t get_pwm_outputs(httpd_req *req);
esp_err_t create_pwm_output(httpd_req *req);
esp_err_t set_pwm_output(httpd_req *req);