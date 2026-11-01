#pragma once

#include <cstddef>
#include <cstdint>
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
    };

    std::string symbol;
    std::size_t offset;
    std::size_t addend;
};

struct section {
    std::string name;
    std::vector<uint8_t> data;
    std::size_t alignment;

    std::vector<vhxcc::relocation> relocations;
};

class object {
private:
    std::unordered_map<std::string, section> _sections;
    std::unordered_map<std::string, symbol> _symbols;

public:
    object() = default;

    const section& section(const std::string& name, std::size_t alignment = 8) {
        vhxcc::section section;
        section.name = name;
        section.alignment = alignment;
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
};

}