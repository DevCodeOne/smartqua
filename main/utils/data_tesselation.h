#pragma once

#include <chrono>
#include <utility>

using range_type = std::pair<std::chrono::seconds, std::chrono::seconds>;

struct SampleType {
    std::chrono::seconds sample_span;
};

template<typename StorageType>
class data_tesselation {
    public:
        void put_data();
        void read_data(range_type range);
    private:
};