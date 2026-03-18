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
#include "utils/variant_utils.h"
#include "storage/serialized_representation.h"
#include "storage/store.h"

namespace SmartAq::Utils {

    using IndexOrName = std::variant<unsigned int, std::string_view>;
    using IndexOrNameOrEmpty = std::variant<std::monostate, unsigned int, std::string_view>;

    namespace ArrayActions {
        // Initialize and Update Value
        // index is optional, the next free position in the array will be used
        // if the index is specified, the settingName can be used to update the name
        // if the index is not specified, the settingName can be used to access the specified value
        // settingNames have to unique
        template<typename BaseType, size_t UID>
        struct SetValue {
            IndexOrNameOrEmpty index;
            std::optional<std::string_view> settingName = "";
            std::string_view jsonSettingValue = "";

            struct {
                CollectionOperationResult collection_result = CollectionOperationResult::failed;
                std::optional<unsigned int> index = std::nullopt;
            } result;
        };

        template<typename BaseType, size_t UID>
        struct RemoveValue {
            IndexOrName index;

            struct {
                CollectionOperationResult collection_result = CollectionOperationResult::failed;
            } result;
        };

        template<typename BaseType, size_t UID>
        struct GetValue {
            std::optional<IndexOrName> index = std::nullopt;
            std::optional<std::string_view> settingName = std::nullopt;

            struct {
                CollectionOperationResult collection_result = CollectionOperationResult::failed;
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
                CollectionOperationResult collection_result = CollectionOperationResult::failed;
            } result;
        };
    }

    template<ValidBaseType BaseType, typename RuntimeType, size_t Size, size_t UID = 0>
    struct EventAccessArray final {
        using ElementType = BaseType;
        using TrivialRepresentationType = SerializedRepresentationCollection<BaseType, Size>;

        static constexpr size_t NumElements = Size;
        static constexpr size_t UniqueIdentifier = UID;

        EventAccessArray() = default;
        ~EventAccessArray() = default;

        template<typename CreationHook>
        EventAccessArray &initialize(const TrivialRepresentationType &newValue, const CreationHook &createRuntime);

        const TrivialRepresentationType &dispatch(ArrayActions::SetValue<ElementType, UID> &);
        template<typename UpdateHook>
        const TrivialRepresentationType &dispatch(ArrayActions::SetValue<ElementType, UID> &, const UpdateHook &update);
        const TrivialRepresentationType &dispatch(ArrayActions::RemoveValue<ElementType, UID> &);
        const TrivialRepresentationType &dispatch(ArrayActions::GetValue<ElementType, UID> &) const;
        const TrivialRepresentationType &dispatch(ArrayActions::GetValueOverview<ElementType, UID> &) const;
        template<typename PrintHook>
        const TrivialRepresentationType &dispatch(ArrayActions::GetValueOverview<ElementType, UID> &, PrintHook hook) const;

        const TrivialRepresentationType &getTrivialRepresentation() const;

        template<typename Callable>
        void invokeOnRuntimeData(IndexOrName index, Callable callable);
        template<typename Callable>
        void invokeOnRuntimeData(IndexOrName index, Callable callable) const;

        template<typename Callable>
        void invokeOnAllRuntimeData(Callable callable);

        bool hasValidRuntimeData(int index) const;

        private:
        template <typename VariantType>
        std::optional<unsigned int> findIndex(const VariantType& indexOrName, bool findFreeSlotOtherwise = false) const;

        TrivialRepresentationType data;
        std::array<std::optional<RuntimeType>, NumElements> runtimeData;

        mutable std::recursive_mutex instanceMutex;
    };

