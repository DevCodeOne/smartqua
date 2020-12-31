#pragma once

#include "nvs.h"

enum struct nvs_op_status {
    success = 0, failure
};

template <typename T>
class nvs_helper {
    public:
        nvs_helper(const char *id);
        ~nvs_helper();

        nvs_op_status read_from_storage(T *dst);
        nvs_op_status write_to_storage(T *dst);
    private:
        nvs_handle_t m_handle;
};
