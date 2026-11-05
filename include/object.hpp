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
        vhx_pc12,
        vhx_pca8,
        vhx_pcs8,
        vhx_rel32,
        vhx_rel64,
        vhx_abs32,
        vhx_abs64,
        vhx_abs_lo16,
        vhx_abs_hi16,
        vhx_abs_hlo16,
        vhx_abs_hhi16
    } type;

    std::string symbol;
    std::size_t offset;
    std::size_t addend;

    relocation()
        : type(vhx_rel32), symbol(""), offset(0), addend(0) {}
    
    relocation(enum type t, std::string s, std::size_t o, std::size_t a = 0)
        : type(t), symbol(s), offset(o), addend(a) {}
};

using enum relocation::type;

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
    std::size_t _entry = 0;

public:
    object() = default;

    object(std::istream&);
    
    object& write(std::ostream&);
    object& read(std::istream&);

    object& operator<<(std::istream&);
    friend std::ostream& operator<<(std::ostream&, const object&);

    const std::string& platform() const { return _platform; }
    const std::string& arch() const { return _arch; }
    std::size_t entry() const { return _entry; }

    const std::string& platform(const std::string& platform) { _platform = platform; return _platform; }
    const std::string& arch(const std::string& arch) { _arch = arch; return _arch; }
    std::size_t entry(std::size_t entry) { _entry = entry; return _entry; }

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
