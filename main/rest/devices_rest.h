#pragma once

#include "esp_http_server.h"

esp_err_t get_devices(httpd_req *req);
esp_err_t post_device(httpd_req *req);
esp_err_t set_device(httpd_req *req);
esp_err_t remove_device(httpd_req *req);