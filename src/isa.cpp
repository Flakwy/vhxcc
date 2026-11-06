
#include "object.hpp"
#include <functional>
#include <isa.hpp>
#include <string>
#include <unordered_map>
#include <vector>

using namespace vhxcc;

// A simple compile-time hash function (FNV-1a 32-bit hash)
constexpr unsigned int strhash(std::string_view str) {
    unsigned int hash = 2166136261u;
    for (char c : str) {
        hash ^= static_cast<unsigned int>(c);
        hash *= 16777619u;
    }
    return hash;
}

static const std::unordered_map<std::string, int> register_names = {
    // general registers
    {"x0", 0}, {"zero", 0},
    {"x1", 1}, {"ra", 1},
    {"x2", 2}, {"sp", 2},
    {"x3", 3}, {"gp", 3},
    {"x4", 4}, {"tp", 4},
    {"x5", 5}, {"a0", 5},
    {"x6", 6}, {"a1", 6},
    {"x7", 7}, {"a2", 7},
    {"x8", 8}, {"a3", 8},
    {"x9", 9}, {"a4", 9},
    {"x10", 10}, {"a5", 10},
    {"x11", 11}, {"a6", 11},
    {"x12", 12}, {"a7", 12},
    {"x13", 13}, {"t0", 13},
    {"x14", 14}, {"t1", 14},
    {"x15", 15}, {"t2", 15},
    {"x16", 16}, {"t3", 16},
    {"x17", 17}, {"t4", 17},
    {"x18", 18}, {"t5", 18},
    {"x19", 19}, {"t6", 19},
    {"x20", 20}, {"t7", 20},
    {"x21", 21}, {"s0", 21},
    {"x22", 22}, {"s1", 22},
    {"x23", 23}, {"s2", 23},
    {"x24", 24}, {"s3", 24},
    {"x25", 25}, {"s4", 25},
    {"x26", 26}, {"s5", 26},
    {"x27", 27}, {"s6", 27},
    {"x28", 28}, {"s7", 28},
    {"x29", 29}, {"s8", 29},
    {"x30", 30}, {"t8", 30}, {"lp", 30},
    {"x31", 31}, {"pc", 31},
    {"f0", 32}, {"f1", 33}, {"f2", 34}, {"f3", 35},
    {"f4", 36}, {"f5", 37}, {"f6", 38}, {"f7", 39},
    {"f8", 40}, {"f9", 41}, {"f10", 42}, {"f11", 43},
    {"f12", 44}, {"f13", 45}, {"f14", 46}, {"f15", 47},
    {"f16", 48}, {"f17", 49}, {"f18", 50}, {"f19", 51},
    {"f20", 52}, {"f21", 53}, {"f22", 54}, {"f23", 55},
    {"f24", 56}, {"f25", 57}, {"f26", 58}, {"f27", 59},
    {"f28", 60}, {"f29", 61}, {"f30", 62}, {"f31", 63},

};

static const std::unordered_map<std::string, int> machine_register_names = {
    // machine registers
    {"casr", 0x200}, {"epc", 0x201}, {"ecsr", 0x202}, {"evtp", 0x203},
    {"tbr", 0x240}, {"tscr", 0x241}, {"tctx", 0x242}, {"vatp", 0x402},
    {"iem", 0x220}, {"ipm", 0x210}, {"cidp", 0x401}, {"cpid", 0x400},
    {"hwclk", 0x410}, {"swclk", 0x412}, {"hwtmr", 0x420}, {"swtmr", 0x422}
};

// all instructions are in little endian
static std::vector<char> pack(uint64_t value, std::size_t size) {
    std::vector<char> bytes(size);
    for (std::size_t i = 0; i < size; i++) {
        bytes[i] = (value >> (i * 8)) & 0xFF;
    }
    return bytes;
}

struct operand {
    enum type {
        _none,
        _register,
        _machine_register,
        _constant,
        _symbol,
        _memory
    } type = _none;

    uint32_t index = 0;
    union {
        int64_t iconstant;
        uint64_t constant = 0;
    };
    std::string symbol = "";

    enum {
        _no_update,
        _pre_increment,
        _pre_decrement,
        _post_increment,
        _post_decrement
    } update = _no_update;

    operand() = default;

    operand(uint64_t value) {
        type = _constant;
        constant = value;
    }

