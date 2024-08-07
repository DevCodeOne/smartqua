#pragma once

#include <array>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <string_view>
#include <mutex>
#include <optional>

#include "frozen.h"

#include "actions/action_types.h"
#include "build_config.h"
#include "utils/utils.h"
#include "utils/stack_string.h"
#include "storage/store.h"

namespace SmartAq::Utils {

    template<typename T>
    static inline constexpr bool IsValidBaseType = true;

    // TODO: seperate name_length
    template<typename BaseType, size_t Size>
        struct TrivialRepresentation {
            static inline const constexpr char *const name = BaseType::StorageName;

            std::array<BaseType, Size> values;
            std::array<bool, Size> initialized;
            std::array<stack_string<name_length>, Size> names;
    };

    namespace ArrayActions {
        // Initialise and Update Value
        // index is optional, the next free position in the array will be used
        // if the index is specified, the settingName can be used to update the name
        // if the index is not specified, the settingName can be used to access the specified value
        // settingNames have to unique
        template<typename BaseType, size_t UID>
        struct SetValue {
            std::optional<unsigned int> index = std::nullopt;
            std::optional<std::string_view> settingName = "";
            std::string_view jsonSettingValue = "";

            struct {
                collection_operation_result collection_result = collection_operation_result::failed;
                std::optional<unsigned int> index = std::nullopt;
            } result;
        };

        template<typename BaseType, size_t UID>
        struct RemoveValue {
            std::optional<unsigned int> index = std::nullopt;
            std::optional<std::string_view> settingName = std::nullopt;

            struct {
                collection_operation_result collection_result = collection_operation_result::failed;
            } result;
        };

        template<typename BaseType, size_t UID>
        struct GetValue {
            std::optional<unsigned int> index = std::nullopt;
            std::optional<std::string_view> settingName = std::nullopt;

            struct {
                collection_operation_result collection_result = collection_operation_result::failed;
                std::optional<BaseType> value = std::nullopt;
            } result;
        };

        // index to start the overview from
        template<typename BaseType, size_t UID>
        struct GetValueOverview {
            std::optional<unsigned int> index;
            char *output_dst = nullptr;
            size_t output_len = 0;

            struct {
                collection_operation_result collection_result = collection_operation_result::failed;
            } result;
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
            const TrivialRepresentationType &, IgnoredEvent>;

        static inline constexpr size_t NumElements = Size;
        static inline constexpr size_t UniqueIdentifier = UID;

        EventAccessArray() = default;
        ~EventAccessArray() = default;

        template<typename CreationHook>
        EventAccessArray &initialize(const TrivialRepresentationType &newValue, const CreationHook &hook);

        template<typename T>
        FilterReturnType<T> dispatch(T &) {};

        FilterReturnType<ArrayActions::SetValue<ElementType, UID>> dispatch(ArrayActions::SetValue<ElementType, UID> &);
        template<typename UpdateHook>
        FilterReturnType<ArrayActions::SetValue<ElementType, UID>> dispatch(ArrayActions::SetValue<ElementType, UID> &, const UpdateHook &hook);
        FilterReturnType<ArrayActions::RemoveValue<ElementType, UID>> dispatch(ArrayActions::RemoveValue<ElementType, UID> &);
        FilterReturnType<ArrayActions::GetValue<ElementType, UID>> dispatch(ArrayActions::GetValue<ElementType, UID> &) const;
        FilterReturnType<ArrayActions::GetValueOverview<ElementType, UID>> dispatch(ArrayActions::GetValueOverview<ElementType, UID> &) const;
        template<typename PrintHook>
        FilterReturnType<ArrayActions::GetValueOverview<ElementType, UID>> dispatch(ArrayActions::GetValueOverview<ElementType, UID> &, PrintHook hook) const;

        template<typename T>
        void dispatch(T &) const {};

        const TrivialRepresentationType &getTrivialRepresentation() const;

        template<typename Callable>
        void invokeOnRuntimeData(int index, Callable callable);
        template<typename Callable>
        void invokeOnRuntimeData(int index, Callable callable) const;

        template<typename Callable>
        void invokeOnAllRuntimeData(Callable callable);

        bool hasValidRuntimeData(int index) const;

        private:
            std::optional<unsigned int> findIndex(std::optional<unsigned int> index, std::optional<std::string_view> name, bool findFreeSlotOtherwise = false) const;

            TrivialRepresentationType data;
            std::array<std::optional<RuntimeType>, NumElements> runtimeData;

            mutable std::recursive_mutex instanceMutex;
    };

