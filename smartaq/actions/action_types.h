#pragma once

enum struct CollectionOperationResult {
    ok, collection_full, index_invalid, failed
};

enum struct JsonActionResultStatus {
    success, not_found, failed
};

struct JsonActionResult  {
    int answer_len = -1;
    JsonActionResultStatus result;
};