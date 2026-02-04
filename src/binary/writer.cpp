#include "writer.hpp"

namespace Mojo::Binary {

enum { WRITE_BIG_ENDIAN, WRITE_LITTLE_ENDIAN };

namespace {
template <typename T>
void write_int_impl(std::vector<uint8_t>& data, T value, int endianness) {
    if (endianness == WRITE_BIG_ENDIAN) {
        for (size_t i = 0; i < sizeof(T); ++i) {
            data.push_back((value >> ((sizeof(T) - 1 - i) * 8)) & 0xFF);
        }
    }
    else {
        for (size_t i = 0; i < sizeof(T); ++i) {
            data.push_back((value >> (i * 8)) & 0xFF);
        }
    }
}
}  // namespace

void Writer::write_uint8(uint8_t value) {
    data_.push_back(value);
}

void Writer::write_uint16_be(uint16_t value) {
    write_int_impl(data_, value, WRITE_BIG_ENDIAN);
}

void Writer::write_uint16_le(uint16_t value) {
    write_int_impl(data_, value, WRITE_LITTLE_ENDIAN);
}

void Writer::write_uint32_be(uint32_t value) {
    write_int_impl(data_, value, WRITE_BIG_ENDIAN);
}

void Writer::write_uint32_le(uint32_t value) {
    write_int_impl(data_, value, WRITE_LITTLE_ENDIAN);
}

void Writer::write_uint64_be(uint64_t value) {
    write_int_impl(data_, value, WRITE_BIG_ENDIAN);
}

void Writer::write_uint64_le(uint64_t value) {
    write_int_impl(data_, value, WRITE_LITTLE_ENDIAN);
}

void Writer::write_string(const std::string& value) {
    data_.insert(data_.end(), value.begin(), value.end());
}
}  // namespace Mojo::Binary