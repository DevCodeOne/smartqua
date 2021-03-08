#pragma once

#include "esp_http_server.h"

#include "actions/soft_timer_actions.h"

esp_err_t do_timers(httpd_req *req);