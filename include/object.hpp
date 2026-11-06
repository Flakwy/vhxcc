#pragma once

#include <cstddef>
#include <istream>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace vhxcc {

struct symbol {
    std::string section;
    std::size_t value;
    bool is_global;
    bool is_defined;
    bool is_absolute;
};

struct relocation {
    enum type {
        // all pc-relative relocation words are stored in little-endian

        // pc-relative 12-bit signed offset
        // synthesis: word(31 downto 20) = (offset - pc)(11 downto 0)
        vhx_pc12,

        // PC-relative unsigned 8-bit magnitude, covering distances 1 through 256.
        // The instruction adds one to the encoded value, so the field is (distance - 1).
        // synthesis:
        // word(15 downto 11) = encoded(4 downto 0)
        // word(31 downto 30) = encoded(6 downto 5)
        // word(19) = encoded(7);
        // pca8 uses target - pc; pcs8 uses pc - target. Both encode positive magnitudes.
        vhx_pca8, // PC-relative 8-bit offset
        vhx_pcs8, // PC-relative 8-bit offset

        // rel32 and rel64 are stored in target endianness, not little-endian
        // relative to the relocation word, not the program counter
        vhx_rel32, // Relative 32-bit offset
        vhx_rel64, // Relative 64-bit offset

        // abs32 and abs64 are stored in target endianness, not little-endian
        vhx_abs32, // Absolute 32-bit offset
        vhx_abs64, // Absolute 64-bit offset

        // lo16, hi16, hlo16, hhi16 are stored in little-endian
        // synthesis: word(31 downto 16) = (offset)(15 downto 0)
        vhx_abs_lo16, // Absolute low 16-bit offset
        // synthesis: word(31 downto 16) = (offset)(31 downto 16)
        vhx_abs_hi16, // Absolute high 16-bit offset
        // synthesis: word(31 downto 16) = (offset)(47 downto 32)
        vhx_abs_hlo16, // Absolute higher low 16-bit offset
        // synthesis: word(31 downto 16) = (offset)(63 downto 48)
        vhx_abs_hhi16  // Absolute higher high 16-bit offset
    } type;

    std::string symbol;
    std::size_t offset;
    std::size_t addend;

    relocation()
        : type(vhx_rel32), symbol(""), offset(0), addend(0) {}
    
    relocation(enum type t, std::string s, std::size_t o, std::size_t a = 0)
        : type(t), symbol(s), offset(o), addend(a) {}
};

struct section {
    std::size_t virt_addr;
    std::vector<char> data;
    std::size_t alignment;

    std::vector<std::string> symbols;
    std::vector<vhxcc::relocation> relocations;
};

class object {
private:
    std::unordered_map<std::string, vhxcc::section> _sections;
    std::unordered_map<std::string, symbol> _symbols;
    std::string _platform = "none";
    std::string _arch = "vhx32i";
    std::string _entry = "";

public:
    object() = default;

    object(std::istream&);
    
    object& write(std::ostream&);
    object& write(std::ostream&, bool is_64, bool big_endian);
    object& read(std::istream&);

    object& operator<<(std::istream&);
    std::ostream& operator>>(std::ostream&);

    std::string platform() const { return _platform; }
    std::string arch() const { return _arch; }
    std::string entry() const { return _entry; }

    std::string platform(const std::string& platform) { auto t = _platform; _platform = platform; return t; }
    std::string arch(const std::string& arch) { auto t = _arch; _arch = arch; return t; }
    std::string entry(const std::string& entry) { auto t = _entry; _entry = entry; return t; }

