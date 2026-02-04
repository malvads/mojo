#include "reader.hpp"

namespace Mojo::Binary {

enum { READ_BIG_ENDIAN, READ_LITTLE_ENDIAN };

namespace {
template <typename T>
T read_int_impl(const std::vector<uint8_t>& data, size_t& offset, int endianness) {
    if (offset + sizeof(T) > data.size()) {
        throw std::out_of_range("Attempt to read past end of buffer.");
    }
    T value = 0;
    if (endianness == READ_BIG_ENDIAN) {
        for (size_t i = 0; i < sizeof(T); ++i) {
            value = (value << 8) | data[offset + i];
        }
    }
    else {
        for (size_t i = 0; i < sizeof(T); ++i) {
            value |= static_cast<T>(data[offset + i]) << (i * 8);
        }
    }
    offset += sizeof(T);
    return value;
}
}  // namespace

uint8_t Reader::read_uint8() {
    if (offset_ >= data_.size()) {
        throw std::out_of_range("Attempt to read past end of buffer.");
    }
    return data_[offset_++];
}

uint16_t Reader::read_uint16_be() {
    return read_int_impl<uint16_t>(data_, offset_, READ_BIG_ENDIAN);
}

uint16_t Reader::read_uint16_le() {
    return read_int_impl<uint16_t>(data_, offset_, READ_LITTLE_ENDIAN);
}

uint32_t Reader::read_uint32_be() {
    return read_int_impl<uint32_t>(data_, offset_, READ_BIG_ENDIAN);
}

uint32_t Reader::read_uint32_le() {
    return read_int_impl<uint32_t>(data_, offset_, READ_LITTLE_ENDIAN);
}

uint64_t Reader::read_uint64_be() {
    return read_int_impl<uint64_t>(data_, offset_, READ_BIG_ENDIAN);
}

uint64_t Reader::read_uint64_le() {
    return read_int_impl<uint64_t>(data_, offset_, READ_LITTLE_ENDIAN);
}

std::string Reader::read_string(size_t length) {
    if (offset_ + length > data_.size())
        length = data_.size() - offset_;
    std::string s(reinterpret_cast<const char*>(data_.data() + offset_), length);
    offset_ += length;
    return s;
}
}  // namespace Mojo::Binary