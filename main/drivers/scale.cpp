#include "scale.h"

loadcell::loadcell(uint8_t sck, uint8_t dout, int32_t offset) : m_offset(offset), m_loadcell_resource(sck, dout) {}

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
}