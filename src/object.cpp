
#include <climits>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <istream>
#include <object.hpp>
#include <sstream>

#define VERSION 1

static constexpr uint8_t MAGIC[4] = {'V', 'H', 'X', 'O'};

template <std::integral T>
static T read_little_endian(std::istream& stream) {
    T value = 0;
    for (std::size_t i = 0; i < sizeof(T); i++) {
        value |= static_cast<T>(static_cast<uint8_t>(stream.get())) << (i * 8);
    }
    return value;
}

template <std::integral T>
static T read_big_endian(std::istream& stream) {
    T value = 0;
    for (std::size_t i = 0; i < sizeof(T); i++) {
        value = (value << 8) | static_cast<T>(static_cast<uint8_t>(stream.get()));
    }
    return value;
}

template <std::integral T>
static void write_little_endian(std::ostream& stream, T value) {
    for (std::size_t i = 0; i < sizeof(T); i++) {
        stream.put(static_cast<char>(static_cast<uint8_t>(value >> (i * 8))));
    }
}

template <std::integral T>
static void write_big_endian(std::ostream& stream, T value) {
    for (std::size_t i = 0; i < sizeof(T); i++) {
        stream.put(static_cast<char>(static_cast<uint8_t>(value >> ((sizeof(T) - 1 - i) * 8))));
    }
}

#define MODE_LE32   0
#define MODE_BE32   1
#define MODE_LE64   2
#define MODE_BE64   3

#define PLATFORM_NONE    0x00
#define PLATFORM_LINUX   0x01
#define PLATFORM_UNKNOWN 0xFF

static std::string platform_to_string(uint8_t platform) {
    switch (platform) {
        case PLATFORM_NONE: return "none";
        case PLATFORM_LINUX: return "linux";
        case PLATFORM_UNKNOWN: return "unknown";
        default: return "unknown";
    }
}

static uint8_t string_to_platform(const std::string& platform) {
    if (platform == "none") return PLATFORM_NONE;
    if (platform == "linux") return PLATFORM_LINUX;
    return PLATFORM_UNKNOWN;
}

#define ARCH_VHX32  (0b000 << 0)
#define ARCH_VHX64  (0b001 << 0)
#define ARCH_VHX128 (0b010 << 0)

static std::string arch_to_string(uint32_t arch) {
    std::string result;
    switch (arch & 0x7) {
        case ARCH_VHX32:
            result = "vhx32";
            break;
        case ARCH_VHX64:
            result = "vhx64";
            break;
        case ARCH_VHX128:
            result = "vhx128";
            break;
        default: throw std::runtime_error("unknown architecture");
    }
    // canonical representation of the architecture string
    if (arch & (1UL << static_cast<uint32_t>('i' - 'a' + 3)))
        result += 'i';
    if (arch & (1UL << static_cast<uint32_t>('m' - 'a' + 3)))
        result += 'm';
    if (arch & (1UL << static_cast<uint32_t>('s' - 'a' + 3)))
        result += 's';
    if (arch & (1UL << static_cast<uint32_t>('a' - 'a' + 3)))
        result += 'a';
    if (arch & (1UL << static_cast<uint32_t>('f' - 'a' + 3)))
        result += 'f';
    if (arch & (1UL << static_cast<uint32_t>('b' - 'a' + 3)))
        result += 'b';
    // the rest in alphabetical order
    // for now
    for (uint32_t i = 0; i < 26; i++) {
        // skip the bits that have already been handled
        if (i == static_cast<uint32_t>('i' - 'a') || i == static_cast<uint32_t>('m' - 'a') || i == static_cast<uint32_t>('s' - 'a') || i == static_cast<uint32_t>('a' - 'a') || i == static_cast<uint32_t>('f' - 'a') || i == static_cast<uint32_t>('b' - 'a'))
            continue;
        if (arch & (1UL << (i + 3)))
            result += static_cast<char>('a' + i);
    }
    return result;
}

static uint32_t string_to_arch(const std::string& arch) {
    uint32_t result = 0;
    std::size_t pos = 0;
    if (arch.substr(0, 5) == "vhx32") {
        pos = 5;
        result |= ARCH_VHX32;
    } else if (arch.substr(0, 5) == "vhx64") {
        pos = 5;
        result |= ARCH_VHX64;
    } else if (arch.substr(0, 6) == "vhx128") {
        pos = 6;
        result |= ARCH_VHX128;
    }
    for (size_t i = pos; i < arch.size(); i++) {
        char c = arch[i];
        if (c >= 'a' && c <= 'z')
            result |= 1UL << (static_cast<uint32_t>(c - 'a') + 3);
    }
    return result;
}