    operand(std::string id) {
        if (register_names.find(id) != register_names.end()) {
            type = _register;
            index = register_names.at(id);
        } else if (machine_register_names.find(id) != machine_register_names.end()) {
            type = _machine_register;
            index = machine_register_names.at(id);
        } else {
            type = _symbol;
            constant = 0;
            symbol = id;
        }
    }

    operand(const vhxcc::parser::expression &expr) {
        if (expr.type == vhxcc::parser::expression::identifier) {
            std::string id = expr.token.lexeme();
            if (register_names.find(id) != register_names.end()) {
                type = _register;
                index = register_names.at(id);
            } else if (machine_register_names.find(id) != machine_register_names.end()) {
                type = _machine_register;
                index = machine_register_names.at(id);
            } else {
                type = _symbol;
                constant = 0;
                symbol = id;
            }
        } else if (expr.type == vhxcc::parser::expression::literal) {
            type = _constant;
            constant = std::stoull(std::string(expr.token.lexeme()), nullptr, 0);
        } else if (expr.type == vhxcc::parser::expression::binary) {
            type = _symbol;
            symbol = expr.branches[0].token.lexeme();
            if (expr.token == vhxcc::token::plus) {
                constant = std::stoll(std::string(expr.branches[1].token.lexeme()), nullptr, 0);
            } else if (expr.token == vhxcc::token::minus) {
                constant = -std::stoll(std::string(expr.branches[1].token.lexeme()), nullptr, 0);
            }
        } else if (expr.type == vhxcc::parser::expression::call) {
            type = _memory;
            constant = std::stoll(std::string(expr.branches[0].token.lexeme()), nullptr, 0);
            vhxcc::parser::expression base = expr.branches[1];
            if (base.type == vhxcc::parser::expression::identifier) {
                std::string id = base.token.lexeme();
                if (register_names.find(id) != register_names.end()) {
                    index = register_names.at(id);
                } else {
                    type = _none;
                }
            } else if (base.type == vhxcc::parser::expression::unary) {
                vhxcc::token op = base.token;
                base = base.branches[0];
                if (op == vhxcc::token::increment) {
                    update = _pre_increment;
                } else if (op == vhxcc::token::decrement) {
                    update = _pre_decrement;
                } else {
                    type = _none;
                    return;
                }
                std::string id = base.token.lexeme();
                if (register_names.find(id) != register_names.end()) {
                    index = register_names.at(id);
                } else {
                    type = _none;
                }
            } else if (base.type == vhxcc::parser::expression::binary) {
                vhxcc::token op = base.token;
                base = base.branches[0];
                if (op == vhxcc::token::increment) {
                    update = _post_increment;
                } else if (op == vhxcc::token::decrement) {
                    update = _post_decrement;
                } else {
                    type = _none;
                    return;
                }
                std::string id = base.token.lexeme();
                if (register_names.find(id) != register_names.end()) {
                    index = register_names.at(id);
                } else {
                    type = _none;
                }
            }
        }
    }
};

static size_t _reduce_operands(const std::vector<vhxcc::parser::expression>& raw, std::vector<operand*> operands) {
    for (std::size_t i = 0; i < std::min(raw.size(), operands.size()); i++) {
        *operands[i] = operand(raw[i]);
    }
    return std::min(raw.size(), operands.size());
}

template <typename ...Args>
static size_t reduce_operands(const std::vector<vhxcc::parser::expression>& raw, Args&... operands) {
    return _reduce_operands(raw, {&operands...});
}

using reloc = vhxcc::relocation;

// ==================================================================================
// queries
// ==================================================================================

static encode_result query_basic(const std::string& mneumonic) {
    encode_result res;
    res.is_valid = (
        mneumonic == "swp" || mneumonic == "add" || mneumonic == "sub" ||
        mneumonic == "or" || mneumonic == "and" || mneumonic == "xor" ||
        mneumonic == "sll" || mneumonic == "srl" || mneumonic == "sra" ||
        mneumonic == "mv" || mneumonic == "bl" || mneumonic == "nop" || mneumonic == "brlr" ||
        mneumonic == "addi" || mneumonic == "ori" || mneumonic == "andi" || mneumonic == "xori" ||
        mneumonic == "slli" || mneumonic == "srli" || mneumonic == "srai" || mneumonic == "adsi"
    );
    res.can_relax = false;
    res.relax = 0;
    res.mneumonic = mneumonic;
    res.size = 4;
    return res;
}

