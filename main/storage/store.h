#pragma once

#include <thread>
#include <tuple>
#include <mutex>

#include "utils/utils.h"
#include "utils/logger.h"
#include "build_config.h"

using IgnoredEvent = void;

namespace Detail {
    template<typename StoreType, typename SaveType, typename EventType>
    static void handleStore(StoreType &current_store, SaveType &save_type, EventType &event, bool deferSaving) {
        using ReturnType = decltype(current_store.dispatch(event));

        if constexpr (not std::is_same_v<ReturnType, IgnoredEvent>) {
            if (!deferSaving) {
                Logger::log(LogLevel::Info, "Store to sd ...");
                // current_store is thread-safe, as well as save_type.set_value
                save_type.set_value(current_store.dispatch(event));
            } else {
                Logger::log(LogLevel::Info, "Defer storing to sd ...");
                current_store.dispatch(event);
            }
        }
    }
}

template<typename StoreType, typename PersistType>
struct SingleTypeStore {
    StoreType sstore;
    PersistType ssave;
};

// TODO: add store type specific prefix to the names, storetypes have to be unique
// TODO: maybe add mutex for every single store, so that one store can trigger another one, but not itself
template<typename ... StoreTypes>
class Store {
    public:
        template<typename EventType>
        void writeEvent(EventType &event, bool deferSaving = false) {
            Logger::log(LogLevel::Info, "Write event to store");
            initValues();

            ConstexprFor<(sizeof...(StoreTypes)) - 1>::doCall(_stores, [&event, &deferSaving](auto &current_store){
                Detail::handleStore(current_store.sstore, current_store.ssave, event, deferSaving);
            });
        }


        template<typename EventType>
        void readEvent(EventType &event) {
            Logger::log(LogLevel::Info, "Read event from store entry");
            initValues();

            ConstexprFor<(sizeof...(StoreTypes)) - 1>::doCall(_stores, [&event](const auto &currentStore){
                currentStore.sstore.dispatch(event);
            });
            Logger::log(LogLevel::Info, "Read event from store exit");
        }

        // TODO: implement
        void reloadFromFileSystem() {

        }

        void initValues() {
            Logger::log(LogLevel::Info, "Entry initValues");
            static std::once_flag _init_flag;
            std::call_once(_init_flag, []() {
                Logger::log(LogLevel::Info, "Init values of central store");
                ConstexprFor<(sizeof...(StoreTypes)) - 1>::doCall(_stores, [](auto &current_store){
                    Logger::log(LogLevel::Info, "Initializing current type");
                    current_store.sstore = current_store.ssave.get_value();
                });
            });
            Logger::log(LogLevel::Info, "Exit initValues");
        }

    private:
        static inline std::tuple<StoreTypes ...> _stores;
};