    vhxcc::section& section(const std::string& name, std::size_t alignment) {
        if (_sections.contains(name)) {
            vhxcc::section& old = _sections.at(name);

            // 1. Strictest alignment wins for the combined section
            old.alignment = std::max(old.alignment, alignment);

            // 2. Pad old.data so the incoming section starts at its required alignment boundary
            std::size_t incoming_alignment = alignment;
            std::size_t remainder = old.data.size() % incoming_alignment;
            if (remainder != 0) {
                std::size_t padding = incoming_alignment - remainder;
                old.data.insert(old.data.end(), padding, 0);
            }

            return old;
        }
        vhxcc::section section;
        section.alignment = alignment;
        _sections.emplace(name, std::move(section));
        return _sections.at(name);
    }

    vhxcc::section& section(const std::string& name) {
        if (_sections.contains(name)) {
            return _sections.at(name);
        }
        vhxcc::section section;
        section.alignment = 16; // default alignment
        _sections.emplace(name, std::move(section));
        return _sections.at(name);
    }

    const vhxcc::symbol& define(const std::string& name, bool global = false) {
        vhxcc::symbol symbol;
        symbol.is_global = global;
        symbol.is_defined = false;
        symbol.is_absolute = false;
        symbol.section = "";
        symbol.value = 0;
        _symbols.emplace(name, std::move(symbol));
        return _symbols.at(name);
    }

    const vhxcc::symbol& define(const std::string& name, std::size_t value, bool global = false) {
        vhxcc::symbol symbol;
        symbol.is_global = global;
        symbol.is_defined = true;
        symbol.is_absolute = true;
        symbol.section = "";
        symbol.value = value;
        _symbols.emplace(name, std::move(symbol));
        return _symbols.at(name);
    }

    const vhxcc::symbol& define(const std::string& section_name, const std::string& name, bool global = false) {
        const vhxcc::section& section = this->section(section_name);
        vhxcc::symbol symbol;
        symbol.is_global = global;
        symbol.is_defined = false;
        symbol.is_absolute = false;
        symbol.section = section_name;
        symbol.value = section.data.size();
        _symbols.emplace(name, std::move(symbol));
        return _symbols.at(name);
    }

    const vhxcc::symbol& define(const std::string& section_name, const std::string& name, std::size_t offset, bool global = false) {
        const vhxcc::section& section = this->section(section_name);
        (void)section;
        vhxcc::symbol symbol;
        symbol.is_global = global;
        symbol.is_defined = true;
        symbol.is_absolute = false;
        symbol.section = section_name;
        symbol.value = offset;
        _symbols.emplace(name, std::move(symbol));
        return _symbols.at(name);
    }

    vhxcc::symbol strip(const std::string& name) {
        if (_symbols.contains(name)) {
            vhxcc::symbol symbol = std::move(_symbols.at(name));
            _symbols.erase(name);
            return symbol;
        }
        throw std::invalid_argument("symbol not found");
    }

    bool global(const std::string& name) {
        if (_symbols.contains(name)) {
            vhxcc::symbol& symbol = _symbols.at(name);
            bool prev = symbol.is_global;
            symbol.is_global = true;
            return prev;
        }
        return false;
    }

    const vhxcc::symbol& require(const std::string& name) {
        if (_symbols.contains(name))
            return _symbols.at(name);
        vhxcc::symbol symbol;
        symbol.is_global = true;
        symbol.is_defined = false;
        symbol.is_absolute = false;
        symbol.section = "";
        symbol.value = 0;
        _symbols.emplace(name, std::move(symbol));
        return _symbols.at(name);
    }

    const vhxcc::relocation& relocation(const std::string& section_name, const std::string& symbol_name, enum vhxcc::relocation::type type) {
        vhxcc::section& section = this->section(section_name);
        vhxcc::relocation relocation;
        relocation.type = type;
        relocation.symbol = symbol_name;
        relocation.offset = section.data.size();
        relocation.addend = 0;
        section.relocations.emplace_back(std::move(relocation));
        return section.relocations.back();
    }

