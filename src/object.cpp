
#include <cstring>
#include <cstdint>
#include <istream>
#include <object.hpp>

static constexpr uint8_t MAGIC[4] = {'V', 'H', 'X', 'O'};

template <std::integral T>
static T read_little_endian(std::istream& stream) {
    T value = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        value |= static_cast<T>(static_cast<uint8_t>(stream.get())) << (i * 8);
    }
    return value;
}

template <std::integral T>
static T read_big_endian(std::istream& stream) {
    T value = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        value = (value << 8) | static_cast<T>(static_cast<uint8_t>(stream.get()));
    }
    return value;
}

template <std::integral T>
static T write_little_endian(std::ostream& stream, T value) {
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        stream.put(static_cast<char>(static_cast<uint8_t>(value >> (i * 8))));
    }
}

template <std::integral T>
static T write_big_endian(std::ostream& stream, T value) {
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        stream.put(static_cast<char>(static_cast<uint8_t>(value >> ((sizeof(T) - 1 - i) * 8))));
    }
}

#define MODE_LE32   0
#define MODE_BE32   1
#define MODE_LE64   2
#define MODE_BE64   3

struct object_header {
    // All little-endian
    uint32_t magic;
    uint8_t  mode;
    uint8_t  platform;
    uint16_t version;
    uint32_t arch;

    // Depends on mode
    uint64_t file_len;
    uint64_t symbols_off;
    uint64_t sections_off;
    uint64_t relocs_off;
    uint64_t strings_off;
    uint64_t entry_off;
    uint64_t symbols_len;
    uint64_t sections_len;
    uint64_t relocs_len;
    uint64_t strings_len;
};

struct section_header {
    uint64_t index;
    uint64_t name_off;
    uint64_t addr;
    uint64_t offset;
    uint64_t alignment;
    uint64_t size;
    uint32_t flags;
};

struct reloc_header {
    uint64_t section_index;
    uint64_t name_off;
    uint64_t offset;
    uint64_t addend;
    uint32_t type;
};

#define REL_NONE 0
#define REL_R_VHX_PC12 1
#define REL_R_VHX_PCA8 2
#define REL_R_VHX_PCS8 3
#define REL_R_VHX_REL32 4
#define REL_R_VHX_REL64 5
#define REL_R_VHX_ABS32 6
#define REL_R_VHX_ABS64 7

struct symbol_header {
    uint64_t section_index;
    uint64_t name_off;
    uint64_t value;
    uint32_t flags;
};


static vhxcc::object read_object_file(std::istream& stream) {
    std::size_t offset = stream.tellg();
    uint32_t magic = read_little_endian<uint32_t>(stream);
    if (std::memcmp(&magic, &MAGIC, 4) != 0) {
        throw std::runtime_error("invalid magic number");
    }
    object_header header;
    header.magic = magic;
    header.mode = static_cast<uint8_t>(stream.get());
    header.platform = static_cast<uint8_t>(stream.get());
    header.version = read_little_endian<uint16_t>(stream);
    header.arch = read_little_endian<uint32_t>(stream);

    bool is_little_endian = (header.mode == MODE_LE32 || header.mode == MODE_LE64);
    bool is_64 = (header.mode == MODE_LE64 || header.mode == MODE_BE64);

    static auto read_value = [&](std::istream& stream) -> uint64_t {
        if (is_little_endian) {
            if (!is_64) {
                return read_little_endian<uint32_t>(stream);
            }
            return read_little_endian<uint64_t>(stream);
        } else {
            if (!is_64) {
                return read_big_endian<uint32_t>(stream);
            }
            return read_big_endian<uint64_t>(stream);
        }
    };

    header.file_len = read_value(stream);
    header.symbols_off = read_value(stream);
    header.sections_off = read_value(stream);
    header.relocs_off = read_value(stream);
    header.strings_off = read_value(stream);
    header.entry_off = read_value(stream);
    header.symbols_len = read_value(stream);
    header.sections_len = read_value(stream);
    header.relocs_len = read_value(stream);
    header.strings_len = read_value(stream);

    vhxcc::object obj;

    std::vector<section_header> sections;
    std::vector<reloc_header> relocs;
    std::vector<symbol_header> symbols;
    
}