static encode_result query_li(const std::string& mneumonic) {
    encode_result res;
    res.is_valid = (mneumonic == "li");
    res.can_relax = true;
    res.relax = 0;
    res.mneumonic = mneumonic;
    res.size = 20; // maximum size of li instruction sequence
    return res;
}

static encode_result query_la(const std::string& mneumonic) {
    encode_result res;
    res.is_valid = mneumonic == "la" || mneumonic == "la32";
    res.can_relax = true;
    res.relax = 0;
    res.mneumonic = mneumonic;
    res.size = mneumonic == "la32" ? 12 : 20;
    return res;
}

static encode_result query_brl(const std::string& mneumonic) {
    encode_result res;
    res.is_valid = mneumonic == "brl";
    res.can_relax = false;
    res.relax = 0;
    res.mneumonic = mneumonic;
    res.size = 24;
    return res;
}

static encode_result query_memory(const std::string& mneumonic) {
    encode_result res;
    res.is_valid = mneumonic == "lb" || mneumonic == "sb" ||
                   mneumonic == "lh" || mneumonic == "sh" ||
                   mneumonic == "lw" || mneumonic == "sw" ||
                   mneumonic == "ld" || mneumonic == "sd";
    res.can_relax = false;
    res.relax = 0;
    res.mneumonic = mneumonic;
    res.size = 4;
    return res;
}

static encode_result query_conditional_move(const std::string& mneumonic) {
    static const std::unordered_set<std::string> names = {
        "mveq", "mvne", "mvlt", "mvle", "mvlo", "mvls", "mvtz", "mvnt"
    };
    encode_result res;
    res.is_valid = names.contains(mneumonic);
    res.can_relax = false;
    res.relax = 0;
    res.mneumonic = mneumonic;
    res.size = 4;
    return res;
}

static encode_result query_multiply_divide(const std::string& mneumonic) {
    encode_result res;
    res.is_valid = mneumonic == "mul" || mneumonic == "muls" ||
                   mneumonic == "mulsu" || mneumonic == "div" ||
                   mneumonic == "divs";
    res.can_relax = false;
    res.relax = 0;
    res.mneumonic = mneumonic;
    res.size = 4;
    return res;
}

static encode_result query_extend(const std::string& mneumonic) {
    static const std::unordered_set<std::string> names = {
        "zxtb", "zxth", "zxtw", "zxtd", "sxtb", "sxth", "sxtw", "sxtd"
    };
    encode_result res;
    res.is_valid = names.contains(mneumonic);
    res.can_relax = false;
    res.relax = 0;
    res.mneumonic = mneumonic;
    res.size = 4;
    return res;
}
// ==================================================================================
// encoders
// ==================================================================================

static encode_result encode_alu_rrr(const std::string &mneumonic, const std::vector<vhxcc::parser::expression> &operands, std::size_t pointer, int relax) {
    encode_result res;
    res.mneumonic = mneumonic;
    res.can_relax = false;
    res.relax = 0;
    res.size = 4;
    static constexpr uint32_t opc6 = 0x00;
    uint32_t func4 = 0x00;
    operand rd, rm, rn;
    size_t count = reduce_operands(operands, rd, rm, rn);

    res.is_valid = false;
    switch (strhash(mneumonic)) {
        case strhash("swp"):
            if (count == 3 && rd.type == operand::_register && rm.type == operand::_register && rn.type == operand::_register) {
                func4 = 0x00;
                res.is_valid = true;
            }
            break;
        case strhash("add"):
            if (count == 3 && rd.type == operand::_register && rm.type == operand::_register && rn.type == operand::_register) {
                func4 = 0x01;
                res.is_valid = true;
            }
            break;
        case strhash("mv"): // pseudo-op
            if (count == 2 && rd.type == operand::_register && rm.type == operand::_register) {
                rn = operand("zer");
                func4 = 0x01;
                res.is_valid = true;
            }
            break;
        case strhash("bl"): // pseudo-op
            if (count == 1 && rd.type == operand::_register) {
                rm = rd;
                rd = operand("pc");
                rn = operand("zer");
                func4 = 0x01;
                res.is_valid = true;
            }
            break;
        case strhash("nop"): // pseudo-op
            if (count == 0) {
                rd = operand("zer");
                rm = operand("zer");
                rn = operand("zer");
                func4 = 0x01;
                res.is_valid = true;
            }
            break;
        case strhash("brlr"): // pseudo-op
            if (count == 0) {
                rd = operand("pc");
                rm = operand("ra");
                rn = operand("zer");
                func4 = 0x01;
                res.is_valid = true;
            }
            break;
        case strhash("sub"):
            if (count == 3 && rd.type == operand::_register && rm.type == operand::_register && rn.type == operand::_register) {
                func4 = 0x02;
                res.is_valid = true;
            }
            break;
        case strhash("or"):
            if (count == 3 && rd.type == operand::_register && rm.type == operand::_register && rn.type == operand::_register) {
                func4 = 0x03;
                res.is_valid = true;
            }
            break;
        case strhash("and"):
            if (count == 3 && rd.type == operand::_register && rm.type == operand::_register && rn.type == operand::_register) {
                func4 = 0x04;
                res.is_valid = true;
            }
            break;
        case strhash("xor"):
            if (count == 3 && rd.type == operand::_register && rm.type == operand::_register && rn.type == operand::_register) {
                func4 = 0x05;
                res.is_valid = true;
            }
            break;
        case strhash("sll"):
            if (count == 3 && rd.type == operand::_register && rm.type == operand::_register && rn.type == operand::_register) {
                func4 = 0x06;
                res.is_valid = true;
            }
            break;
        case strhash("srl"):
            if (count == 3 && rd.type == operand::_register && rm.type == operand::_register && rn.type == operand::_register) {
                func4 = 0x07;
                res.is_valid = true;
            }
            break;
        case strhash("sra"):
            if (count == 3 && rd.type == operand::_register && rm.type == operand::_register && rn.type == operand::_register) {
                func4 = 0x08;
                res.is_valid = true;
            }
            break;
        default:
            break;
    }

    if (res.is_valid) {
        res.bytes = pack((
            (opc6 << 0) |
            (rd.index << 6) |
            (rm.index << 11) |
            (func4 << 16) |
            (rn.index << 20)
        ), 4);
    } else {
        res.bytes = pack(0, 4);
    }
    return res;
}