    const vhxcc::relocation& relocation(const std::string& section_name, const std::string& symbol_name, enum vhxcc::relocation::type type, std::size_t offset, std::size_t addend = 0) {
        vhxcc::section& section = this->section(section_name);
        vhxcc::relocation relocation;
        relocation.type = type;
        relocation.symbol = symbol_name;
        relocation.offset = offset;
        relocation.addend = addend;
        section.relocations.emplace_back(std::move(relocation));
        return section.relocations.back();
    }

    object& append(const std::string& section_name, const std::vector<char>& data) {
        vhxcc::section& old = this->section(section_name);
        old.data.insert(old.data.end(), data.cbegin(), data.cend());
        return *this;
    }

    object& append(const std::string& section_name, const std::vector<char>& data, std::size_t alignment) {
        vhxcc::section& old = this->section(section_name, alignment);
        old.data.insert(old.data.end(), data.cbegin(), data.cend());
        return *this;
    }

    object& append(const std::string& section_name, const std::string& data) {
        vhxcc::section& old = this->section(section_name);
        old.data.insert(old.data.end(), data.cbegin(), data.cend());
        return *this;
    }

    object& append(const std::string& section_name, const std::string& data, std::size_t alignment) {
        vhxcc::section& old = this->section(section_name, alignment);
        old.data.insert(old.data.end(), data.cbegin(), data.cend());
        return *this;
    }

    object& operator<<(const vhxcc::object& object) {
        if (_entry == "")
            _entry = object._entry;
        // copy all sectionless symbols

        for (const auto& elem : object._symbols) {
            const std::string& symbol_name = elem.first;
            const vhxcc::symbol& symbol = elem.second;
            if (symbol.section == "") {
                _symbols.emplace(symbol_name, symbol);
            }
        }

        // copy all sections

        for (const auto& elem : object._sections) {
            const std::string& section_name = elem.first;
            const vhxcc::section& section = elem.second;
            if (!_sections.contains(section_name)) {
                // simple copy for new sections
                _sections.emplace(section_name, section);
                for (const std::string& symbol_name : section.symbols) {
                    if (object._symbols.contains(symbol_name)) {
                        _symbols.emplace(symbol_name, object._symbols.at(symbol_name));
                    }
                }
            } else {
                // Inside the section merging branch of operator<<:

                vhxcc::section& old = _sections.at(section_name);

                // 1. Strictest alignment wins for the combined section
                old.alignment = std::max(old.alignment, section.alignment);

                // 2. Pad old.data so the incoming section starts at its required alignment boundary
                std::size_t incoming_alignment = section.alignment;
                std::size_t remainder = old.data.size() % incoming_alignment;
                if (remainder != 0) {
                    std::size_t padding = incoming_alignment - remainder;
                    old.data.insert(old.data.end(), padding, 0);
                }

                // 3. Record the exact byte offset where the incoming section begins
                std::size_t offset = old.data.size();

                // 4. Append the raw bytes
                old.data.insert(old.data.end(), section.data.cbegin(), section.data.cend());

                // 5. Shift and copy symbols belonging to this section
                for (const std::string& symbol_name : section.symbols) {
                    if (object._symbols.contains(symbol_name)) {
                        const vhxcc::symbol& _symbol = object._symbols.at(symbol_name);
                        vhxcc::symbol symbol = _symbol;
                        if (symbol.is_defined && !symbol.is_absolute) {
                            symbol.value += offset; // Shift symbol by the alignment-adjusted offset
                        }
                        _symbols.emplace(symbol_name, symbol);
                    }
                    old.symbols.emplace_back(symbol_name);
                }

                // 6. Shift and copy relocations
                for (const vhxcc::relocation& _reloc : section.relocations) {
                    vhxcc::relocation reloc = _reloc;
                    reloc.offset += offset; // Shift relocation offset by the same amount
                    old.relocations.emplace_back(reloc);
                }
            }
        }
        return *this;
    }

    const std::unordered_map<std::string, vhxcc::section>& sections() const {
        return _sections;
    }

    const std::unordered_map<std::string, vhxcc::symbol>& symbols() const {
        return _symbols;
    }
};

}
