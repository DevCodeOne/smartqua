#pragma once

enum struct I2cAddressValue : uint8_t { Invalid = 0xFF };

template<>
struct read_from_json<I2cAddressValue> {
    static void read(const char *str, int len, I2cAddressValue &addr) {
        std::string_view input(str, len);
        int base = 10;

        addr = I2cAddressValue::Invalid;
        const char *start = str;

        if (input.starts_with("0b")) {
            str += 2;
            base = 2;
        } else if (input.starts_with("0x")) {
            str += 2;
            base = 16;
        } else if (input.starts_with("0")) {
            str += 1;
            base = 8;
        }

        const char *end = str + len;
        auto asIntValue{static_cast<std::underlying_type_t<I2cAddressValue>>(I2cAddressValue::Invalid)};
        auto result = std::from_chars(start, end, asIntValue, base);

        if (result.ec == std::errc()) {
            addr = static_cast<I2cAddressValue>(asIntValue + 0x20);
        }
    }
};
