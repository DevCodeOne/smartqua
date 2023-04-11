#include "rmt_stepper_driver.h"

#include "driver/rmt_encoder.h"
#include "driver/rmt_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hal/rmt_types.h"

#include <algorithm>
#include <array>

// Points per rotation
static inline constexpr size_t POINTS_PER_ROTATION = 200;

struct UniformStepperMovementEncoder {
    rmt_encoder_t base;
    rmt_encoder_handle_t copyEncoder;
    uint32_t resolution;
};

static size_t encodeStepperMotorUniform(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primaryData, size_t dataSize, rmt_encode_state_t *state) {
    UniformStepperMovementEncoder *motorEncoder = __containerof(encoder, UniformStepperMovementEncoder, base);
    rmt_encoder_handle_t copyEncoder = motorEncoder->copyEncoder;
    rmt_encode_state_t sessionState = static_cast<rmt_encode_state_t>(0);
    uint32_t targetFreqHz = *reinterpret_cast<const uint32_t *>(primaryData);
    uint32_t symbolDuration = motorEncoder->resolution / targetFreqHz / 2;

    rmt_symbol_word_t freqSample = {
        .duration0 = symbolDuration,
        .level0 = 0,
        .duration1 = symbolDuration,
        .level1 = 1
    };

    size_t encodedSymbols = copyEncoder->encode(copyEncoder, channel, &freqSample,
        sizeof(freqSample), &sessionState);
    *state = sessionState;

    return encodedSymbols;
}

static esp_err_t deleteUniformMotorEncoder(rmt_encoder_t *encoder) {
    UniformStepperMovementEncoder *motorEncoder = __containerof(encoder, UniformStepperMovementEncoder, base);
    rmt_del_encoder(motorEncoder->copyEncoder);
    free(motorEncoder);

    return ESP_OK;
}

static esp_err_t resetUniformMotorEncoder(rmt_encoder_t *encoder) {
    UniformStepperMovementEncoder *motorEncoder = __containerof(encoder, UniformStepperMovementEncoder, base);
    rmt_encoder_reset(motorEncoder->copyEncoder);

    return ESP_OK;
}

esp_err_t createNewRmtUniformEncoder(const UniformStepperMovement &conf, rmt_encoder_handle_t *handle) {
    UniformStepperMovementEncoder *encoder = nullptr;
    encoder = reinterpret_cast<UniformStepperMovementEncoder *>(calloc(1, sizeof(UniformStepperMovementEncoder)));

    if (encoder == nullptr) {
        return ESP_FAIL;
    }

    auto failCleanUp = [&encoder]() {
        if (!encoder) {
            return ESP_FAIL;
        }

        if (encoder->copyEncoder) {
            rmt_del_encoder(encoder->copyEncoder);
        }
        free(encoder);

        return ESP_FAIL;
    };

    rmt_copy_encoder_config_t copyEncoderConf = {};

    auto copyEncoderResult = rmt_new_copy_encoder(&copyEncoderConf, &encoder->copyEncoder);

    if (copyEncoderResult != ESP_OK) {
        return failCleanUp();
    }

    encoder->resolution = conf.resolution;
    encoder->base.del = deleteUniformMotorEncoder;
    encoder->base.encode = encodeStepperMotorUniform;
    encoder->base.reset = resetUniformMotorEncoder;
    *handle = &(encoder->base);

    return ESP_OK;
}
