#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace Mojo::Binary {
class Reader {
public:
    explicit Reader(const std::vector<uint8_t>& data) : data_(data), offset_(0) {
    }

    uint8_t     read_uint8();
    uint16_t    read_uint16_be();
    uint16_t    read_uint16_le();
    uint32_t    read_uint32_be();
    uint32_t    read_uint32_le();
    uint64_t    read_uint64_be();
    uint64_t    read_uint64_le();
    std::string read_string(size_t length);

    template <typename T>
    T read() {
        T value;
        if (offset_ + sizeof(T) > data_.size()) {
            throw std::out_of_range("Attempt to read past end of buffer.");
        }
        memcpy(&value, data_.data() + offset_, sizeof(T));
        offset_ += sizeof(T);
        return value;
    }

    bool eof() const {
        return offset_ >= data_.size();
    }
    size_t offset() const {
        return offset_;
    }

private:
    const std::vector<uint8_t>& data_;
    size_t                      offset_;
};
}  // namespace Mojo::Binary