#pragma once

#include "esp_http_server.h"

#include "actions/soft_timer_actions.h"

esp_err_t get_timers_rest(httpd_req *req);
esp_err_t post_timer_rest(httpd_req *req);
esp_err_t remove_timer_rest(httpd_req *req);
esp_err_t set_timer_rest(httpd_req *req);