    // TODO: add return value to indicate if it is a newly created value
    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    std::optional<unsigned int> EventAccessArray<BaseType, RuntimeType, Size, UID>::findIndex(std::optional<unsigned int> index, std::optional<std::string_view> name, bool findFreeSlotOtherwise) const {
        if (!index.has_value() && !name.has_value() && !findFreeSlotOtherwise) {
            return std::nullopt;
        } else if (index.has_value() && *index >= NumElements) {
            return std::nullopt;
        }

        std::optional<unsigned int> foundIndex = index;

        if (!foundIndex.has_value() && name.has_value()) {
            for (unsigned int i = 0; i < NumElements; ++i) {
                if (*name == data.names[i].data()) {
                    foundIndex = i;
                    break;
                }
            }
        }

        if (!foundIndex.has_value() && findFreeSlotOtherwise) {
            for (unsigned int i = 0; i < NumElements; ++i) {
                if (!data.initialized[i]) {
                    foundIndex = i;
                    break;
                }
            }
        }
        
        return foundIndex;
    }

    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    template<typename CreationHook>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::initialize(const TrivialRepresentationType &newValue, const CreationHook &createRuntime) -> EventAccessArray & {
        std::unique_lock instanceGuard{instanceMutex};
        data = newValue;

        for (unsigned int i = 0; i < data.initialized.size(); ++i) {
            if (!data.initialized[i]) {
                continue;
            }

            if (!createRuntime(&data.values[i], runtimeData[i])) {
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

    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    template<typename UpdateHook>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::dispatch(ArrayActions::SetValue<BaseType, UID> &event, const UpdateHook &update) -> FilterReturnType<ArrayActions::SetValue<BaseType, UID>> {
        std::unique_lock instanceGuard{instanceMutex};

        std::optional<unsigned int> foundIndex = findIndex(event.index, event.settingName, true);

        if (!foundIndex.has_value()) {
            event.result.collection_result = collection_operation_result::collection_full;
            return data;
        }

        auto successfullySet = update(runtimeData[*foundIndex], data.values[*foundIndex], event.jsonSettingValue);
        if (successfullySet) {
            data.initialized[*foundIndex] = true;
            if (event.settingName) {
                data.names[*foundIndex] = *event.settingName;
            }
            event.result.collection_result = collection_operation_result::ok;
            event.result.index = foundIndex;
        } else {
            event.result.collection_result = collection_operation_result::failed;
        }

        return data;

    }


    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::dispatch(ArrayActions::RemoveValue<BaseType, UID> &event) -> FilterReturnType<ArrayActions::RemoveValue<BaseType, UID>> {
        std::unique_lock instanceGuard{instanceMutex};

        std::optional<unsigned int> indexToDelete = findIndex(event.index, event.settingName);

        if (!indexToDelete.has_value()) {
            event.result.collection_result = collection_operation_result::index_invalid;
            return data;
        }

        event.index = indexToDelete;
        event.result.collection_result = collection_operation_result::ok;
        // First delete class, so the class can use the information in the trivial representation if needed
        runtimeData[*indexToDelete] = std::nullopt;
        data.values[*indexToDelete] = BaseType{};
        data.names[*indexToDelete][0] = '\0';
        data.initialized[*indexToDelete] = false;

        return data;
    }

    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::dispatch(ArrayActions::GetValue<BaseType, UID> &event) const -> FilterReturnType<ArrayActions::GetValue<BaseType, UID>> {
        Logger::log(LogLevel::Info, "Try locking to read");
        std::unique_lock instanceGuard{instanceMutex};

        auto foundIndex = findIndex(event.index, event.settingName);

        if (!foundIndex.has_value()) {
            event.result.collection_result = collection_operation_result::index_invalid;
            return data;
        }

        event.result.value = data.values[*foundIndex];
        event.result.collection_result = collection_operation_result::ok;

        return data;
    }

    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::dispatch(ArrayActions::GetValueOverview<BaseType, UID> &event) const -> FilterReturnType<ArrayActions::GetValueOverview<BaseType, UID>> {
        auto printLambda = [](auto &out, const auto &name, const auto &, int index, bool) -> int {
            const bool firstPrint = index > 0;
            const char *format = ", { index : %u, description : %M }";
            return json_printf(&out, format + (firstPrint ? 1 : 0), index, json_printf_single<std::decay_t<decltype(name)>>, &name);
        };

        return dispatch<decltype(printLambda)>(event, printLambda);
    }

    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    template<typename PrintHook>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::dispatch(ArrayActions::GetValueOverview<BaseType, UID> &event, PrintHook printHook) const -> FilterReturnType<ArrayActions::GetValueOverview<BaseType, UID>> {
        std::unique_lock  instanceGuard{instanceMutex};

        unsigned int start_index = 0;
        if (event.index.has_value()) {
            start_index = *event.index;
        }

        if (start_index > Size) {
            event.result.collection_result = collection_operation_result::index_invalid; 
        }

        json_out out = JSON_OUT_BUF(event.output_dst, event.output_len);

        json_printf(&out, "[");
        int written = 0;
        for (unsigned int index = start_index; index < Size; ++index) {
            if (data.initialized[index]) {
                written += printHook(out, data.names[index], data.values[index], index, written == 0);
            }
        }
        json_printf(&out, " ]");
        event.result.collection_result = collection_operation_result::ok;

        return data;
    }
    
    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    bool EventAccessArray<BaseType, RuntimeType, Size, UID>::hasValidRuntimeData(int index) const {
        if (index >= NumElements) {
            return false;
        }

        return data.initialized[index] && runtimeData[index].has_value();
    }

    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::getTrivialRepresentation() const -> const TrivialRepresentationType & {
        std::unique_lock instanceGuard{instanceMutex};

        return data;
    }

    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    template<typename Callable>
    void EventAccessArray<BaseType, RuntimeType, Size, UID>::invokeOnRuntimeData(int index, Callable callable) const {
        std::unique_lock instanceGuard{instanceMutex};

        if (hasValidRuntimeData(index)) {
            callable(*runtimeData[index]);
        }
    }

    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    template<typename Callable>
    void EventAccessArray<BaseType, RuntimeType, Size, UID>::invokeOnRuntimeData(int index, Callable callable) {
        std::unique_lock instanceGuard{instanceMutex};

        if (hasValidRuntimeData(index)) {
            callable(*runtimeData[index]);
        }
    }

    template<typename BaseType, typename RuntimeType, size_t Size, size_t UID>
    template<typename Callable>
    void EventAccessArray<BaseType, RuntimeType, Size, UID>::invokeOnAllRuntimeData(Callable callable) {
        std::unique_lock instanceGuard{instanceMutex};

        for (int i = 0; i < NumElements; ++i) {
            if (hasValidRuntimeData(i)) {
                callable(*runtimeData[i]);
            }
        }
    }
}