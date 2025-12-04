#include "json_utils.h"

std::string_view tokenToStringView(json_token token, std::optional<int> maxLen)
{
    return std::string_view(token.ptr, std::min(token.len, maxLen.value_or(token.len)));
}