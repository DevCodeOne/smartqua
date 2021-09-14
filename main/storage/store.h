#pragma once

#include "esp_log.h"

#include <tuple>
#include <mutex>

#include "utils/utils.h"

using ignored_event = void;

namespace Detail {
    template<typename StoreType, typename SaveType, typename EventType, typename ReturnType>
    class ReturnValueHandler {
        public:
        static void handle(StoreType &current_store, SaveType &save_type, EventType &event) {
            std::unique_lock instanceGuard{instanceMutex};
            // TODO: maybe check if value has changed first ?
            save_type.set_value(current_store.dispatch(event));
        }

        static inline std::mutex instanceMutex;
    };

    template<typename StoreType, typename SaveType, typename EventType>
    class ReturnValueHandler<StoreType, SaveType, EventType, ignored_event> {
        public:
        static void handle(StoreType &, SaveType &, EventType &) { /* Do nothing, that event is ignored by the storetype */ }
    };
}

template<typename StoreType, typename PersistType>
struct single_store {
    StoreType sstore;
    PersistType ssave;
};

// TODO: add store type specific prefix to the names, storetypes have to be unique
// TODO: maybe add mutex for every single store, so that one store can trigger another one, but not itself
template<typename ... StoreTypes>
class Store {
    public:
        template<typename T>
        void writeEvent(T &event) {
            ESP_LOGI(__PRETTY_FUNCTION__, "event");
            initValues();

            constexpr_for<(sizeof...(StoreTypes)) - 1>::doCall(_stores, [&event](auto &current_store){
                using result_type = decltype(std::declval<decltype(current_store.sstore)>().dispatch(event));

                Detail::ReturnValueHandler<decltype(current_store.sstore), decltype(current_store.ssave), decltype(event), result_type>
                    ::handle(current_store.sstore, current_store.ssave, event);
            });
        }


        template<typename T>
        void readEvent(T &event) {
            ESP_LOGI(__PRETTY_FUNCTION__, "event");
            initValues();

            constexpr_for<(sizeof...(StoreTypes)) - 1>::doCall(_stores, [&event](const auto &currentStore){
                currentStore.sstore.dispatch(event);
            });
        }

        void initValues() {
            static std::once_flag _init_flag;
            std::call_once(_init_flag, []() {
                ESP_LOGI(__PRETTY_FUNCTION__, "Init values");
                constexpr_for<(sizeof...(StoreTypes)) - 1>::doCall(_stores, [](auto &current_store){
                    auto initial_value = current_store.ssave.get_value();
                    current_store.sstore = initial_value;
                });
            });
        }

    private:
        static inline std::tuple<StoreTypes ...> _stores;
};