struct object_header64 {
    // All little-endian
    uint32_t magic;
    uint8_t  mode;
    uint8_t  platform;
    uint16_t version;
    uint32_t arch;
    uint32_t reserved;

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

struct object_header32 {
    // All little-endian
    uint32_t magic;
    uint8_t  mode;
    uint8_t  platform;
    uint16_t version;
    uint32_t arch;
    uint32_t reserved;

    // Depends on mode
    uint32_t file_len;
    uint32_t symbols_off;
    uint32_t sections_off;
    uint32_t relocs_off;
    uint32_t strings_off;
    uint32_t entry_off;
    uint32_t symbols_len;
    uint32_t sections_len;
    uint32_t relocs_len;
    uint32_t strings_len;
};

struct section_header64 {
    uint32_t index;
    uint32_t flags;
    uint64_t name_off;
    uint64_t addr;
    uint64_t data_off;
    uint64_t alignment;
    uint64_t size;
};

struct section_header32 {
    uint32_t index;
    uint32_t flags;
    uint32_t name_off;
    uint32_t addr;
    uint32_t data_off;
    uint32_t alignment;
    uint32_t size;
};

struct reloc_header64 {
    uint32_t section_index;
    uint32_t type;
    uint64_t symbol_name_off;
    uint64_t offset;
    uint64_t addend;
};

struct reloc_header32 {
    uint32_t section_index;
    uint32_t type;
    uint32_t symbol_name_off;
    uint32_t offset;
    uint32_t addend;
};

#define REL_R_VHX_PC12 0
#define REL_R_VHX_PCA8 1
#define REL_R_VHX_PCS8 2
#define REL_R_VHX_REL32 3
#define REL_R_VHX_REL64 4
#define REL_R_VHX_ABS32 5
#define REL_R_VHX_ABS64 6

static enum vhxcc::relocation::type to_reloc_type(uint32_t type) {
    switch (type) {
        case REL_R_VHX_PC12: return vhxcc::relocation::type::vhx_pc12;
        case REL_R_VHX_PCA8: return vhxcc::relocation::type::vhx_pca8;
        case REL_R_VHX_PCS8: return vhxcc::relocation::type::vhx_pcs8;
        case REL_R_VHX_REL32: return vhxcc::relocation::type::vhx_rel32;
        case REL_R_VHX_REL64: return vhxcc::relocation::type::vhx_rel64;
        case REL_R_VHX_ABS32: return vhxcc::relocation::type::vhx_abs32;
        case REL_R_VHX_ABS64: return vhxcc::relocation::type::vhx_abs64;
        default: throw std::runtime_error("unknown relocation type");
    }
}

static uint32_t reloc_type_to_uint32(enum vhxcc::relocation::type type) {
    switch (type) {
        case vhxcc::relocation::type::vhx_pc12: return REL_R_VHX_PC12;
        case vhxcc::relocation::type::vhx_pca8: return REL_R_VHX_PCA8;
        case vhxcc::relocation::type::vhx_pcs8: return REL_R_VHX_PCS8;
        case vhxcc::relocation::type::vhx_rel32: return REL_R_VHX_REL32;
        case vhxcc::relocation::type::vhx_rel64: return REL_R_VHX_REL64;
        case vhxcc::relocation::type::vhx_abs32: return REL_R_VHX_ABS32;
        case vhxcc::relocation::type::vhx_abs64: return REL_R_VHX_ABS64;
        default: throw std::runtime_error("unknown relocation type");
    }
}

struct symbol_header64 {
    uint32_t section_index;
    uint32_t flags;
    uint64_t name_off;
    uint64_t value;
};

struct symbol_header32 {
    uint32_t section_index;
    uint32_t flags;
    uint32_t name_off;
    uint32_t value;
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
    object_header64 header;
    header.magic = magic;
    header.mode = read_little_endian<uint8_t>(stream);
    header.platform = read_little_endian<uint8_t>(stream);
    header.version = read_little_endian<uint16_t>(stream);
    header.arch = read_little_endian<uint32_t>(stream);

    if (header.version != VERSION) {
        throw std::runtime_error("unsupported object file version");
    }

    bool is_little_endian = (header.mode == MODE_LE32 || header.mode == MODE_LE64);
    bool is_64 = (header.mode == MODE_LE64 || header.mode == MODE_BE64);

