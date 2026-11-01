
#include <cstdint>
#include <istream>
#include <object.hpp>

static constexpr uint32_t MAGIC = 'OXHV';

static vhxcc::object read_object_file(std::istream& stream) {
    std::size_t pos = stream.tellg();
    uint8_t magic[4];
    stream.read(reinterpret_cast<char*>(&magic), 4);
}