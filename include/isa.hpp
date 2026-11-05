#pragma once

#include "object.hpp"
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>
#include <parser.hpp>

namespace vhxcc {

struct encode_result {
    std::string mneumonic;
    std::vector<char> bytes;
    std::vector<vhxcc::relocation> relocations;
    std::size_t size;      // size of the instruction in bytes
    int         relax;     // relaxation level of the instruction
    bool        can_relax; // indicates if the instruction can be relaxed further
    bool        is_valid;  // indicates if the instruction is valid
};

encode_result query_mneumonic(const std::string& mneumonic);
encode_result encode_mneumonic(const std::string& mneumonic, const std::vector<vhxcc::parser::expression>& operands, std::size_t pointer, int relax = 0);

}