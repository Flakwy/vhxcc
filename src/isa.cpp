
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
    for (std::size_t i = 0; i < size; ++i) {
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
    uint64_t constant = 0;
    std::string symbol = "";

    enum {
        _no_update,
        _pre_increment,
        _pre_decrement,
        _post_increment,
        _post_decrement
    } update = _no_update;

    operand() = default;

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
            constant = std::stoll(std::string(expr.token.lexeme()), nullptr, 0);
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
    for (std::size_t i = 0; i < std::min(raw.size(), operands.size()); ++i) {
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

static encode_result query_alu_rrr(const std::string& mneumonic) {
    encode_result res;
    res.is_valid = (
        mneumonic == "swp" || mneumonic == "add" || mneumonic == "sub" ||
        mneumonic == "or" || mneumonic == "and" || mneumonic == "xor" ||
        mneumonic == "sll" || mneumonic == "srl" || mneumonic == "sra" ||
        mneumonic == "mv" || mneumonic == "bl" || mneumonic == "nop" || mneumonic == "brlr"
    );
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
        ), 4); // Placeholder for actual encoding
    } else {
        res.bytes = pack(0, 4);
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
    {"nop", encode_alu_rrr},
    {"sub", encode_alu_rrr},
    {"or", encode_alu_rrr},
    {"and", encode_alu_rrr},
    {"xor", encode_alu_rrr},
    {"sll", encode_alu_rrr},
    {"srl", encode_alu_rrr},
    {"sra", encode_alu_rrr}
};

static const std::unordered_map<std::string, query_fn> query_map = {
    {"swp", query_alu_rrr},
    {"add", query_alu_rrr},
    {"mv", query_alu_rrr},
    {"bl", query_alu_rrr},
    {"brlr", query_alu_rrr},
    {"nop", query_alu_rrr},
    {"sub", query_alu_rrr},
    {"or", query_alu_rrr},
    {"and", query_alu_rrr},
    {"xor", query_alu_rrr},
    {"sll", query_alu_rrr},
    {"srl", query_alu_rrr},
    {"sra", query_alu_rrr}
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