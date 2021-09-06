#pragma once

#include <array>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <string_view>
#include <optional>

#include "utils/utils.h"
#include "storage/store.h"

namespace SmartAq::Utils {

    template<typename T>
    static inline constexpr bool IsValidBaseType = true;

    template<typename BaseType, size_t Size>
        struct TrivialRepresentation {
            static inline const constexpr char *const name = BaseType::StorageName;

            std::array<BaseType, Size> values;
            std::array<bool, Size> initialized;
            std::array<std::array<char, name_length>, Size> names;
    };

    namespace ArrayActions {
        json_action_result GetOverviewAction(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
        json_action_result GetValueAction(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
        json_action_result AddValueAction(std::optional<unsigned int> index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
        json_action_result RemoveValueAction(unsigned int index, const char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);
        json_action_result SetValueAction(unsigned int index, char *input, size_t input_len, char *output_buffer, size_t output_buffer_len);

        // Initialise and Update Value
        // index is optional, the next free position in the array will be used
        // if the index is specified, the settingName can be used to update the name
        // if the index is not specified, the settingName can be used to access the specified value
        // settingNames have to unique
        template<typename BaseType, size_t UID>
        struct SetValue {
            std::optional<unsigned int> index;
            std::optional<std::string_view> settingName;
            std::string_view jsonSettingValue;

            struct {
                collection_operation_result collection_result = collection_operation_result::failed;
                std::optional<unsigned int> index;
            } result;
        };

        template<typename BaseType, size_t UID>
        struct RemoveValue {
            std::optional<unsigned int> index;
            std::optional<std::string_view> settingName;

            struct {
                collection_operation_result collection_result = collection_operation_result::failed;
            } result;
        };

        template<typename BaseType, size_t UID>
        struct GetValue {
            std::optional<unsigned int> index;
            std::optional<std::string_view> settingName;

            struct {
                collection_operation_result collection_result = collection_operation_result::failed;
                std::optional<BaseType> value;
            } result;
        };

        // index to start the overview from
        template<typename BaseType, size_t UID>
        struct GetValueOverview {
            std::optional<unsigned int> index;
            char *outputDst = nullptr;
            size_t outputLen = 0;

            struct {
                collection_operation_result collection_result = collection_operation_result::failed;
            } Result;
        };
    }

    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID = 0>
    struct EventAccessArray final {
        static_assert(IsValidBaseType<BaseType>, "BaseType is not valid check IsValidBaseType for details");

        using ElementType = BaseType;
        using TrivialRepresentationType = TrivialRepresentation<BaseType, Size>;
        template<typename T>
        using FilterReturnType = std::conditional_t<!
        all_unique_v<T,
            ArrayActions::SetValue<ElementType, UID>,
            ArrayActions::RemoveValue<ElementType, UID>,
            ArrayActions::GetValue<ElementType, UID>,
            ArrayActions::GetValueOverview<ElementType, UID>>, 
            TrivialRepresentationType, ignored_event>;

        static inline constexpr size_t NumElements = Size;
        static inline constexpr size_t UniqueIdentifier = UID;

        EventAccessArray() = default;
        ~EventAccessArray() = default;

        EventAccessArray &operator=(const TrivialRepresentationType &newValue);

        template<typename T>
        FilterReturnType<T> dispatch(T &) {};

        FilterReturnType<ArrayActions::SetValue<ElementType, UID>> dispatch(ArrayActions::SetValue<ElementType, UID> &);
        template<typename UpdateHook>
        FilterReturnType<ArrayActions::SetValue<ElementType, UID>> dispatch(ArrayActions::SetValue<ElementType, UID> &, const UpdateHook &hook);
        FilterReturnType<ArrayActions::RemoveValue<ElementType, UID>> dispatch(ArrayActions::RemoveValue<ElementType, UID> &);
        FilterReturnType<ArrayActions::GetValue<ElementType, UID>> dispatch(ArrayActions::GetValue<ElementType, UID> &) const;
        FilterReturnType<ArrayActions::GetValueOverview<ElementType, UID>> dispatch(ArrayActions::GetValueOverview<ElementType, UID> &) const;

        template<typename T>
        void dispatch(T &) const {};

        const TrivialRepresentationType &trivial_representation() const;

        private:
            std::optional<unsigned int> findIndex(std::optional<unsigned int> index, std::optional<std::string_view> name, bool findFreeSlotOtherwise = false) const;

            TrivialRepresentationType data;
            std::array<std::optional<RuntimeType>, NumElements> runtimeData;
    };

    // TODO: add return value to indicate if it is a newly created value
    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    std::optional<unsigned int> EventAccessArray<BaseType, RuntimeType, Size, UID>::findIndex(std::optional<unsigned int> index, std::optional<std::string_view> name, bool findFreeSlotOtherwise) const {
        if (!index.has_value() && name.has_value()) {
            return std::nullopt;
        } else if (index.has_value() && *index >= NumElements) {
            return std::nullopt;
        }

        std::optional<unsigned int> foundIndex = index;

        if (!foundIndex.has_value() && name.has_value()) {
            for (int i = 0; i < NumElements; ++i) {
                if (*name == data.names[i].data()) {
                    foundIndex = i;
                    break;
                }
            }
        }

        if (!foundIndex.has_value() && findFreeSlotOtherwise) {
            for (int i = 0; i < NumElements; ++i) {
                if (!data.initialized[i]) {
                    foundIndex = i;
                    break;
                }
            }
        }
        
        return foundIndex;
    }

    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::operator=(const TrivialRepresentationType &newData) -> EventAccessArray & {
        data = newData;

        for (unsigned int i = 0; i < data.initialized.size(); ++i) {
            if (!data.initialized[i]) {
                continue;
            }

            runtimeData[i] = RuntimeType::create(&data.values[i]);

            if (!runtimeData[i]) {
                // If it fails, we could reset data.initialized ?
            }
        }

        return *this;
    }

    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::dispatch(ArrayActions::SetValue<BaseType, UID> &event) -> FilterReturnType<ArrayActions::SetValue<BaseType, UID>> {
        auto doNothing = [](auto &, auto &) -> bool { return true; };
        return dispatch<decltype(doNothing)>(event, doNothing);
    }

    // TODO: use hook
    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    template<typename UpdateHook>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::dispatch(ArrayActions::SetValue<BaseType, UID> &event, const UpdateHook &update) -> FilterReturnType<ArrayActions::SetValue<BaseType, UID>> {
        std::optional<unsigned int> foundIndex = findIndex(event.index, event.settingName, true);

        // Set already initialised value
        if (foundIndex.has_value() && data.initialized[*foundIndex]) {
            event.index = *foundIndex;
            // bool successfullySet = runtimeData[*foundIndex].set(event.jsonSettingValue);
            auto successfullySet = update(runtimeData[*foundIndex], data.values[*foundIndex]);
            if (successfullySet && event.settingName.has_value()) {
                std::strncpy(data.names[*foundIndex].data(), event.settingName->data(), data.names[*foundIndex].size());
            }
            if (!successfullySet) {
                event.result.collection_result = collection_operation_result::failed;
                return data;
            }
        } else if (foundIndex.has_value() && !data.initialized[*foundIndex] && event.settingName.has_value()) {
            // runtimeData[*foundIndex] = RuntimeType::create(event.jsonSettingValue, data.values[*foundIndex]);
            if (auto successfullyInitialized = update(runtimeData[*foundIndex], data.values[*foundIndex]); successfullyInitialized) {
                data.initialized[*foundIndex] = true;
                std::strncpy(data.names[*foundIndex].data(), event.settingName->data(), data.names[*foundIndex].size());
            } else {
                event.result.collection_result = collection_operation_result::failed;
                return data;
            }

            event.index = *foundIndex;
           
        } else if (!foundIndex.has_value() && !event.index.has_value()) {
            event.result.collection_result = collection_operation_result::collection_full;
            return data;
        }

        return data;

    }


    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::dispatch(ArrayActions::RemoveValue<BaseType, UID> &event) -> FilterReturnType<ArrayActions::RemoveValue<BaseType, UID>> {
        std::optional<unsigned int> indexToDelete = findIndex(event.index, event.settingName);

        if (!indexToDelete.has_value()) {
            event.result.collection_result = collection_operation_result::index_invalid;
            return data;
        }

        event.index = indexToDelete;
        data.values[*indexToDelete] = BaseType{};
        data.names[*indexToDelete][0] = '\0';
        data.initialized[*indexToDelete] = false;
        runtimeData[*indexToDelete] = std::nullopt;

        return data;
    }

    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::dispatch(ArrayActions::GetValue<BaseType, UID> &event) const -> FilterReturnType<ArrayActions::GetValue<BaseType, UID>> {
        auto foundIndex = findIndex(event.index, event.settingName);

        if (!foundIndex.has_value()) {
            event.result.collection_result = collection_operation_result::index_invalid;
            return data;
        }

        event.result.value = data.data[*foundIndex];

        return data;
    }

    // TODO: implement this
    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::dispatch(ArrayActions::GetValueOverview<BaseType, UID> &event) const -> FilterReturnType<ArrayActions::GetValueOverview<BaseType, UID>> {
    }
}