static encode_result encode_alu_rri(const std::string &mneumonic, const std::vector<vhxcc::parser::expression> &operands, std::size_t pointer, int relax) {
    encode_result res;
    res.mneumonic = mneumonic;
    res.can_relax = false;
    res.relax = 0;
    res.size = 4;
    static constexpr uint32_t opc6 = 0x00;
    uint32_t func4 = 0x00;
    operand rd, rm, imm12;
    size_t count = reduce_operands(operands, rd, rm, imm12);

    res.is_valid = false;
    switch (strhash(mneumonic)) {
        case strhash("addi"):
            if (count == 3 && rd.type == operand::_register && rm.type == operand::_register && imm12.type == operand::_constant) {
                func4 = 0x09;
                res.is_valid = true;
            }
            break;
        case strhash("ori"):
            if (count == 3 && rd.type == operand::_register && rm.type == operand::_register && imm12.type == operand::_constant) {
                func4 = 0x0a;
                res.is_valid = true;
            }
            break;
        case strhash("andi"):
            if (count == 3 && rd.type == operand::_register && rm.type == operand::_register && imm12.type == operand::_constant) {
                func4 = 0x0b;
                res.is_valid = true;
            }
            break;
        case strhash("xori"):
            if (count == 3 && rd.type == operand::_register && rm.type == operand::_register && imm12.type == operand::_constant) {
                func4 = 0x0c;
                res.is_valid = true;
            }
            break;
        case strhash("slli"):
            if (count == 3 && rd.type == operand::_register && rm.type == operand::_register && imm12.type == operand::_constant) {
                func4 = 0x0d;
                res.is_valid = true;
            }
            break;
        case strhash("srli"):
            if (count == 3 && rd.type == operand::_register && rm.type == operand::_register && imm12.type == operand::_constant) {
                func4 = 0x0e;
                res.is_valid = true;
            }
            break;
        case strhash("srai"):
            if (count == 3 && rd.type == operand::_register && rm.type == operand::_register && imm12.type == operand::_constant) {
                func4 = 0x0f;
                res.is_valid = true;
            }
            break;
        default:
            break;
    }

    if (res.is_valid) {
        res.bytes = pack((
            (opc6 << 0) |
            (rd.index << 6) |
            (rm.index << 11) |
            (func4 << 16) |
            (imm12.constant << 20)
        ), 4);
    } else {
        res.bytes = pack(0, 4);
    }
    return res;
}

