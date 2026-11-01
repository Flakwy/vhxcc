#pragma once

#include <cstddef>
#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace vhxcc {

struct symbol {
    std::string name;
    std::string section_name;
    std::size_t value;
    bool is_global;
    bool is_defined;
    bool is_absolute;
};

struct relocation {
    enum type {
        vhx32_pc_rel12,
        vhx32_pc_rela12,
        vhx32_pc_rels12,
        vhx32_abs32,
        vhx64_abs64
    } type;

    std::string symbol;
    std::size_t offset;
    std::size_t addend;
};

struct section {
    std::string name;
    std::vector<uint8_t> data;
    std::size_t alignment;

    std::vector<std::string> symbols;
    std::vector<vhxcc::relocation> relocations;
};

class object {
private:
    std::unordered_map<std::string, section> _sections;
    std::unordered_map<std::string, symbol> _symbols;

public:
    object() = default;

    object(std::istream&);
    
    object& write(std::ostream&) const;
    object& read(std::istream&);

    object& operator<<(std::istream&);
    friend std::ostream& operator<<(std::ostream&, const object&);

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
        section.name = name;
        section.alignment = alignment;
        _sections.emplace(name, std::move(section));
        return _sections.at(name);
    }

    vhxcc::section& section(const std::string& name) {
        if (_sections.contains(name)) {
            return _sections.at(name);
        }
        vhxcc::section section;
        section.name = name;
        section.alignment = 16; // default alignment
        _sections.emplace(name, std::move(section));
        return _sections.at(name);
    }

    const vhxcc::symbol& define(const std::string& name, std::size_t value, bool global = false) {
        vhxcc::symbol symbol;
        symbol.name = name;
        symbol.is_global = global;
        symbol.is_defined = true;
        symbol.is_absolute = true;
        symbol.section_name = "";
        symbol.value = value;
        _symbols.emplace(name, std::move(symbol));
        return _symbols.at(name);
    }

    const vhxcc::symbol& define(const std::string& section_name, const std::string& name, bool global = false) {
        const vhxcc::section& section = this->section(section_name);
        vhxcc::symbol symbol;
        symbol.name = name;
        symbol.is_global = global;
        symbol.is_defined = true;
        symbol.is_absolute = false;
        symbol.section_name = section_name;
        symbol.value = section.data.size();
        _symbols.emplace(name, std::move(symbol));
        return _symbols.at(name);
    }

    const vhxcc::symbol& define(const std::string& section_name, std::size_t offset, const std::string& name, bool global = false) {
        const vhxcc::section& section = this->section(section_name);
        vhxcc::symbol symbol;
        symbol.name = name;
        symbol.is_global = global;
        symbol.is_defined = true;
        symbol.is_absolute = false;
        symbol.section_name = section_name;
        symbol.value = offset;
        _symbols.emplace(name, std::move(symbol));
        return _symbols.at(name);
    }

    const vhxcc::symbol& require(const std::string& name) {
        vhxcc::symbol symbol;
        symbol.name = name;
        symbol.is_global = true;
        symbol.is_defined = false;
        symbol.is_absolute = false;
        symbol.section_name = "";
        symbol.value = 0;
        _symbols.emplace(name, std::move(symbol));
        return _symbols.at(name);
    }

    object& append(const std::string& section_name, const std::vector<uint8_t>& data) {
        vhxcc::section& old = this->section(section_name);
        old.data.insert(old.data.end(), data.cbegin(), data.cend());
        return *this;
    }

    object& append(const std::string& section_name, const std::vector<uint8_t>& data, std::size_t alignment) {
        vhxcc::section& old = this->section(section_name, alignment);
        old.data.insert(old.data.end(), data.cbegin(), data.cend());
        return *this;
    }

    object& operator<<(const vhxcc::object& object) {
        // copy all sectionless symbols

        for (const auto& elem : object._symbols) {
            const vhxcc::symbol& symbol = elem.second;
            if (symbol.section_name == "") {
                _symbols.emplace(symbol.name, symbol);
            }
        }

        // copy all sections

        for (const auto& elem : object._sections) {
            const vhxcc::section& section = elem.second;
            if (!_sections.contains(section.name)) {
                // simple copy for new sections
                _sections.emplace(section.name, section);
                for (const std::string& symbol_name : section.symbols) {
                    if (object._symbols.contains(symbol_name)) {
                        _symbols.emplace(symbol_name, object._symbols.at(symbol_name));
                    }
                }
            } else {
                // Inside the section merging branch of operator<<:

                vhxcc::section& old = _sections.at(section.name);

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


                // keep for tracking
                // // append section onto existing section
                // vhxcc::section& old = _sections.at(section.name);
                // while ((old.data.size() + old.alignment) % section.alignment >= 1024)
                //     old.data.insert(old.data.end(), 1024, 0);
                // while ((old.data.size() + old.alignment) % section.alignment != 0)
                //     old.data.push_back(0);
                // std::size_t offset = old.data.size();
                // std::size_t remainder = offset % section.alignment;
                // if (remainder != 0)
                //     old.data.insert(old.data.end(), remainder, 0);
                // old.data.insert(old.data.end(), section.data.cbegin(), section.data.cend());
                // for (const std::string& symbol_name : section.symbols) {
                //     if (object._symbols.contains(symbol_name)) {
                //         const vhxcc::symbol& _symbol = object._symbols.at(symbol_name);
                //         vhxcc::symbol symbol = _symbol;
                //         if (symbol.is_defined && !symbol.is_absolute)
                //             symbol.value += offset;
                //         _symbols.emplace(symbol_name, symbol);
                //     }
                //     old.symbols.emplace_back(symbol_name);
                // }
                // for (const vhxcc::relocation& _reloc : section.relocations) {
                //     vhxcc::relocation reloc = _reloc;
                //     reloc.offset += offset;
                //     old.relocations.emplace_back(reloc);
                // }
            }
        }
        return *this;
    }
};

}