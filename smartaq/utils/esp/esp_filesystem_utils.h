#pragma once

#include "esp_log.h"
#include "esp_partition.h"

#include "drivers/system_info.h"

template<DeviceKind device>
class MMAPPartition;

template<>
class MMAPPartition<DeviceKind::ESPDevice> final {
public:
    MMAPPartition (const MMAPPartition &) = delete;

    MMAPPartition (MMAPPartition &&other) :
        outPtr(std::move(other.outPtr)),
        partitionSize(std::move(other.partitionSize)),
        outHandle(std::move(other.outHandle)) {
        other.outPtr = nullptr;
        other.partitionSize = 0;
        other.outHandle = 0;
    }

    MMAPPartition &operator=(const MMAPPartition &) = delete;
    MMAPPartition &operator=(MMAPPartition &&other) {
        using std::swap;

        swap(outPtr, other.outPtr);
        swap(partitionSize, other.partitionSize);
        swap(outHandle, other.outHandle);

        return *this;
    }

    ~MMAPPartition() {
        if (outPtr) {
            esp_partition_munmap(outHandle);
        }
    }

    const void *data() const {
        return outPtr;
    }

    size_t size() const {
        return partitionSize;
    }

private:
    const void *outPtr = nullptr;
    size_t partitionSize = 0;
    esp_partition_mmap_handle_t outHandle = 0;

    MMAPPartition(const void *outPtr, size_t partitionSize, esp_partition_mmap_handle_t outHandle)
        : outPtr(outPtr), partitionSize(partitionSize), outHandle(outHandle) { }

    template<DeviceKind device>
    friend std::optional<MMAPPartition<device>> mapPartition(const char *partitionName);
};

template<DeviceKind device>
std::optional<MMAPPartition<device>> mapPartition(const char *partitionName) {
    const auto *partition = esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, partitionName);

    if (partition == nullptr) {
        return std::nullopt;
    }

    const void *outPtr;
    esp_partition_mmap_handle_t outHandle;

    uint8_t id[32];
    esp_partition_get_sha256(partition, id);

    char output[128];
    int offset = 0;
    for (int i = 0; i < 32; ++i) {
        offset += snprintf(output + offset, (output + sizeof(output)) - (output + offset), "%02x", id[i]);
    }
    ESP_LOGI("mmap partition", "%.*s", 128, output);

    auto mmapResult = esp_partition_mmap(partition, 0, partition->size, ESP_PARTITION_MMAP_DATA, &outPtr, &outHandle);


    if (mmapResult != ESP_OK) {
        return std::nullopt;
    }

    return std::move(MMAPPartition<device>(outPtr, partition->size, outHandle));
}

