#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "build_config.h"
#include "actions/action_types.h"
#include "drivers/setting_types.h"
#include "drivers/setting_driver.h"
#include "utils/utils.h"
#include "utils/event_access_array.h"
#include "storage/store.h"

using setting_collection_operation = collection_operation_result;

struct RuntimeSingleSetting { };

struct set_setting {
    unsigned int index;
    std::string_view setting_name;
    std::string_view json_setting_value;

    struct {
        setting_collection_operation collection_result = setting_collection_operation::failed;
        single_setting_result op_result = single_setting_result::failure; 
    } result;
};

struct remove_setting {
    std::optional<unsigned int> index;
    std::optional<std::string_view> setting_name;

    struct {
        setting_collection_operation collection_result = setting_collection_operation::failed;
    } result;
};

struct retrieve_setting {
    unsigned int index;
    std::string_view setting_name;

    struct {
        setting_collection_operation collection_result = setting_collection_operation::failed;
    } result;
};

struct retrieve_all_setting_names {
    unsigned int offset = 0;
    char *output_dst = nullptr;
    size_t output_len = 0;

    struct {
        setting_collection_operation collection_result = setting_collection_operation::failed;
    } result;
};

using setting_changed_notifier = void (*)(const SingleSetting *setting);

struct add_setting_notifier {
    const char *setting_name = nullptr;
    setting_changed_notifier notifier = nullptr;

    struct {
        setting_collection_operation collection_result = setting_collection_operation::failed;
    } result;
};

template<size_t N>
class Settings {
    public:
        static inline constexpr size_t NumSettings = N;
        using TrivialRepresentationType = SmartAq::Utils::EventAccessArray<SingleSetting, RuntimeSingleSetting, NumSettings, 19>;

        Settings() = default;
        ~Settings() = default;

        template<typename T>
        using filter_return_type = std::conditional_t<!
        all_unique_v<T,
                    set_setting,
                    remove_setting,
                    retrieve_setting,
                    retrieve_all_setting_names,
                    add_setting_notifier>, TrivialRepresentationType, ignored_event>;

        Settings &operator=(const TrivialRepresentationType &new_value);

        template<typename T>
        filter_return_type<T> dispatch(T &event) const {}
    private:
        TrivialRepresentationType data;
        std::array<std::pair<const char **, setting_changed_notifier>, N> notifier;
};