static encode_result encode_adsi(const std::string &mneumonic, const std::vector<vhxcc::parser::expression> &operands, std::size_t pointer, int relax) {
    encode_result res;
    res.mneumonic = mneumonic;
    res.can_relax = false;
    res.relax = 0;
    res.size = 4;
    static constexpr uint32_t opc6 = 0x0b;
    operand rd, imm;
    size_t count = reduce_operands(operands, rd, imm);
    uint32_t imm16 = 0;
    uint32_t shimm = 0;

    res.is_valid = false;
    if (count == 2 && rd.type == operand::_register && imm.type == operand::_constant) {
        // verify immediate
        for (size_t i = 0; i < 32; i++) {
            uint32_t _imm16 = (imm.constant >> (4 * i)) & 0xffff;
            if ((static_cast<uint64_t>(_imm16) << (4 * i)) == imm.constant) {
                imm16 = _imm16;
                shimm = i;
                res.is_valid = true;
                break;
            }
        }
    }

    if (res.is_valid) {
        res.bytes = pack((
            (opc6 << 0) |
            (rd.index << 6) |
            (shimm << 11) |
            (imm16 << 16)
        ), 4);
    }
    return res;
}

static std::vector<char> make_aluop(uint32_t func4, operand rd, operand rm, operand rn_imm12) {
    uint32_t third = 0;
    if (rn_imm12.type == operand::_constant) {
        third = rn_imm12.constant;
    } else if (rn_imm12.type == operand::_register) {
        third = rn_imm12.index;
    }
    return pack((
        (0x0 << 0) |
        (rd.index << 6) |
        (rm.index << 11) |
        (func4 << 16) |
        (third << 20)
    ), 4);
}

static std::vector<char> _swp(operand rd, operand rm, operand rn) {
    return make_aluop(0x00, rd, rm, rn);
}

static std::vector<char> _add(operand rd, operand rm, operand rn) {
    return make_aluop(0x01, rd, rm, rn);
}

static std::vector<char> _mv(operand rd, operand rm) {
    return make_aluop(0x01, rd, rm, operand("zer"));
}

static std::vector<char> _addi(operand rd, operand rm, operand imm12) {
    return make_aluop(0x09, rd, rm, imm12);
}

static std::vector<char> _adsi(operand rd, operand imm, bool* is_valid) {
    uint32_t imm16 = 0;
    uint32_t shimm = 0;
    // verify immediate
    for (size_t i = 0; i < 32; i++) {
        uint32_t _imm16 = (imm.constant >> (4 * i)) & 0xffff;
        if ((static_cast<uint64_t>(_imm16) << (4 * i)) == imm.constant) {
            imm16 = _imm16;
            shimm = i;
            if (is_valid) {
                *is_valid = true;
            }
            return pack((
                (0x0b << 0) |
                (rd.index << 6) |
                (shimm << 11) |
                (imm16 << 16)
            ), 4);
        }
    }
    if (is_valid) {
        *is_valid = false;
    }
    return pack(0, 4);
}

static encode_result encode_li(const std::string &mneumonic, const std::vector<vhxcc::parser::expression> &operands, std::size_t pointer, int relax) {
    (void)pointer;
    encode_result res;
    res.mneumonic = mneumonic;
    res.can_relax = true;
    res.relax = relax;
    operand rd, val;
    size_t count = reduce_operands(operands, rd, val);

    res.is_valid = false;
    if (count == 2 && rd.type == operand::_register && val.type == operand::_constant) {
        switch (relax) {
            case 0: {
                res.bytes = _mv(rd, operand("zer"));
                res.is_valid = true;
                for (unsigned chunk = 0; chunk < 4; ++chunk) {
                    const uint64_t value = val.constant & (uint64_t{0xffff} << (chunk * 16));
                    if (value == 0)
                        continue;
                    bool valid = false;
                    const std::vector<char> encoded = _adsi(rd, operand(value), &valid);
                    if (!valid) {
                        res.is_valid = false;
                        res.bytes.clear();
                        break;
                    }
                    res.bytes.insert(res.bytes.end(), encoded.begin(), encoded.end());
                }
                res.can_relax = true;
            } break;
            case 1: {
                res.bytes = _mv(rd, operand("zer"));
                res.is_valid = true;
                for (unsigned chunk = 0; chunk < 2; ++chunk) {
                    const uint64_t value = val.constant & (uint64_t{0xffff} << (chunk * 16));
                    if (value == 0)
                        continue;
                    bool valid = false;
                    const std::vector<char> encoded = _adsi(rd, operand(value), &valid);
                    if (!valid) {
                        res.is_valid = false;
                        res.bytes.clear();
                        break;
                    }
                    res.bytes.insert(res.bytes.end(), encoded.begin(), encoded.end());
                }
                res.can_relax = true;
            }
        }
    }
    res.size = res.bytes.size();

    return res;
}

