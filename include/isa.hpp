#pragma once

#include "object.hpp"
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>
#include <parser.hpp>

namespace vhxcc {

static const std::unordered_set<std::string> register_names = {
    // general registers
    "x0", "zer",
    "x1", "ra",
    "x2", "sp",
    "x3", "gp",
    "x4", "tp",
    "x5", "a0",
    "x6", "a1",
    "x7", "a2",
    "x8", "a3",
    "x9", "a4",
    "x10", "a5",
    "x11", "a6",
    "x12", "a7",
    "x13", "t0",
    "x14", "t1",
    "x15", "t2",
    "x16", "t3",
    "x17", "t4",
    "x18", "t5",
    "x19", "t6",
    "x20", "t7",
    "x21", "s0",
    "x22", "s1",
    "x23", "s2",
    "x24", "s3",
    "x25", "s4",
    "x26", "s5",
    "x27", "s6",
    "x28", "s7",
    "x29", "s8",
    "x30", "t8", "lp",
    "x31", "pc",
    "f0", "f1", "f2", "f3",
    "f4", "f5", "f6", "f7",
    "f8", "f9", "f10", "f11",
    "f12", "f13", "f14", "f15",
    "f16", "f17", "f18", "f19",
    "f20", "f21", "f22", "f23",
    "f24", "f25", "f26", "f27",
    "f28", "f29", "f30", "f31",

    // machine registers
    "casr", "epc", "ecsr", "evtp",
    "tbr", "tscr", "tctx", "vatp",
    "iem", "ipm", "cidp", "cpid",
    "hwclk", "swclk", "hwtmr", "swtmr"
};

struct encode_result {
    std::string mneumonic;
    std::vector<char> bytes;
    std::vector<vhxcc::relocation> relocations;
    std::size_t size;      // size of the instruction in bytes
    int         relax;     // relaxation level of the instruction
    bool        can_relax; // indicates if the instruction can be relaxed further
    bool        is_valid;  // indicates if the instruction is valid
};

encode_result query_mneumonic(const std::string& mneumonic, int relax = 0);
encode_result encode_mneumonic(const std::string& mneumonic, const std::vector<vhxcc::parser::expression>& operands, int relax = 0);

}