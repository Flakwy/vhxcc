
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
    uint32_t index;
    uint32_t flags;
    uint64_t name_off;
    uint64_t addr;
    uint64_t offset;
    uint64_t alignment;
    uint64_t size;
};

struct reloc_header {
    uint32_t section_index;
    uint32_t type;
    uint64_t name_off;
    uint64_t offset;
    uint64_t addend;
};

#define REL_R_VHX_PC12 0
#define REL_R_VHX_PCA8 1
#define REL_R_VHX_PCS8 2
#define REL_R_VHX_REL32 3
#define REL_R_VHX_REL64 4
#define REL_R_VHX_ABS32 5
#define REL_R_VHX_ABS64 6

struct symbol_header {
    uint32_t section_index;
    uint32_t flags;
    uint64_t name_off;
    uint64_t value;
};

#define SYM_FLAG_GLOBAL 1
#define SYM_FLAG_DEFINED 2
#define SYM_FLAG_ABSOLUTE 4

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
    
    stream.seekg(offset + header.sections_off, std::ios::beg);
    sections.resize(header.sections_len / sizeof(section_header));
    for (std::size_t i = 0; i < sections.size(); ++i) {
        sections[i].index = read_little_endian<uint32_t>(stream);
        sections[i].flags = read_little_endian<uint32_t>(stream);
        sections[i].name_off = read_value(stream);
        sections[i].offset = read_value(stream);
        sections[i].alignment = read_value(stream);
        sections[i].size = read_value(stream);
    }

    stream.seekg(offset + header.relocs_off, std::ios::beg);
    relocs.resize(header.relocs_len / sizeof(reloc_header));
    for (std::size_t i = 0; i < relocs.size(); ++i) {
        relocs[i].section_index = read_value(stream);
        relocs[i].type = read_little_endian<uint32_t>(stream);
        relocs[i].offset = read_value(stream);
        relocs[i].addend = read_value(stream);
    }

    stream.seekg(offset + header.symbols_off, std::ios::beg);
    symbols.resize(header.symbols_len / sizeof(symbol_header));
    for (std::size_t i = 0; i < symbols.size(); ++i) {
        symbols[i].name_off = read_value(stream);
        symbols[i].flags = read_little_endian<uint32_t>(stream);
        symbols[i].section_index = read_value(stream);
        symbols[i].value = read_value(stream);
    }

    // Map sections by their index
    std::unordered_map<uint32_t, section_header> section_map;
    for (const auto& section : sections) {
        section_map[section.index] = section;
    }

    // Populate object sections
    for (const auto& section : sections) {
        
    }
}