static encode_result encode_la(const std::string& mneumonic,
                               const std::vector<vhxcc::parser::expression>& operands,
                               std::size_t pointer, int relax) {
    (void)pointer;
    encode_result res;
    res.mneumonic = mneumonic;
    res.can_relax = false;
    res.relax = relax;
    operand rd, symbol;
    const std::size_t count = reduce_operands(operands, rd, symbol);
    res.is_valid = count == 2 && rd.type == operand::_register &&
                   symbol.type == operand::_symbol;
    if (!res.is_valid)
        return res;

    static constexpr enum vhxcc::relocation::type relocation_types[] = {
        relocation::vhx_abs_lo16, relocation::vhx_abs_hi16,
        relocation::vhx_abs_hlo16, relocation::vhx_abs_hhi16
    };
    const int pc_relative_level = mneumonic == "la32" ? 1 : 2;
    const int absolute_levels = pc_relative_level;
    if (relax >= 0 && relax < absolute_levels) {
        const std::size_t chunks =
            mneumonic == "la32" || relax == 1 ? 2 : 4;
        res.bytes = pack((rd.index << 6) | (1U << 16), 4);
        for (std::size_t chunk = 0; chunk < chunks; ++chunk) {
            const uint32_t word = 0x0bU | (rd.index << 6) |
                                (static_cast<uint32_t>(chunk) << 11);
            const std::size_t offset = res.bytes.size();
            const std::vector<char> encoded = pack(word, 4);
            res.bytes.insert(res.bytes.end(), encoded.begin(), encoded.end());
            res.relocations.emplace_back(relocation_types[chunk], symbol.symbol, offset);
        }
        res.can_relax = true;
    } else if (relax == pc_relative_level && rd.index == 31) {
        res.bytes = _addi(operand("pc"), operand("pc"), operand(0));
        res.size = res.bytes.size();
        res.relocations.emplace_back(relocation::vhx_pc12, symbol.symbol, 0);
    } else {
        res.is_valid = false;
        return res;
    }
    if (res.size == 0)
        res.size = res.bytes.size();
    return res;
}

static encode_result encode_brl(const std::string& mneumonic,
                                const std::vector<vhxcc::parser::expression>& operands,
                                std::size_t pointer, int relax) {
    (void)pointer;
    (void)relax;
    encode_result res;
    res.mneumonic = mneumonic;
    res.can_relax = false;
    res.relax = 0;
    res.size = 24;
    res.is_valid = false;

    operand target;
    if (reduce_operands(operands, target) != 1 || target.type != operand::_symbol)
        return res;

    parser::expression return_address{
        parser::expression::identifier,
        token(token::identifier, "ra", operands.front().token.pos()),
        {}
    };
    encode_result address = encode_la(
        "la", {return_address, operands.front()}, pointer, 0);
    if (!address.is_valid || address.bytes.size() != 20)
        return res;

    const std::vector<char> call = _swp(operand("pc"), operand("ra"), operand("pc"));
    res.bytes = std::move(address.bytes);
    res.relocations = std::move(address.relocations);
    res.bytes.insert(res.bytes.end(), call.begin(), call.end());
    res.size = res.bytes.size();
    res.is_valid = true;
    return res;
}

static encode_result encode_memory(const std::string& mneumonic,
                                   const std::vector<vhxcc::parser::expression>& operands,
                                   std::size_t pointer, int relax) {
    (void)pointer;
    (void)relax;
    encode_result res;
    res.mneumonic = mneumonic;
    res.can_relax = false;
    res.relax = 0;
    res.size = 4;

    operand rd, memory;
    const std::size_t count = reduce_operands(operands, rd, memory);
    uint32_t opcode = 0;
    uint32_t function = 0;
    if (mneumonic == "lb") opcode = 1;
    else if (mneumonic == "sb") { opcode = 1; function = 1; }
    else if (mneumonic == "lh") opcode = 2;
    else if (mneumonic == "sh") { opcode = 2; function = 1; }
    else if (mneumonic == "lw") opcode = 3;
    else if (mneumonic == "sw") { opcode = 3; function = 1; }
    else if (mneumonic == "ld") opcode = 4;
    else if (mneumonic == "sd") { opcode = 4; function = 1; }

    res.is_valid = count == 2 && rd.type == operand::_register &&
                   memory.type == operand::_memory &&
                   memory.iconstant <= 2047 && memory.iconstant >= -2048;
    if (res.is_valid) {
        const uint32_t word = opcode | (rd.index << 6) | (memory.index << 11) |
                              (function << 16) |
                              (static_cast<uint32_t>(memory.constant) << 20);
        res.bytes = pack(word, 4);
    }
    return res;
}

