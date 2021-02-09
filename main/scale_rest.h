#pragma once

#include "esp_http_server.h"
#include "esp_log.h"
#include "frozen.h"

#include "scale.h"
#include "settings.h"

esp_err_t get_load(httpd_req_t *req);
esp_err_t set_contained_co2(httpd_req *req);
esp_err_t tare_scale(httpd_req_t *req);