    auto read_value = [&](std::istream& stream) -> uint64_t {
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

    header.reserved = read_little_endian<uint32_t>(stream);

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

    std::vector<section_header64> sections;
    std::vector<reloc_header64> relocs;
    std::vector<symbol_header64> symbols;
    
    stream.seekg(offset + header.sections_off, std::ios::beg);
    if (is_64) {
        sections.resize(header.sections_len / sizeof(section_header64));
    } else {
        sections.resize(header.sections_len / sizeof(section_header32));
    }
    for (std::size_t i = 0; i < sections.size(); i++) {
        sections[i].index = read_little_endian<uint32_t>(stream);
        sections[i].flags = read_little_endian<uint32_t>(stream);
        sections[i].name_off = read_value(stream);
        sections[i].addr = read_value(stream);
        sections[i].data_off = read_value(stream);
        sections[i].alignment = read_value(stream);
        sections[i].size = read_value(stream);

    }

    stream.seekg(offset + header.relocs_off, std::ios::beg);
    if (is_64) {
        relocs.resize(header.relocs_len / sizeof(reloc_header64));
    } else {
        relocs.resize(header.relocs_len / sizeof(reloc_header32));
    }
    for (std::size_t i = 0; i < relocs.size(); i++) {
        relocs[i].section_index = read_little_endian<uint32_t>(stream);
        relocs[i].type = read_little_endian<uint32_t>(stream);
        relocs[i].symbol_name_off = read_value(stream);
        relocs[i].offset = read_value(stream);
        relocs[i].addend = read_value(stream);

    }

    stream.seekg(offset + header.symbols_off, std::ios::beg);
    if (is_64) {
        symbols.resize(header.symbols_len / sizeof(symbol_header64));
    } else {
        symbols.resize(header.symbols_len / sizeof(symbol_header32));
    }
    for (std::size_t i = 0; i < symbols.size(); i++) {
        symbols[i].section_index = read_little_endian<uint32_t>(stream);
        symbols[i].flags = read_little_endian<uint32_t>(stream);
        symbols[i].name_off = read_value(stream);
        symbols[i].value = read_value(stream);

    }

    obj.platform(platform_to_string(header.platform));
    obj.arch(arch_to_string(header.arch));
    obj.entry(header.entry_off);

    // Map sections by their index
    std::unordered_map<uint32_t, section_header64> section_map;
    for (const auto& section : sections) {
        section_map[section.index] = section;
    }

    // Populate object sections
    for (const auto& section : sections) {
        std::string name;
        stream.seekg(offset + header.strings_off + section.name_off, std::ios::beg);
        while (stream.peek() != '\0') {
            name += static_cast<char>(stream.get());
        }
        stream.get(); // consume the null terminator
        obj.section(name, section.alignment);
        std::size_t remaining = section.size;
        stream.seekg(offset + section.data_off, std::ios::beg);
        while (remaining > 0) {
            std::size_t chunk_size = std::min(remaining, static_cast<std::size_t>(1024));
            std::vector<char> buffer(chunk_size);
            stream.read(buffer.data(), chunk_size);
            obj.append(name, buffer);
            remaining -= chunk_size;
        }
    }

    // Populate object relocations
    for (const auto& reloc : relocs) {
        auto it = section_map.find(reloc.section_index);
        if (it != section_map.end()) {
            std::string section_name;
            stream.seekg(offset + header.strings_off + it->second.name_off, std::ios::beg);
            while (stream.peek() != '\0') {
                section_name += static_cast<char>(stream.get());
            }
            stream.get(); // consume the null terminator
            std::string symbol_name;
            stream.seekg(offset + header.strings_off + reloc.symbol_name_off, std::ios::beg);
            while (stream.peek() != '\0') {
                symbol_name += static_cast<char>(stream.get());
            }
            stream.get(); // consume the null terminator
            obj.relocation(section_name, symbol_name, to_reloc_type(reloc.type), reloc.offset, reloc.addend);
        }
    }

    // Populate object symbols
    for (const auto& symbol : symbols) {
        std::string name;
        // somehow getting an offset of the second symbol of 21474836480
        stream.seekg(offset + header.strings_off + symbol.name_off, std::ios::beg);
        // if (stream.tellg() == -1)
        while (stream.peek() != '\0') {
            name += static_cast<char>(stream.get());
        }
        stream.get(); // consume the null terminator
        if (symbol.section_index == 0) {
            if ((symbol.flags & SYM_FLAG_DEFINED) != 0)
                obj.define(name, symbol.value, (symbol.flags & SYM_FLAG_GLOBAL) != 0);
            else
                obj.define(name, (bool)(symbol.flags & SYM_FLAG_GLOBAL) != 0);
        } else {
            auto it = section_map.find(symbol.section_index);
            if (it != section_map.end()) {
                std::string section_name;
                stream.seekg(offset + header.strings_off + it->second.name_off, std::ios::beg);
                while (stream.peek() != '\0') {
                    section_name += static_cast<char>(stream.get());
                }
                stream.get(); // consume the null terminator
                if ((symbol.flags & SYM_FLAG_DEFINED) != 0)
                    obj.define(section_name, name, symbol.value, (symbol.flags & SYM_FLAG_GLOBAL) != 0);
                else
                    obj.define(section_name, name, (bool)(symbol.flags & SYM_FLAG_GLOBAL) != 0);
            }
        }
    }

    return obj;
}

static std::ostream& write_object_file(std::ostream& stream, const vhxcc::object& obj) {
    constexpr std::size_t pointer_bits = sizeof(void*) * CHAR_BIT;

    bool is_little_endian = true;
    bool is_64 = pointer_bits == 64;

    auto write_value = [&](std::ostream& stream, uint64_t value) {
        if (is_little_endian) {
            if (is_64) {
                write_little_endian<uint64_t>(stream, value);
            } else {
                write_little_endian<uint32_t>(stream, static_cast<uint32_t>(value));
            }
        } else {
            if (is_64) {
                write_big_endian<uint64_t>(stream, value);
            } else {
                write_big_endian<uint32_t>(stream, static_cast<uint32_t>(value));
            }
        }
    };

    std::unordered_map<std::string, std::size_t> string_cache;
    std::unordered_map<std::string, uint32_t> section_indexes;

    std::stringstream string_block;
    std::stringstream data_block;

    // write sections
    std::vector<section_header64> section_headers;
    std::vector<reloc_header64> reloc_headers;
    for (const auto& [section_name, section] : obj.sections()) {
        if (string_cache.find(section_name) == string_cache.end()) {
            string_cache.emplace(section_name, string_block.str().size());
            string_block.write(section_name.data(), section_name.size());
            string_block.put('\0');
        }

        // section 0 is reserved for the null section
        uint32_t section_index = static_cast<uint32_t>(section_indexes.size() + 1);
        section_indexes.emplace(section_name, section_index);

        section_header64 header;
        header.index = section_index;
        header.flags = 0;
        header.name_off = static_cast<uint64_t>(string_cache[section_name]);
        header.addr = section.virt_addr;
        header.data_off = static_cast<uint64_t>(data_block.str().size());
        header.alignment = section.alignment;
        header.size = section.data.size();

        data_block.write(section.data.data(), section.data.size());

        for (const auto& reloc : section.relocations) {
            if (string_cache.find(reloc.symbol) == string_cache.end()) {
                string_cache.emplace(reloc.symbol, string_block.str().size());
                string_block.write(reloc.symbol.data(), reloc.symbol.size());
                string_block.put('\0');
            }
            reloc_header64 reloc_header;
            reloc_header.section_index = section_index;
            reloc_header.type = reloc_type_to_uint32(reloc.type);
            reloc_header.symbol_name_off = static_cast<uint64_t>(string_cache[reloc.symbol]);
            reloc_header.offset = reloc.offset;
            reloc_header.addend = reloc.addend;
            reloc_headers.push_back(reloc_header);
        }

        section_headers.push_back(header);
    }

    // write symbols
    std::vector<symbol_header64> symbol_headers;
    for (const auto& [symbol_name, symbol] : obj.symbols()) {
        if (string_cache.find(symbol_name) == string_cache.end()) {
            string_cache.emplace(symbol_name, string_block.str().size());
            string_block.write(symbol_name.data(), symbol_name.size());
            string_block.put('\0');
        }
        symbol_header64 symbol_header;
        if (symbol.section == "") {
            symbol_header.section_index = 0;
        } else if (section_indexes.find(symbol.section) != section_indexes.end()) {
            symbol_header.section_index = section_indexes[symbol.section];
        } else {
            throw std::runtime_error("symbol section not found: " + symbol.section);
        }
        symbol_header.flags = 0;
        if (symbol.is_absolute)
            symbol_header.flags |= SYM_FLAG_ABSOLUTE;
        if (symbol.is_defined)
            symbol_header.flags |= SYM_FLAG_DEFINED;
        if (symbol.is_global)
            symbol_header.flags |= SYM_FLAG_GLOBAL;
        symbol_header.name_off = static_cast<uint64_t>(string_cache[symbol_name]);
        symbol_header.value = symbol.value;
        symbol_headers.push_back(symbol_header);
    }
    
    std::size_t header_size = is_64 ? sizeof(object_header64) : sizeof(object_header32);
    std::size_t section_header_size = section_headers.size() * (is_64 ? sizeof(section_header64) : sizeof(section_header32));
    std::size_t symbol_header_size = symbol_headers.size() * (is_64 ? sizeof(symbol_header64) : sizeof(symbol_header32));
    std::size_t reloc_header_size = reloc_headers.size() * (is_64 ? sizeof(reloc_header64) : sizeof(reloc_header32));
    std::size_t string_block_size = (string_block.str().size() + 3) & ~3;
    std::size_t data_block_size = data_block.str().size();

    std::size_t total_size = header_size + section_header_size + symbol_header_size + reloc_header_size + string_block_size + data_block_size;
    std::size_t data_off = header_size + symbol_header_size + section_header_size + reloc_header_size + string_block_size;

    // begin writing the object file to the stream
    stream.write(reinterpret_cast<const char*>(&MAGIC), 4);
    write_little_endian<uint8_t>(stream, is_64 ? MODE_LE64 : MODE_LE32);
    write_little_endian<uint8_t>(stream, string_to_platform(obj.platform()));
    write_little_endian<uint16_t>(stream, VERSION);
    write_little_endian<uint32_t>(stream, string_to_arch(obj.arch()));
    write_little_endian<uint32_t>(stream, 0); // reserved

    // file_len
    write_value(stream, total_size);
    // symbols_off
    write_value(stream, header_size);
    // sections_off
    write_value(stream, header_size + symbol_header_size);
    // relocs_off
    write_value(stream, header_size + symbol_header_size + section_header_size);
    // strings_off
    write_value(stream, header_size + symbol_header_size + section_header_size + reloc_header_size);
    // entry_off
    write_value(stream, obj.entry());
    // symbols_len
    write_value(stream, symbol_header_size);
    // sections_len
    write_value(stream, section_header_size);
    // relocs_len
    write_value(stream, reloc_header_size);
    // strings_len
    write_value(stream, string_block_size);

    for (const auto& symbol_header : symbol_headers) {
        write_little_endian<uint32_t>(stream, symbol_header.section_index);
        write_little_endian<uint32_t>(stream, symbol_header.flags);
        write_value(stream, symbol_header.name_off);
        write_value(stream, symbol_header.value);
    }

    for (const auto& section_header : section_headers) {
        write_little_endian<uint32_t>(stream, section_header.index);
        write_little_endian<uint32_t>(stream, section_header.flags);
        write_value(stream, section_header.name_off);
        write_value(stream, section_header.addr);
        write_value(stream, data_off + section_header.data_off);
        write_value(stream, section_header.alignment);
        write_value(stream, section_header.size);
    }

    for (const auto& reloc_header : reloc_headers) {
        write_little_endian<uint32_t>(stream, reloc_header.section_index);
        write_little_endian<uint32_t>(stream, reloc_header.type);
        write_value(stream, reloc_header.symbol_name_off);
        write_value(stream, reloc_header.offset);
        write_value(stream, reloc_header.addend);
    }

    // write string block
    stream.write(string_block.str().data(), string_block.str().size());
    // pad to 4-byte alignment
    for (std::size_t i = string_block.str().size(); i < string_block_size; i++) {
        stream.write("\0", 1);
    }
    // write data block
    stream.write(data_block.str().data(), data_block_size);

    return stream;
}

vhxcc::object::object(std::istream& stream) {
    *this = read_object_file(stream);
}

vhxcc::object& vhxcc::object::write(std::ostream& stream) {
    write_object_file(stream, *this);
    return *this;
}

vhxcc::object& vhxcc::object::read(std::istream& stream) {
    vhxcc::object tmp = read_object_file(stream);
    return *this << tmp;
}

vhxcc::object& vhxcc::object::operator<<(std::istream& stream) {
    vhxcc::object tmp = read_object_file(stream);
    return *this << tmp;
}

std::ostream& operator<<(std::ostream& stream, const vhxcc::object& obj) {
    write_object_file(stream, obj);
    return stream;
}