    // TODO: add return value to indicate if it is a newly created value
    template<ValidBaseType BaseType, typename RuntimeType, size_t Size, size_t UID>
    template<typename VariantType>
    std::optional<unsigned int> EventAccessArray<BaseType, RuntimeType, Size, UID>::findIndex(const VariantType &indexOrName, bool findFreeSlotOtherwise) const {
        const auto index = getOpt<unsigned int>(indexOrName);

        if (index.has_value() && *index >= NumElements) {
            return std::nullopt;
        }

        std::optional<unsigned int> foundIndex = index;
        std::optional<unsigned int> foundName;

        if (const auto name = getOpt<std::string_view>(indexOrName); name)
        {
            for (unsigned int i = 0; i < NumElements; ++i)
            {
                if (*name == data.values[i].name)
                {
                    foundName = i;
                    break;
                }
            }
        }

        const bool nameIndexConflict = foundIndex.has_value() && foundName.has_value()
            && foundIndex.value() != foundName.value();

        if (nameIndexConflict)
        {
            return {};
        }

        if (foundIndex.has_value())
        {
            return { *foundIndex };
        }

        if (findFreeSlotOtherwise) {
            for (unsigned int i = 0; i < NumElements; ++i) {
                if (!data.values[i].initialized) {
                    return i;
                }
            }
        }

        return foundIndex;
    }

    template<ValidBaseType BaseType, typename RuntimeType, size_t Size, size_t UID>
    template<typename CreationHook>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::initialize(const TrivialRepresentationType &newValue, const CreationHook &createRuntime) -> EventAccessArray & {
        std::unique_lock instanceGuard{instanceMutex};
        data = newValue;

        for (unsigned int i = 0; i < data.values.size(); ++i) {
            if (!data.values[i].initialized) {
                continue;
            }

            if (!createRuntime(&data.values[i].value, runtimeData[i])) {
                // If it fails, we could reset data.initialized ?
            }
        }

        return *this;

    }

    template<ValidBaseType BaseType, typename RuntimeType, size_t Size, size_t UID>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::dispatch(ArrayActions::SetValue<BaseType, UID> &event) -> const TrivialRepresentationType & {
        auto doNothing = [](auto &, auto &) -> bool { return true; };
        return dispatch(event, doNothing);
    }

    template<ValidBaseType BaseType, typename RuntimeType, size_t Size, size_t UID>
    template<typename UpdateHook>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::dispatch(ArrayActions::SetValue<BaseType, UID> &event, const UpdateHook &update) -> const TrivialRepresentationType & {
        std::unique_lock instanceGuard{instanceMutex};

        std::optional<unsigned int> foundIndex = findIndex(event.index, true);

        if (!foundIndex.has_value()) {
            event.result.collection_result = CollectionOperationResult::collection_full;
            return data;
        }

        auto &currentValue = data.values[*foundIndex];
        auto successfullySet = update(runtimeData[*foundIndex], currentValue.value, event.jsonSettingValue);
        if (successfullySet) {
            currentValue.initialized = true;
            if (event.settingName) {
                currentValue.name = *event.settingName;
            }
            event.result.collection_result = CollectionOperationResult::ok;
            event.result.index = foundIndex;
        } else {
            event.result.collection_result = CollectionOperationResult::failed;
        }

        return data;

    }


    template<ValidBaseType BaseType, typename RuntimeType, size_t Size, size_t UID>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::dispatch(ArrayActions::RemoveValue<BaseType, UID> &event) -> const TrivialRepresentationType & {
        std::unique_lock instanceGuard{instanceMutex};

        std::optional<unsigned int> indexToDelete = findIndex(event.index);

        if (!indexToDelete.has_value()) {
            event.result.collection_result = CollectionOperationResult::index_invalid;
            return data;
        }

        event.index = *indexToDelete;
        event.result.collection_result = CollectionOperationResult::ok;
        // First delete class, so the class can use the information in the trivial representation if needed
        runtimeData[*indexToDelete] = std::nullopt;
        data.values[*indexToDelete] = typename TrivialRepresentationType::ValueType{ BaseType{}, false, ""};
        return data;
    }

