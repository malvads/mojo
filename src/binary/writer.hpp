#ifndef MOJO_BINARY_WRITER_HPP
#define MOJO_BINARY_WRITER_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace Mojo::Binary {
class Writer {
public:
    explicit Writer(std::vector<uint8_t>& data) : data_(data) {
    }

    void write_uint8(uint8_t value);
    void write_uint16_be(uint16_t value);
    void write_uint16_le(uint16_t value);
    void write_uint32_be(uint32_t value);
    void write_uint32_le(uint32_t value);
    void write_uint64_be(uint64_t value);
    void write_uint64_le(uint64_t value);
    void write_string(const std::string& value);

    template <typename T>
    void write(const T& value) {
        data_.insert(data_.end(),
                     reinterpret_cast<const uint8_t*>(&value),
                     reinterpret_cast<const uint8_t*>(&value) + sizeof(T));
    }

private:
    std::vector<uint8_t>& data_;
};
}  // namespace Mojo::Binary

#endif  // MOJO_BINARY_WRITER_HPP