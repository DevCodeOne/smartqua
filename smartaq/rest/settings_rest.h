#pragma once

#include <esp_err.h>
#include <esp_http_server.h>

#include "utils/esp/web_utils.h"

esp_err_t do_settings(httpd_req *req);
