#pragma once

#include <cstdint>

#include "driver/rmt_tx.h"
#include "driver/rmt_types.h"

struct UniformStepperMovement {
    uint32_t resolution; // hz
};

esp_err_t createNewRmtUniformEncoder(const UniformStepperMovement &conf, rmt_encoder_handle_t *handle);