#pragma once

#include "esp_log.h"

#include <tuple>
#include <mutex>
#include <shared_mutex>

#include "utils/utils.h"

using ignored_event = void;

namespace Detail {
    template<typename StoreType, typename SaveType, typename EventType, typename ReturnType>
    class return_value_handler {
        public:
        static void handle(StoreType &current_store, SaveType &save_type, EventType &event) {
            // TODO: maybe check if value has changed first ?
            save_type.set_value(current_store.dispatch(event));
        }
    };

    template<typename StoreType, typename SaveType, typename EventType>
    class return_value_handler<StoreType, SaveType, EventType, ignored_event> {
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
// TODO: initialize stores
template<typename ... StoreTypes>
class store {
    public:
        template<typename T>
        void write_event(T &event) {
            init_values();

            ESP_LOGI(__PRETTY_FUNCTION__, "event");
            std::unique_lock instance_guard{_instance_mutex};

            constexpr_for<(sizeof...(StoreTypes)) - 1>::doCall(_stores, [&event](auto &current_store){
                using result_type = decltype(std::declval<decltype(current_store.sstore)>().dispatch(event));

                Detail::return_value_handler<decltype(current_store.sstore), decltype(current_store.ssave), decltype(event), result_type>
                    ::handle(current_store.sstore, current_store.ssave, event);
            });
        }


        template<typename T>
        void read_event(T &event) {
            init_values();

            ESP_LOGI(__PRETTY_FUNCTION__, "event");
            std::shared_lock instance_guard{_instance_mutex};

            constexpr_for<(sizeof...(StoreTypes)) - 1>::doCall(_stores, [&event](const auto &currentStore){
                currentStore.sstore.dispatch(event);
            });
        }

        void init_values() {
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
        static inline std::shared_mutex _instance_mutex;
        static inline std::once_flag _init_flag;
};