static encode_result encode_conditional_move(
    const std::string& mneumonic,
    const std::vector<vhxcc::parser::expression>& operands,
    std::size_t pointer, int relax) {
    (void)pointer;
    (void)relax;
    encode_result res;
    res.mneumonic = mneumonic;
    res.can_relax = false;
    res.relax = 0;
    res.size = 4;
    operand rd, rn, rl, rm;
    const std::size_t count = reduce_operands(operands, rd, rn, rl, rm);
    const std::unordered_map<std::string, uint32_t> functions = {
        {"mveq", 0}, {"mvne", 1}, {"mvlt", 2}, {"mvle", 3},
        {"mvlo", 4}, {"mvls", 5}, {"mvtz", 6}, {"mvnt", 7}
    };
    const auto found = functions.find(mneumonic);
    res.is_valid = found != functions.end() && count == 4 &&
                   rd.type == operand::_register && rn.type == operand::_register &&
                   rl.type == operand::_register && rm.type == operand::_register;
    if (res.is_valid) {
        const uint32_t word = 0x07U | (rd.index << 6) | (rn.index << 11) |
                              (found->second << 16) | (rl.index << 20) |
                              (rm.index << 25);
        res.bytes = pack(word, 4);
    }
    return res;
}

static encode_result encode_multiply_divide(
    const std::string& mneumonic,
    const std::vector<vhxcc::parser::expression>& operands,
    std::size_t pointer, int relax) {
    (void)pointer;
    (void)relax;
    encode_result res;
    res.mneumonic = mneumonic;
    res.can_relax = false;
    res.relax = 0;
    res.size = 4;
    operand rd, rm, rn, rl;
    const std::size_t count = reduce_operands(operands, rd, rm, rn, rl);
    uint32_t function = 0;
    if (mneumonic == "muls") function = 1;
    else if (mneumonic == "mulsu") function = 2;
    else if (mneumonic == "div") function = 3;
    else if (mneumonic == "divs") function = 4;
    res.is_valid = count == 4 && rd.type == operand::_register &&
                   rm.type == operand::_register && rn.type == operand::_register &&
                   rl.type == operand::_register;
    if (res.is_valid) {
        const uint32_t word = 0x0cU | (rd.index << 6) | (rm.index << 11) |
                              (function << 16) | (rn.index << 20) |
                              (rl.index << 25);
        res.bytes = pack(word, 4);
    }
    return res;
}

static encode_result encode_extend(const std::string& mneumonic,
                                   const std::vector<vhxcc::parser::expression>& operands,
                                   std::size_t pointer, int relax) {
    (void)pointer;
    (void)relax;
    encode_result res;
    res.mneumonic = mneumonic;
    res.can_relax = false;
    res.relax = 0;
    res.size = 4;
    operand rd, rm;
    const std::size_t count = reduce_operands(operands, rd, rm);
    static const std::unordered_map<std::string, uint32_t> functions = {
        {"zxtb", 0}, {"zxth", 1}, {"zxtw", 2}, {"zxtd", 3},
        {"sxtb", 4}, {"sxth", 5}, {"sxtw", 6}, {"sxtd", 7}
    };
    const auto found = functions.find(mneumonic);
    res.is_valid = found != functions.end() && count == 2 &&
                   rd.type == operand::_register && rm.type == operand::_register;
    if (res.is_valid) {
        res.bytes = pack(0x06U | (rd.index << 6) | (rm.index << 11) |
                         (found->second << 16), 4);
    }
    return res;
}

// ==================================================================================
// api
// ==================================================================================

using query_fn = std::function<encode_result(const std::string &mneumonic)>;
using encoder_fn = std::function<encode_result(const std::string &mneumonic, const std::vector<vhxcc::parser::expression> &operands, std::size_t pointer, int relax)>;


