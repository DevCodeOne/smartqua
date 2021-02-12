#include "http_utils.h"

#include <string_view>
#include <cstdlib>
#include <optional>

std::optional<int> extract_index_from_uri(const char *uri) { 
    std::string_view uri_view = uri;
    auto argument = uri_view.find_last_of("/");
    std::optional<int> index{};

    if (argument + 1 != uri_view.size()) {
        int new_index = 0;
        int result = std::sscanf(uri_view.data() + argument + 1, "%d", &new_index);

        if (result) {
            index = new_index;
        }
    }

    return index;
}