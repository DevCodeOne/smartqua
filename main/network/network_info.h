#pragma once

#include <atomic>

#include "esp_wifi.h"

class NetworkInfo {
    public:
        static bool canUseNetwork() {
            return networkFlag.load();
        }

        template<wifi_mode_t T>
        friend class WifiManager;

    private:
        static inline std::atomic_bool networkFlag{false};

        static void allowNetwork() {
            networkFlag.store(true);
        }

        static void disallowNetwork() {
            networkFlag.store(false);
        }


};