static const std::unordered_map<std::string, encoder_fn> encoder_map = {
    {"swp", encode_alu_rrr},
    {"add", encode_alu_rrr},
    {"mv", encode_alu_rrr},
    {"bl", encode_alu_rrr},
    {"brlr", encode_alu_rrr},
    {"brl", encode_brl},
    {"nop", encode_alu_rrr},
    {"sub", encode_alu_rrr},
    {"or", encode_alu_rrr},
    {"and", encode_alu_rrr},
    {"xor", encode_alu_rrr},
    {"sll", encode_alu_rrr},
    {"srl", encode_alu_rrr},
    {"sra", encode_alu_rrr},
    {"addi", encode_alu_rri},
    {"ori", encode_alu_rri},
    {"andi", encode_alu_rri},
    {"xori", encode_alu_rri},
    {"slli", encode_alu_rri},
    {"srli", encode_alu_rri},
    {"srai", encode_alu_rri},
    {"adsi", encode_adsi},
    {"li", encode_li},
    {"la", encode_la},
    {"la32", encode_la},
    {"lb", encode_memory},
    {"sb", encode_memory},
    {"lh", encode_memory},
    {"sh", encode_memory},
    {"lw", encode_memory},
    {"sw", encode_memory},
    {"ld", encode_memory},
    {"sd", encode_memory},
    {"mveq", encode_conditional_move},
    {"mvne", encode_conditional_move},
    {"mvlt", encode_conditional_move},
    {"mvle", encode_conditional_move},
    {"mvlo", encode_conditional_move},
    {"mvls", encode_conditional_move},
    {"mvtz", encode_conditional_move},
    {"mvnt", encode_conditional_move},
    {"mul", encode_multiply_divide},
    {"muls", encode_multiply_divide},
    {"mulsu", encode_multiply_divide},
    {"div", encode_multiply_divide},
    {"divs", encode_multiply_divide},
    {"zxtb", encode_extend},
    {"zxth", encode_extend},
    {"zxtw", encode_extend},
    {"zxtd", encode_extend},
    {"sxtb", encode_extend},
    {"sxth", encode_extend},
    {"sxtw", encode_extend},
    {"sxtd", encode_extend},
};

static const std::unordered_map<std::string, query_fn> query_map = {
    {"swp", query_basic},
    {"add", query_basic},
    {"mv", query_basic},
    {"bl", query_basic},
    {"brlr", query_basic},
    {"brl", query_brl},
    {"nop", query_basic},
    {"sub", query_basic},
    {"or", query_basic},
    {"and", query_basic},
    {"xor", query_basic},
    {"sll", query_basic},
    {"srl", query_basic},
    {"sra", query_basic},
    {"addi", query_basic},
    {"ori", query_basic},
    {"andi", query_basic},
    {"xori", query_basic},
    {"slli", query_basic},
    {"srli", query_basic},
    {"srai", query_basic},
    {"adsi", query_basic},
    {"li", query_li},
    {"la", query_la},
    {"la32", query_la},
    {"lb", query_memory},
    {"sb", query_memory},
    {"lh", query_memory},
    {"sh", query_memory},
    {"lw", query_memory},
    {"sw", query_memory},
    {"ld", query_memory},
    {"sd", query_memory},
    {"mveq", query_conditional_move},
    {"mvne", query_conditional_move},
    {"mvlt", query_conditional_move},
    {"mvle", query_conditional_move},
    {"mvlo", query_conditional_move},
    {"mvls", query_conditional_move},
    {"mvtz", query_conditional_move},
    {"mvnt", query_conditional_move},
    {"mul", query_multiply_divide},
    {"muls", query_multiply_divide},
    {"mulsu", query_multiply_divide},
    {"div", query_multiply_divide},
    {"divs", query_multiply_divide},
    {"zxtb", query_extend},
    {"zxth", query_extend},
    {"zxtw", query_extend},
    {"zxtd", query_extend},
    {"sxtb", query_extend},
    {"sxth", query_extend},
    {"sxtw", query_extend},
    {"sxtd", query_extend},
};

encode_result vhxcc::query_mneumonic(const std::string& mneumonic) {
    auto it = query_map.find(mneumonic);
    if (it != query_map.end()) {
        return it->second(mneumonic);
    }
    return {};
}

encode_result vhxcc::encode_mneumonic(const std::string &mneumonic, const std::vector<vhxcc::parser::expression> &operands, std::size_t pointer, int relax) {
    auto it = encoder_map.find(mneumonic);
    if (it != encoder_map.end()) {
        return it->second(mneumonic, operands, pointer, relax);
    }
    return {};
}