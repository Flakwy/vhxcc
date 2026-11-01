#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace vhxcc {

struct symbol {
    std::string name;
    std::string section;
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

    const vhxcc::symbol& define(std::string name, std::size_t value, bool global = false) {
        vhxcc::symbol symbol;
        symbol.name = name;
        symbol.is_global = global;
        symbol.is_defined = true;
        symbol.is_absolute = true;
        symbol.section = "";
        symbol.value = value;
        return (_symbols[name] = std::move(symbol));
    }
};

}