    template<ValidBaseType BaseType, typename RuntimeType, size_t Size, size_t UID>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::dispatch(ArrayActions::GetValue<BaseType, UID> &event) const -> const TrivialRepresentationType &  {
        Logger::log(LogLevel::Info, "Try locking to read");
        std::unique_lock instanceGuard{instanceMutex};

        auto foundIndex = findIndex(event.index);
        if (!foundIndex.has_value()) {
            event.result.collection_result = CollectionOperationResult::index_invalid;
            return data;
        }

        event.result.value = data.values[*foundIndex];
        event.result.collection_result = CollectionOperationResult::ok;

        return data;
    }

    template<ValidBaseType BaseType, typename RuntimeType, size_t Size, size_t UID>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::dispatch(ArrayActions::GetValueOverview<BaseType, UID> &event) const -> const TrivialRepresentationType & {
        auto printLambda = []<typename NameType>(auto &out, const NameType &name, const auto &, int index, bool) -> int {
            const bool firstPrint = index > 0;
            auto format = ", { index : %u, description : %M }";
            return json_printf(&out, format + (firstPrint ? 1 : 0), index, json_printf_single<NameType>, &name);
        };

        return dispatch(event, printLambda);
    }

    template<ValidBaseType BaseType, typename RuntimeType, size_t Size, size_t UID>
    template<typename PrintHook>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::dispatch(ArrayActions::GetValueOverview<BaseType, UID> &event, PrintHook printHook) const -> const TrivialRepresentationType & {
        std::unique_lock  instanceGuard{instanceMutex};

        unsigned int start_index = 0;
        if (event.index.has_value()) {
            start_index = *event.index;
        }

        if (start_index > Size) {
            event.result.collection_result = CollectionOperationResult::index_invalid;
        }

        json_out out = JSON_OUT_BUF(event.output_dst, event.output_len);

        json_printf(&out, "[");
        int written = 0;
        for (unsigned int index = start_index; index < Size; ++index) {
            auto &currentValue = data.values[index];
            if (currentValue.initialized) {
                written += printHook(out, currentValue.name, currentValue.value, index, written == 0);
            }
        }
        json_printf(&out, " ]");
        event.result.collection_result = CollectionOperationResult::ok;

        return data;
    }
    
    template<ValidBaseType BaseType, typename RuntimeType, size_t Size, size_t UID>
    bool EventAccessArray<BaseType, RuntimeType, Size, UID>::hasValidRuntimeData(int index) const {
        if (index >= NumElements) {
            return false;
        }

        return data.values[index].initialized && runtimeData[index].has_value();
    }

    template<ValidBaseType BaseType, typename RuntimeType, size_t Size, size_t UID>
    auto EventAccessArray<BaseType, RuntimeType, Size, UID>::getTrivialRepresentation() const -> const TrivialRepresentationType & {
        std::unique_lock instanceGuard{instanceMutex};

        return data;
    }

    template<ValidBaseType BaseType, typename RuntimeType, size_t Size, size_t UID>
    template<typename Callable>
    void EventAccessArray<BaseType, RuntimeType, Size, UID>::invokeOnRuntimeData(IndexOrName index, Callable callable) {
        std::unique_lock instanceGuard{instanceMutex};

        auto findIndexResult = findIndex(index, false);
        if (!findIndexResult)
        {
            return;
        }

        if (hasValidRuntimeData(*findIndexResult)) {
            callable(*runtimeData[*findIndexResult]);
        }
    }

    template<ValidBaseType BaseType, typename RuntimeType, size_t Size, size_t UID>
    template<typename Callable>
    void EventAccessArray<BaseType, RuntimeType, Size, UID>::invokeOnRuntimeData(IndexOrName index, Callable callable) const {
        std::unique_lock instanceGuard{instanceMutex};

        auto findIndexResult = findIndex(index, false);
        if (!findIndexResult)
        {
            return;
        }

        if (hasValidRuntimeData(*findIndexResult)) {
            callable(*runtimeData[*findIndexResult]);
        }
    }

    template<ValidBaseType BaseType, typename RuntimeType, size_t Size, size_t UID>
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
