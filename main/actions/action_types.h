#pragma once

enum struct collection_operation_result {
    ok, collection_full, index_invalid, failed
};

enum struct json_action_result_value {
    successfull, not_found, failed
};

struct json_action_result  {
    int answer_len = -1;
    json_action_result_value result;
};