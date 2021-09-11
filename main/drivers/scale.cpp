#include "scale.h"

#include <optional>

loadcell::loadcell(const device_config *conf) : m_conf(conf), m_loadcell_resource(reinterpret_cast<loadcell_config *>(&m_conf->device_config)) {}

// TODO: implement
device_operation_result loadcell::write_value(const device_values &value) {
    return device_operation_result::failure;
}

device_operation_result loadcell::read_value(device_values &out) const {
    return device_operation_result::failure;
}

device_operation_result loadcell::write_device_options(const char *json_input, size_t input_len) {
    return device_operation_result::failure;
}

device_operation_result loadcell::get_info(char *output, size_t output_buffer_len) const {
    return device_operation_result::failure;
}

device_operation_result loadcell::update_runtime_data() {
    return device_operation_result::ok;
}

std::optional<loadcell> loadcell::create_driver(const std::string_view input, device_config &device_conf_out) {
    return std::nullopt;
}

std::optional<loadcell> loadcell::create_driver(const device_config *config) {
    return std::nullopt;
}

/*
loadcell_status loadcell::init_scale() {
    std::unique_lock instance_lock{m_resource_lock};

    return loadcell_status::success;
}

loadcell_status loadcell::read_raw(int32_t *value) {
    std::shared_lock instance_lock{m_resource_lock};

    m_loadcell_resource.read_value(value);
    *value -= m_offset;

    return loadcell_status::success;
}

loadcell_status loadcell::tare() {
    int32_t value = 0;
    loadcell_status result = loadcell_status::failure;
    {
        std::shared_lock instance_lock{m_resource_lock};

        // TODO: maybe use even more samples ?
        result = read_raw(&value);

        if (result != loadcell_status::success) {
            return result;
        }
    }

    // Yes race condition ahead, but no lock up, for now that's good enough
    int32_t old_offset = get_offset();
    set_offset(old_offset + value);

    return result;
}

loadcell_status loadcell::set_offset(int32_t value) {
    std::unique_lock instance_lock{m_resource_lock};
    m_offset = value;

    return loadcell_status::success;
}

loadcell_status loadcell::set_scale(float scale) {
    std::unique_lock instance_lock{m_resource_lock};
    if (std::fabs(scale) < 0.05f) {
        return loadcell_status::failure;
    }

    m_scale = scale;
    return loadcell_status::success;
}

loadcell_status loadcell::read_value(float *value) {
    std::shared_lock instance_lock{m_resource_lock};

    int32_t raw_value = 0;
    auto result = read_raw(&raw_value);

    if (result != loadcell_status::success) {
        return result;
    }

    *value = static_cast<float>(raw_value) / m_scale;

    return loadcell_status::success;
}*/