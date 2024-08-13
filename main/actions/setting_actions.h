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

using SettingCollectionOperation = CollectionOperationResult;

struct RuntimeSingleSetting { };

struct SetSetting {
    unsigned int index;
    std::string_view setting_name;
    std::string_view json_setting_value;

    struct {
        SettingCollectionOperation collection_result = SettingCollectionOperation::failed;
        single_setting_result op_result = single_setting_result::failure; 
    } result;
};

struct RemoveSetting {
    std::optional<unsigned int> index;
    std::optional<std::string_view> setting_name;

    struct {
        SettingCollectionOperation collection_result = SettingCollectionOperation::failed;
    } result;
};

struct RetrieveSetting {
    unsigned int index;
    std::string_view setting_name;

    struct {
        SettingCollectionOperation collection_result = SettingCollectionOperation::failed;
    } result;
};

struct RetrieveAllSettingNames {
    unsigned int offset = 0;
    char *output_dst = nullptr;
    size_t output_len = 0;

    struct {
        SettingCollectionOperation collection_result = SettingCollectionOperation::failed;
    } result;
};

using SettingChangedNotifier = void (*)(const SingleSetting *setting);

struct AddSettingNotifier {
    const char *setting_name = nullptr;
    SettingChangedNotifier notifier = nullptr;

    struct {
        SettingCollectionOperation collection_result = SettingCollectionOperation::failed;
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
                    SetSetting,
                    RemoveSetting,
                    RetrieveSetting,
                    RetrieveAllSettingNames,
                    AddSettingNotifier>, TrivialRepresentationType, IgnoredEvent>;

        Settings &operator=(const TrivialRepresentationType &new_value);

        template<typename T>
        filter_return_type<T> dispatch(T &event) const {}
    private:
        TrivialRepresentationType data;
        std::array<std::pair<const char **, SettingChangedNotifier>, N> notifier;
};
