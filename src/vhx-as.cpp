
#include "error.hpp"
#include "isa.hpp"
#include "object.hpp"
#include "token.hpp"
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <lexer.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <parser.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>

using namespace vhxcc;

struct ast {
    enum type {
        none,
        directive,
        expression,
        instruction,
        label,
        incbin
    } type;

    vhxcc::position pos;
    std::string name;
    parser::expression expr;
    std::vector<ast> children;
};

static const std::string help = R"(Usage: vhx64-as [option...] [asmfile...]
Options:
  --help                  show all assembler options and exit
  -c                      compile only, do not link
  -e <entry>              specify the entry point symbol <entry>
  -I <dir>                add <dir> to search list for .include directives
  -o <output>             name the object-file output <output> (default a.out)
  --version                print assembler version and exit
  -W, --no-warn           suppress warnings
  -Werror                 treat warnings as errors
VHX options:
  -mabi=<abi name>        specify for ABI <abi name>
  -march=<arch name>      assemble for architecture <arch name>;
  -mrelax                 enable relax (default)
  -mno-relax              disable relax
  -mbig-endian            assemble for big-endian
  -mlittle-endian         assemble for little-endian
)";

#ifndef HOSTNAME
#define HOSTNAME "unknown"
#endif

#ifndef VERSION
#define VERSION ""
#endif

static const std::string version = R"(VHX assembler (vhxcc for )" HOSTNAME ") " VERSION R"(
Copyright (C) 2026 Free Software Foundation Inc.
This program is free software; you may redistribute it under the terms of
the GNU General Public License version 3 or later.
This program has absolutely no warranty.)";

struct opts {
    std::vector<std::filesystem::path> inputs;
    std::filesystem::path output = "a.out";
    std::string arch = "vhx64imsa";
    std::vector<std::pair<std::string, std::string>> defines;
    std::vector<std::filesystem::path> include_dirs;
    std::string entry = "_start";
    bool relax = true;
    bool help = false;
    bool version = false;
    bool little = true;
    bool fatal_warnings = false;
    bool no_warn = false;
} opts;

#define BOLD "\033[1m"
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"

static bool errored = false;
static std::string first_file = "";
void vhxcc::error(std::string_view msg) {
    std::cerr << std::format("{}{}: {}error:{} {}", BOLD, first_file, RED, RESET, msg) << std::endl;
    errored = true;
}
void vhxcc::error(std::string_view msg, const vhxcc::token& tok) {
    std::cerr << std::format("{}{}:{}:{}: {}error:{} {}", BOLD, tok.pos().file, tok.pos().line, tok.pos().column, RED, RESET, msg) << std::endl;
    errored = true;
}

static std::size_t warnings = 0;
void vhxcc::warn(std::string_view msg, const vhxcc::token& tok) {
    if (!opts.no_warn)
        std::cerr << std::format("{}{}:{}:{}: {}warning:{} {}", BOLD, tok.pos().file, tok.pos().line, tok.pos().column, MAGENTA, RESET, msg) << std::endl;
    warnings++;
}

// static std::string expr_to_string(enum parser::expression::type type) {
//     switch (type) {
//         case parser::expression::call: return "call";
//         case parser::expression::binary: return "binary";
//         case parser::expression::unary: return "unary";
//         case parser::expression::literal: return "literal";
//         case parser::expression::identifier: return "identifier";
//         default: return "unknown";
//     }
// }

// static std::string ast_type_str(enum ast::type type) {
//     switch (type) {
//         case ast::expression: return "expression";
//         case ast::label: return "label";
//         case ast::directive: return "directive";
//         case ast::instruction: return "instruction";
//         case ast::incbin: return "incbin";
//         default: return "unknown";
//     }
// }

// static void print_expression(const parser::expression& node, int indent = 0) {
//     std::cout << std::string(indent*2, ' ');
//     std::cout << "expression(" << expr_to_string(node.type) << "): " << node.token.lexeme() << std::endl;
//     for (const auto& child : node.branches) {
//         print_expression(child, indent + 1);
//     }
// }

// static void print_ast(const ast& node, int indent = 0) {
//     std::cout << std::string(indent*2, ' ');
//     switch (node.type) {
//         case ast::expression: {
//             // std::cout << "ast(expression): " << node.expr.token.lexeme() << std::endl;
//             print_expression(node.expr, indent);
//         } break;
//         case ast::label: std::cout << "ast(label): " << node.name << std::endl; break;
//         case ast::directive: std::cout << "ast(directive): " << node.name << std::endl; break;
//         case ast::instruction: std::cout << "ast(instruction): " << node.name << std::endl; break;
//         case ast::incbin: std::cout << "ast(incbin)" << std::endl; break;
//         default: std::cout << "unknown" << std::endl; break;
//     }
//     for (const auto& child : node.children) {
//         print_ast(child, indent + 1);
//     }
// };

static void expand_builtins(ast& node, const std::unordered_map<std::string, ast (*)(ast&)>& builtins) {
    for (auto& child : node.children) {
        expand_builtins(child, builtins);
    }
    if (node.type == ast::expression && node.expr.type == parser::expression::call) {
        parser::expression callee = node.expr.branches[0];
        if (callee.type == parser::expression::identifier) {
            std::string builtin = node.expr.token.lexeme();
            // std::cout << "builtin " << builtin << std::endl;

            if (!builtins.contains(builtin)) {
                vhxcc::error(std::format("unknown builtin `{}'", builtin), node.expr.token);
            } else {
                node = builtins.at(builtin)(node);
            }
        }
    }
};

static token reduce_string_or_char(const token& tok) {
    if (tok.type() == token::string_literal || tok.type() == token::char_literal) {
        std::string reduced = "";
        auto it = tok.lexeme().begin();
        while (it != tok.lexeme().end()) {
            char c = *it++;
            if (c == '\\') {
                if (it == tok.lexeme().end())
                    break;
                char a = *it++;
                switch (a) {
                    case 'a':
                        reduced += '\a';
                        break;
                    case 'b':
                        reduced += '\b';
                        break;
                    case 'e':
                        reduced += '\e';
                        break;
                    case 'f':
                        reduced += '\f';
                        break;
                    case 'n':
                        reduced += '\n';
                        break;
                    case 'r':
                        reduced += '\r';
                        break;
                    case 't':
                        reduced += '\t';
                        break;
                    case 'v':
                        reduced += '\v';
                        break;
                    case '\\':
                        reduced += '\\';
                        break;
                    case '\'':
                        reduced += '\'';
                        break;
                    case '\"':
                        reduced += '\"';
                        break;
                    case '?':
                        reduced += '\?';
                        break;
                    case '0':
                        reduced += '\0';
                        break;
                    case 'x': {
                        char b = 0;
                        if (it == tok.lexeme().end())
                            break;
                        a = *it++;
                        if (a >= '0' && a <= '9') {
                            b = (a - '0') & 0xf;
                        } else if (a >= 'a' && a <= 'f') {
                            b = (a - 'a' + 0xa) & 0xf;
                        } else if (a >= 'A' && a <= 'F') {
                            b = (a - 'A' + 0xA) & 0xF;
                        } else {
                            // TODO: unfinished escape warning
                            reduced += a; // add this char to match C
                        }
                        if (it == tok.lexeme().end()) {
                            // TODO: unfinished escape warning
                            reduced += b; // add current to match C
                            break;
                        }
                        a = *it++;
                        if (a >= '0' && a <= '9') {
                            b = (b << 4) | ((a - '0') & 0xf);
                            reduced += b;
                        } else if (a >= 'a' && a <= 'f') {
                            b = (b << 4) | ((a - 'a' + 0xa) & 0xf);
                            reduced += b;
                        } else if (a >= 'A' && a <= 'F') {
                            b = (b << 4) | ((a - 'A' + 0xA) & 0xF);
                            reduced += b;
                        } else {
                            // TODO: unfinished escape warning
                            reduced += b;
                            reduced += a; // add this char to match C
                        }
                    } break;
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7': {
                        char b = 0;
                        bool skip = false;
                        for (int i = 0; i < 3; i++) {
                            b = (b << 3) | ((a - '0') & 0x7);
                            if (it == tok.lexeme().end())
                                break;
                            a = *it++;
                            if (a < '0' || a > '7') {
                                reduced += b; // add current to match C
                                reduced += a; // add this char to match C
                                skip = true;
                                break;
                            }
                        }
                        if (!skip)
                            reduced += b;
                    } break;
                    default:
                        // TODO: unknown escape warning
                        reduced += a;
                        break;
                }
            } else {
                reduced += c;
            }
        }
        return token(tok.type(), reduced, tok.pos());
    }
    return tok;
}

struct constexpr_value {
    std::variant<long long, double, std::string> value;
    vhxcc::position pos;

    constexpr_value() = default;
    constexpr_value(long long v) : value(v), pos() {}
    constexpr_value(int v) : value(v), pos() {}
    constexpr_value(double v) : value(v), pos() {}
    constexpr_value(const vhxcc::token& tok) : pos(tok.pos()) {
        try {
            if (tok.type() == token::integer_literal || tok.type() == token::number)
                value = std::stoll(tok.lexeme(), nullptr, 0);
            else if (tok.type() == token::float_literal)
                value = std::stod(tok.lexeme());
            else if (tok.type() == token::char_literal) {
                long long char_value = 0;
                for (char c : reduce_string_or_char(tok).lexeme()) {
                    char_value = (char_value << 8) | static_cast<unsigned char>(c);
                }
                value = char_value;
            }
        } catch (const std::invalid_argument&) {
            throw vhxcc::compile_error("invalid numeric literal", tok);
        } catch (const std::out_of_range&) {
            throw vhxcc::compile_error("numeric literal out of range", tok);
        }
    }
    constexpr_value(const parser::expression& expr) : pos(expr.token.pos()) {
        try {
            if (expr.token.type() == token::integer_literal || expr.token.type() == token::number)
                value = std::stoll(expr.token.lexeme(), nullptr, 0);
            else if (expr.token.type() == token::float_literal)
                value = std::stod(expr.token.lexeme());
            else if (expr.token.type() == token::string_literal)
                value = expr.token.lexeme();
            else
                throw vhxcc::compile_error("expression must be constant", expr.token);
        } catch (const std::invalid_argument&) {
            throw vhxcc::compile_error("invalid numeric literal", expr.token);
        } catch (const std::out_of_range&) {
            throw vhxcc::compile_error("numeric literal out of range", expr.token);
        }
    }

    std::string type() const {
        if (is_integer())
            return "integer";
        else if (is_double())
            return "double";
        else
            return "string";
    }

    bool is_number() const {
        return is_integer() || is_double();
    }

    bool is_integer() const {
        return std::holds_alternative<long long>(value);
    }

    bool is_double() const {
        return std::holds_alternative<double>(value);
    }

    bool is_string() const {
        return std::holds_alternative<std::string>(value);
    }

    long long as_integer() const {
        if (is_double())
            return static_cast<long long>(as_double());
        return std::get<long long>(value);
    }

    double as_double() const {
        if (is_integer())
            return static_cast<double>(as_integer());
        return std::get<double>(value);
    }

    std::string as_string() const {
        if (is_integer())
            return std::to_string(as_integer());
        else if (is_double())
            return std::to_string(as_double());
        else
            return std::get<std::string>(value);
    }

    vhxcc::token as_token() const {
        if (is_integer())
            return token(vhxcc::token::integer_literal, as_string(), pos);
        else if (is_double())
            return token(vhxcc::token::float_literal, as_string(), pos);
        else
            return token(vhxcc::token::string_literal, as_string(), pos);
    }

    operator vhxcc::token() const {
        return as_token();
    }

    parser::expression as_expression() const {
        parser::expression expr;
        expr.type = parser::expression::literal;
        expr.token = as_token();
        return expr;
    }

    operator parser::expression() const {
        return as_expression();
    }

    operator bool() const {
        return *this != constexpr_value(0);
    }

    bool operator== (const constexpr_value& other) const {
        if (is_string() && other.is_string())
            return std::get<std::string>(value) == std::get<std::string>(other.value);
        else if ((is_string() && !other.is_string()) || (!is_string() && other.is_string()))
            return false;
        return as_string() == other.as_string();
    }
    bool operator!= (const constexpr_value& other) const {
        return !(*this == other);
    }
    bool operator< (const constexpr_value& other) const {
        if (is_string() && other.is_string())
            return std::get<std::string>(value) < std::get<std::string>(other.value);
        else if ((is_string() && !other.is_string()) || (!is_string() && other.is_string()))
            return false;
        if (is_double() || other.is_double())
            return as_double() < other.as_double();
        return as_integer() < other.as_integer();
    }
    bool operator<= (const constexpr_value& other) const {
        if (is_string() && other.is_string())
            return std::get<std::string>(value) <= std::get<std::string>(other.value);
        else if ((is_string() && !other.is_string()) || (!is_string() && other.is_string()))
            return false;
        if (is_double() || other.is_double())
            return as_double() <= other.as_double();
        return as_integer() <= other.as_integer();
    }
    bool operator> (const constexpr_value& other) const {
        if (is_string() && other.is_string())
            return std::get<std::string>(value) > std::get<std::string>(other.value);
        else if ((is_string() && !other.is_string()) || (!is_string() && other.is_string()))
            return false;
        if (is_double() || other.is_double())
            return as_double() > other.as_double();
        return as_integer() > other.as_integer();
    }
    bool operator>= (const constexpr_value& other) const {
        if (is_string() && other.is_string())
            return std::get<std::string>(value) >= std::get<std::string>(other.value);
        else if ((is_string() && !other.is_string()) || (!is_string() && other.is_string()))
            return false;
        if (is_double() || other.is_double())
            return as_double() >= other.as_double();
        return as_integer() >= other.as_integer();
    }
    bool operator! () const {
        return !((bool)*this);
    }
    bool operator&& (const constexpr_value& other) const {
        return (bool)*this && (bool)other;
    }
    bool operator|| (const constexpr_value& other) const {
        return (bool)*this || (bool)other;
    }
    constexpr_value operator& (const constexpr_value& other) const {
        if (is_integer() && other.is_integer())
            return constexpr_value(as_integer() & other.as_integer());
        else
            vhxcc::error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::_and, "&", pos));
    return *this;
    }
    constexpr_value operator| (const constexpr_value& other) const {
        if (is_integer() && other.is_integer())
            return constexpr_value(as_integer() | other.as_integer());
        else
            vhxcc::error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::_or, "|", pos));
    return *this;
    }
    constexpr_value operator^ (const constexpr_value& other) const {
        if (is_integer() && other.is_integer())
            return constexpr_value(as_integer() ^ other.as_integer());
        else
            vhxcc::error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::_xor, "^", pos));
    return *this;
    }
    constexpr_value operator<< (const constexpr_value& other) const {
        if (is_integer() && other.is_integer())
            return constexpr_value(as_integer() << other.as_integer());
        else
            vhxcc::error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::lshift, "<<", pos));
    return *this;
    }
    constexpr_value operator>> (const constexpr_value& other) const {
        if (is_integer() && other.is_integer())
            return constexpr_value(as_integer() >> other.as_integer());
        else
            vhxcc::error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::rshift, ">>", pos));
    return *this;
    }
    constexpr_value operator~ () const {
        if (is_integer())
            return constexpr_value(~as_integer());
        else
            vhxcc::error(std::format("invalid operand to unary expression ('{}')", type()), token(token::_not, "~", pos));
    return *this;
    }
    constexpr_value operator+ () const {
        if (is_integer())
            return constexpr_value(+as_integer());
        else if (is_double())
            return constexpr_value(+as_double());
        else
            vhxcc::error(std::format("invalid operand to unary expression ('{}')", type()), token(token::plus, "+", pos));
    return *this;
    }
    constexpr_value operator-() const {
        if (is_integer())
            return constexpr_value(-as_integer());
        else if (is_double())
            return constexpr_value(-as_double());
        else
            vhxcc::error(std::format("invalid operand to unary expression ('{}')", type()), token(token::minus, "-", pos));
    return *this;
    }
    constexpr_value operator+ (const constexpr_value& other) const {
        if (is_number() && other.is_number()) {
            if (is_double() || other.is_double())
                return constexpr_value(as_double() + other.as_double());
            return constexpr_value(as_integer() + other.as_integer());
        } else
            vhxcc::error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::plus, "+", pos));
        return *this;
    }
    constexpr_value operator-(const constexpr_value& other) const {
        if (is_number() && other.is_number()) {
            if (is_double() || other.is_double())
                return constexpr_value(as_double() - other.as_double());
            return constexpr_value(as_integer() - other.as_integer());
        } else
            vhxcc::error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::minus, "-", pos));
        return *this;
    }
    constexpr_value operator* (const constexpr_value& other) const {
        if (is_number() && other.is_number()) {
            if (is_double() || other.is_double())
                return constexpr_value(as_double() * other.as_double());
            return constexpr_value(as_integer() * other.as_integer());
        } else
            vhxcc::error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::times, "*", pos));
        return *this;
    }
    constexpr_value operator/ (const constexpr_value& other) const {
        if (is_number() && other.is_number()) {
            if (is_double() || other.is_double())
                return constexpr_value(as_double() / other.as_double());
            return constexpr_value(as_integer() / other.as_integer());
        } else
            vhxcc::error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::slash, "/", pos));
        return *this;
    }
    constexpr_value operator% (const constexpr_value& other) const {
        if (is_number() && other.is_number()) {
            if (is_double() || other.is_double())
                return constexpr_value(std::fmod(as_double(), other.as_double()));
            return constexpr_value(as_integer() % other.as_integer());
        } else
            vhxcc::error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::modulo, "%", pos));
        return *this;
    }
};

static parser::expression constexpr_fold(const parser::expression& node, const std::unordered_map<std::string, parser::expression>& constexprs) {
    parser::expression copy = node;

    // handle easy cases first
    if (copy.type == parser::expression::none)
        return parser::none;
    else if (copy.type == parser::expression::identifier) {
        if (constexprs.contains(copy.token.lexeme()))
            return constexprs.at(copy.token.lexeme());
        return parser::none;
    } else if (copy.type == parser::expression::literal) {
        return copy;
    }

    for (auto& child : copy.branches) {
        child = constexpr_fold(child, constexprs);
        if (child.type == parser::expression::none)
            return parser::none;
    }

    if (copy.type == parser::expression::unary) {
        constexpr_value rhs(copy.branches[0]);
        // Handle binary operations here
        switch (copy.token.type()) {
            case token::plus:
                return +rhs;
            case token::minus:
                return -rhs;
            case token::lnot:
                return constexpr_value(!rhs ? 1 : 0);
            default:
                break;
        }
    } else if (copy.type == parser::expression::binary) {
        constexpr_value lhs(copy.branches[0]);
        constexpr_value rhs(copy.branches[1]);
        switch (copy.token.type()) {
            case token::plus:
                return lhs + rhs;
            case token::minus:
                return lhs - rhs;
            case token::times:
                return lhs * rhs;
            case token::slash:
                return lhs / rhs;
            case token::modulo:
                return lhs % rhs;
            case token::_and:
                return lhs & rhs;
            case token::_or:
                return lhs | rhs;
            case token::_xor:
                return lhs ^ rhs;
            case token::lshift:
                return lhs << rhs;
            case token::rshift:
                return lhs >> rhs;
            case token::equal:
                return constexpr_value(lhs == rhs ? 1 : 0);
            case token::not_equal:
                return constexpr_value(lhs != rhs ? 1 : 0);
            case token::less_than:
                return constexpr_value(lhs < rhs ? 1 : 0);
            case token::greater_than:
                return constexpr_value(lhs > rhs ? 1 : 0);
            case token::less_equal:
                return constexpr_value(lhs <= rhs ? 1 : 0);
            case token::greater_equal:
                return constexpr_value(lhs >= rhs ? 1 : 0);
            case token::land:
                return constexpr_value(lhs && rhs ? 1 : 0);
            case token::lor:
                return constexpr_value(lhs || rhs ? 1 : 0);
            default:
                break;
        }
        // Handle binary operations here
    } else if (copy.type == parser::expression::ternary) {
        constexpr_value cond(copy.branches[0]);
        constexpr_value true_val(copy.branches[1]);
        constexpr_value false_val(copy.branches[2]);
        // Handle ternary operations here
        return cond ? true_val : false_val;
    }
    
    return copy;
}

int main(int argc, const char *argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-version" || arg == "--version") {
            std::cout << version << std::endl;
            return 0;
        } else if (arg == "-help" || arg == "--help") {
            std::cout << help << std::endl;
            return 0;
        } else if (arg.substr(0, 2) == "-I") {
            // Handle include directory option
            if (arg == "-I") {
                if (i + 1 < argc) {
                    std::string dir = argv[++i];
                    // Add the directory to the include search list
                    opts.include_dirs.push_back(dir);
                } else {
                    std::cerr << BOLD << "vhx-as:" << RED << " error:" << RESET << " missing path after '-I'" << std::endl;
                }
            } else {
                std::string dir = arg.substr(2);
                // Add the directory to the include search list
                opts.include_dirs.push_back(dir);
            }
        } else if (arg.substr(0, 2) == "-o") {
            // Handle output file option
            if (arg == "-o") {
                if (i + 1 < argc) {
                    std::string output = argv[++i];
                    // Set the output file
                    opts.output = output;
                } else {
                    std::cerr << BOLD << "vhx-as:" << RED << " error:" << RESET << " missing filename after '-o'" << std::endl;
                }
            } else {
                std::string output = arg.substr(2);
                // Set the output file
                opts.output = output;
            }
        } else if (arg.substr(0, 5) == "-mabi=") {
            // Handle ABI option
            std::string abi = arg.substr(5);
            // Store the ABI option in the opts structure
            opts.arch = abi;
        } else if (arg.substr(0, 6) == "-march=") {
            // Handle architecture option
            std::string arch = arg.substr(6);
            // Store the architecture option in the opts structure
            opts.arch = arch;
        } else if (arg.substr(0, 2) == "-e") {
            // Handle entry point option
            if (arg == "-e") {
                if (i + 1 < argc) {
                    std::string entry = argv[++i];
                    // Set the entry point
                    opts.entry = entry;
                } else {
                    std::cerr << BOLD << "vhx-as:" << RED << " error:" << RESET << " missing entry point after '-e'" << std::endl;
                }
            } else {
                std::string entry = arg.substr(2);
                // Set the entry point
                opts.entry = entry;
            }
        } else if (arg == "-mrelax" || arg == "--mrelax") {
            opts.relax = true;
        } else if (arg == "-mno-relax" || arg == "--mno-relax") {
            opts.relax = false;
        } else if (arg == "-mbig-endian") {
            opts.little = false;
        } else if (arg == "-mlittle-endian") {
            opts.little = true;
        } else if (arg == "--fatal-warnings") {
            opts.fatal_warnings = true;
        } else if (arg == "--no-warn") {
            opts.no_warn = true;
        } else if (arg == "--warn") {
            opts.no_warn = false;
        } else if (arg[0] == '-') {
            std::cerr << BOLD << "vhx-as:" << RED << " error:" << RESET << " unknown option '" << arg << "'" << std::endl;
        } else {
            // Handle input file
            if (first_file.empty()) {
                first_file = arg;
            }
            opts.inputs.push_back(arg);
        }
    }

    if (opts.inputs.empty()) {
        std::cerr << BOLD << "vhx-as:" << RED << " fatal error:" << RESET << " no input files" << std::endl;
        return 1;
    }

    std::vector<std::filesystem::path> input_files;
    for (const auto &input : opts.inputs) {
        input_files.push_back(input);
    }

    vhxcc::source source;
    for (const auto &file : input_files) {
        source.append(file);
    }

    vhxcc::lexer lexer(source);

    std::unordered_map<std::string, ast (*)(const vhxcc::token&, vhxcc::lexer::iterator&)> directives;

    auto simple_data = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        // Process the directive arguments here
        do {
            ast child;
            child.type = ast::type::expression;
            parser::expression expr = parser::expect_expression(it, true);
            child.expr = expr;
            node.children.push_back(child);
        } while (lexer::match(it, vhxcc::token::comma));
        return node;
    };

    directives[".byte"] = simple_data;
    directives[".half"] = simple_data;
    directives[".hword"] = simple_data;
    directives[".word"] = simple_data;
    directives[".dword"] = simple_data;
    directives[".qword"] = simple_data;
    directives[".quad"] = simple_data;
    directives[".ascii"] = simple_data;
    directives[".asciz"] = simple_data;
    directives[".string"] = simple_data;
    directives[".short"] = simple_data;
    directives[".int"] = simple_data;
    directives[".long"] = simple_data;
    directives[".float"] = simple_data;
    directives[".double"] = simple_data;

    auto one_argument = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        ast child;
        child.type = ast::type::expression;
        child.expr = parser::expect_expression(it, true);
        node.children.push_back(child);
        return node;
    };

    directives[".org"] = one_argument;
    directives[".align"] = one_argument;
    directives[".balign"] = one_argument;
    directives[".offset"] = one_argument;
    directives[".if"] = one_argument;

    auto no_argument = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        (void)it;
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        return node;
    };

    directives[".else"] = no_argument;
    directives[".endif"] = no_argument;
    directives[".err"] = no_argument;
    directives[".text"] = no_argument;
    directives[".data"] = no_argument;
    directives[".bss"] = no_argument;

    auto one_string_argument = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        ast child;
        child.type = ast::type::expression;
        child.expr.type = parser::expression::literal;
        child.expr.token = lexer::expect(it, vhxcc::token::string_literal);
        node.children.push_back(child);
        return node;
    };

    directives[".include"] = one_string_argument;
    directives[".incbin"] = one_string_argument;
    directives[".file"] = one_string_argument;

    auto one_number_argument = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        ast child;
        child.type = ast::type::expression;
        vhxcc::token tok = lexer::expect(it, vhxcc::token::integer_literal);
        child.name = tok.lexeme();
        parser::expression expr;
        expr.type = parser::expression::literal;
        expr.token = tok;
        child.expr = expr;
        node.children.push_back(child);
        return node;
    };

    directives[".ln"] = one_number_argument;
    directives[".line"] = one_number_argument;

    auto one_identifier_argument = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        ast child;
        child.type = ast::type::expression;
        child.expr = parser::expect_identifier(it, true);
        node.children.push_back(child);
        return node;
    };

    directives[".section"] = one_identifier_argument;
    directives[".global"] = one_identifier_argument;
    directives[".globl"] = one_identifier_argument;
    directives[".extern"] = one_identifier_argument;
    directives[".ifdef"] = one_identifier_argument;
    directives[".ifndef"] = one_identifier_argument;
    // for GNU as compatibility
    directives[".ifnotdef"] = one_identifier_argument;

    auto one_identifier_and_expression_argument = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        ast label_child;
        label_child.type = ast::type::expression;
        label_child.expr = parser::expect_identifier(it, true);
        node.children.push_back(label_child);
        lexer::expect(it, vhxcc::token::comma);
        ast child;
        child.type = ast::type::expression;
        child.expr = parser::expect_expression(it, true);
        node.children.push_back(child);
        return node;
    };

    directives[".set"] = one_identifier_and_expression_argument;
    directives[".equ"] = one_identifier_and_expression_argument;
    directives[".equiv"] = one_identifier_and_expression_argument;

    auto space_directive = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        ast child;
        child.type = ast::type::expression;
        child.expr = parser::expect_expression(it, true);
        node.children.push_back(child);
        if (lexer::match(it, vhxcc::token::comma)) {
            ast child2;
            child2.type = ast::type::expression;
            child2.expr = parser::expect_expression(it, true);
            node.children.push_back(child2);
        }
        return node;
    };

    directives[".space"] = space_directive;
    directives[".skip"] = space_directive;

    directives[".fill"] = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        ast repeat_child;
        repeat_child.type = ast::type::expression;
        repeat_child.expr = parser::expect_expression(it, true);
        node.children.push_back(repeat_child);
        if (lexer::match(it, vhxcc::token::comma)) {
            ast size_child;
            size_child.type = ast::type::expression;
            size_child.expr = parser::expect_expression(it, true);
            node.children.push_back(size_child);
            if (lexer::match(it, vhxcc::token::comma)) {
                ast value_child;
                value_child.type = ast::type::expression;
                value_child.expr = parser::expect_expression(it, true);
                node.children.push_back(value_child);
            }
        }
        return node;
    };

    directives[".arch"] = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        if (vhxcc::token tok = lexer::match(it, vhxcc::token::identifier)) {
            if (tok != "push" && tok != "pop") {
                vhxcc::error(std::format("unknown directive: arch '{}'", tok.lexeme()), tok);
                return {ast::none, tok.pos(), "", {}, {}};
            }
            ast arch_child;
            arch_child.type = ast::type::label;
            arch_child.name = tok.lexeme();
            node.children.push_back(arch_child);
        } else {
            ast arch_child;
            arch_child.type = ast::type::expression;
            arch_child.expr = parser::expect_expression(it, true);
            node.children.push_back(arch_child);
        }
        return node;
    };

    std::string current_label;

    try {

        std::vector<std::unique_ptr<vhxcc::source>> include_sources;
        std::vector<std::unique_ptr<vhxcc::lexer>> include_lexers;
    
        std::vector<ast> asts;
        auto parse_assembly_name = [](vhxcc::lexer::iterator& it) {
            parser::expression name_expr;
            vhxcc::lexer::iterator start = it;
            token first = lexer::match(it, token::dot, token::identifier);
            if (!first)
                return name_expr;

            const position start_pos = first.pos();
            std::string name;
            token previous = first;
            if (first == token::dot) {
                token part = lexer::match(it, token::identifier);
                if (!part || part.pos().line != first.pos().line ||
                    part.pos().column != first.pos().column + first.lexeme().size()) {
                    it = start;
                    return name_expr;
                }
                name = "." + part.lexeme();
                previous = part;
            } else {
                name = first.lexeme();
            }

            while (true) {
                vhxcc::lexer::iterator before_dot = it;
                token dot = lexer::match(it, token::dot);
                if (!dot || dot.pos().line != previous.pos().line ||
                    dot.pos().column != previous.pos().column + previous.lexeme().size()) {
                    it = before_dot;
                    break;
                }
                token part = lexer::match(it, token::identifier);
                if (!part || part.pos().line != dot.pos().line ||
                    part.pos().column != dot.pos().column + dot.lexeme().size()) {
                    it = before_dot;
                    break;
                }
                name += dot.lexeme() + part.lexeme();
                previous = part;
            }

            name_expr.type = parser::expression::identifier;
            name_expr.token = token(token::identifier, name, start_pos);
            return name_expr;
        };

        // Parse the source code into an abstract syntax tree (AST)
        vhxcc::lexer::iterator it = lexer.begin();
        while (it != lexer.end()) {
            // Process each token from the lexer

            while (lexer::match(it, vhxcc::token::semicolon))
                ;

            parser::expression expr = parse_assembly_name(it);
            if (expr.type == parser::expression::identifier) {
                if (lexer::match(it, vhxcc::token::colon)) {
                    // label
                    std::string label = expr.token.lexeme();
                    ast ast;
                    ast.name = label;
                    ast.type = ast::type::label;
                    ast.pos = expr.token.pos();
                    asts.push_back(ast);
                    continue;
                } else if (directives.contains(expr.token.lexeme())) {
                    // directive
                    ast ast;
                    ast.name = expr.token.lexeme();
                    ast.type = ast::type::directive;
                    ast = directives[ast.name](expr.token, it);
                    if (ast.name == ".include") {
                        std::filesystem::path filename = ast.children.front().expr.token.lexeme();
                        vhxcc::source& source = *include_sources.emplace_back(std::make_unique<vhxcc::source>(filename));
                        vhxcc::lexer& lexer = *include_lexers.emplace_back(std::make_unique<vhxcc::lexer>(source));
                        // Recursively parse the included file
                        it = lexer::iterator(lexer.begin(), it);
                    } else {
                        ast.pos = expr.token.pos();
                        asts.push_back(ast);
                    }
                    continue;
                } else if (vhxcc::token tok2 = lexer::match(it, vhxcc::token::assign)) {
                    // assignment to a label or symbol
                    ast node;
                    node.type = ast::type::directive;
                    node.name = "equ";
                    ast label;
                    label.type = ast::type::label;
                    label.name = expr.token.lexeme();
                    node.children.push_back(std::move(label));
                    ast child;
                    child.type = ast::type::expression;
                    child.expr = parser::expect_expression(it, true);
                    node.children.push_back(std::move(child));
                    node.pos = expr.token.pos();
                    asts.push_back(node);
                    continue;
                } else {
                    // instruction
                    ast node;
                    node.name = expr.token.lexeme();
                    node.type = ast::type::instruction;
                    node.pos = expr.token.pos();
                    while (it->pos().line == node.pos.line) {
                        parser::expression operand = parser::parse_expression(it, true);
                        if (operand.type == parser::expression::none)
                            break;
                        ast child;
                        child.type = ast::type::expression;
                        child.expr = std::move(operand);
                        node.children.push_back(std::move(child));
                        if (!lexer::match(it, vhxcc::token::comma))
                            break;
                    }
                    asts.push_back(node);
                    continue;
                }
            }

            // error matches GNU assembler error format
            
            vhxcc::error(std::format("junk at end of line, first unrecognized token: `{}'", it->lexeme()), *it++);
        }

        // for (const auto& ast : asts) {
        //     print_ast(ast, 0);
        // }

        std::unordered_map<std::string, ast (*)(ast&)> builtins;

        static auto make_unary [[maybe_unused]]  = [](enum token::type op, const parser::expression& operand) {
            parser::expression expr;
            expr.type = parser::expression::unary;
            expr.token = token(op);
            expr.branches.push_back(operand);
            return expr;
        };

        static auto make_binary [[maybe_unused]] = [](const parser::expression& lhs, enum token::type op, const parser::expression& rhs) {
            parser::expression expr;
            expr.type = parser::expression::binary;
            expr.token = token(op);
            expr.branches.push_back(lhs);
            expr.branches.push_back(rhs);
            return expr;
        };

        static auto make_literal [[maybe_unused]] = [](long x) {
            parser::expression expr;
            expr.type = parser::expression::literal;
            expr.token = token(token::integer_literal, std::to_string(x));
            return expr;
        };

        static auto make_pointer [[maybe_unused]] = []() {
            parser::expression expr;
            expr.type = parser::expression::identifier;
            expr.token = token(token::pointer, "$");
            return expr;
        };

        static auto make_identifier [[maybe_unused]] = [](const std::string& id) {
            parser::expression expr;
            expr.type = parser::expression::identifier;
            expr.token = token(token::pointer, id);
            return expr;
        };

        builtins["hi"] = [](ast& node) -> ast {
            // hi as well!
            if (node.expr.branches.size() < 1)
                throw vhxcc::compile_error("expected expression", node.expr.token);
            ast result;
            result.type = ast::expression;
            result.expr = make_binary(make_binary(node.expr.branches[1], token::rshift, make_literal(16)), token::_and, make_literal(0xffff));
            return result;
        };

        builtins["lo"] = [](ast& node) -> ast {
            // hel-lo!
            if (node.children.size() < 1)
                throw vhxcc::compile_error("expected expression", node.expr.token);
            ast result;
            result.type = ast::expression;
            result.expr = make_binary(node.children[0].expr, token::_and, make_literal(0xffff));
            return result;
        };

        builtins["lo12"] = [](ast& node) -> ast {
            // hel-lo!
            if (node.children.size() < 1)
                throw vhxcc::compile_error("expected expression", node.expr.token);
            ast result;
            result.type = ast::expression;
            result.expr = make_binary(node.children[0].expr, token::_and, make_literal(0xfff));
            return result;
        };

        builtins["pc_rel"] = [](ast& node) -> ast {
            if (node.children.size() < 1)
                throw vhxcc::compile_error("expected expression", node.expr.token);
            ast result;
            result.type = ast::expression;
            result.expr = make_binary(node.children[0].expr, token::minus, make_pointer());
            return result;
        };

        std::string section = ".text";
        std::unordered_map<std::string, std::vector<ast>> sections_asts;
        std::unordered_set<std::string> defined;

        // Track state for nested conditional assembly blocks
        struct if_block_state {
            bool active; // Is code currently being emitted in this block?
            bool taken;  // Has a branch in this if/else chain already evaluated true?
        };
        std::vector<if_block_state> cond_stack = {{true, true}};
        std::unordered_map<std::string, parser::expression> constexprs;

        for (auto& ast : asts) {
            expand_builtins(ast, builtins);

            bool parent_active = (cond_stack.size() > 1) ? cond_stack[cond_stack.size() - 2].active : true;

            if (ast.type == ast::directive) {
                if (ast.name == ".ifdef") {
                    bool cond = defined.contains(ast.children.front().expr.token.lexeme());
                    cond_stack.push_back({parent_active && cond, cond});
                    continue; // Do not emit the directive itself into sections
                }
                else if (ast.name == ".ifndef" || ast.name == ".ifnotdef") {
                    bool cond = !defined.contains(ast.children.front().expr.token.lexeme());
                    cond_stack.push_back({parent_active && cond, cond});
                    continue;
                }
                else if (ast.name == ".if") {
                    parser::expression folded = constexpr_fold(ast.children.front().expr, constexprs);
                    bool cond = (bool)constexpr_value(folded);
                    cond_stack.push_back({parent_active && cond, cond});
                    continue;
                }
                else if (ast.name == ".else") {
                    if (cond_stack.size() <= 1) {
                        vhxcc::error(".else without matching .if", token(token::identifier, ast.name, ast.pos));
                        continue;
                    }
                    
                    auto& current = cond_stack.back();
                    // If the 'if' branch was NOT taken, activate the else branch
                    current.active = parent_active && !current.taken;
                    current.taken = true; // Mark chain as fully exhausted
                    continue;
                }
                else if (ast.name == ".endif") {
                    if (cond_stack.size() <= 1) {
                        vhxcc::error("mismatched .endif", token(token::identifier, ast.name, ast.pos));
                    }
                    cond_stack.pop_back();
                    continue;
                }
            }

            // Only process nodes if the current conditional scope allows emission
            if (cond_stack.back().active) {
                if (ast.type == ast::directive && ast.name == ".err") {
                    vhxcc::error(".err encountered", token(token::identifier, ast.name, ast.pos));
                } else if (ast.type == ast::directive && ast.name == ".section") {
                    std::string section_name = ast.children.front().expr.token.lexeme();
                    if (section_name.empty()) {
                        vhxcc::error("empty section name", token(token::identifier, ast.name, ast.pos));
                    } else {
                        section = section_name;
                    }
                } else if (ast.type == ast::directive &&
                           (ast.name == ".text" || ast.name == ".data" || ast.name == ".bss")) {
                    section = ast.name;
                } else if (ast.type == ast::directive && (ast.name == ".set" || ast.name == ".equ" || ast.name == ".equiv")) {
                    std::string symbol_name = ast.children.front().expr.token.lexeme();
                    if (ast.name == ".equiv" && defined.contains(symbol_name)) {
                        vhxcc::error(std::format("symbol `{}' is already defined", symbol_name), token(token::identifier, ast.name, ast.pos));
                    } else {
                        defined.insert(symbol_name);
                        parser::expression folded = constexpr_fold(ast.children[1].expr, constexprs);
                        if (folded.type != parser::expression::none) {
                            constexpr_value folded_value = folded;
                            constexprs[symbol_name] = folded;
                        }
                        sections_asts[section].push_back(ast);
                    }
                } else if (ast.type == ast::label) {
                    defined.insert(ast.name);
                    sections_asts[section].push_back(ast);
                } else {
                    sections_asts[section].push_back(ast);
                }
            }
        }

        if (cond_stack.size() > 1) {
            vhxcc::error("unclosed conditional block (missing .endif)", lexer::none);
        }

        struct binary_node {
            enum type {
                opcode,
                binary,
                label,
                align,
                org
            } type;

            vhxcc::position source_pos;
            std::string name;
            std::size_t size;
            encode_result encoding;
            std::vector<parser::expression> operands;
        };

        struct section_final {
            std::vector<binary_node> nodes;
            std::size_t pointer;
        };

        struct label_info {
            std::string section;
            std::size_t offset;
        };

        std::unordered_map<std::string, std::vector<std::pair<std::string, std::size_t>>> label_references;
        std::unordered_map<std::string, section_final> sections_final;
        std::unordered_map<std::string, label_info> labels_info;

        auto fold_and_validate = [&](this auto& self, const parser::expression& expr, std::vector<label_info> cur_labels = {}) -> parser::expression {
            // 1. Helper to represent a flattened linear expression: constant + sum(coeff * symbol)
            struct evaluated_expr {
                bool valid = true;
                int64_t constant = 0;
                std::unordered_map<std::string, int64_t> symbols; // symbol -> net coefficient
            };

            // Recursive flattener that distributes multipliers (handling unary/binary +/- and constant multiplication)
            std::function<evaluated_expr(const parser::expression&, int64_t)> flatten = [&](const parser::expression& e, int64_t mult) -> evaluated_expr {
                evaluated_expr res;
                
                // Reject string literals immediately
                // (Assuming parser::expression has a string type or token check)
                if (e.type == parser::expression::literal && e.token.type() == token::string_literal) {
                    res.valid = false;
                    return res;
                }

                if (e.type == parser::expression::literal) {
                    // Extract literal value (adjust based on your token/expression value member)
                    int64_t val = 0;
                    if (!e.token.lexeme().empty()) {
                        if (e.token.type() == token::char_literal) {
                            val = 0;
                            const std::string characters = reduce_string_or_char(e.token).lexeme();
                            for (std::size_t i = 0; i < characters.size() && i < 8; i++) {
                                val = (val << 8) | static_cast<uint8_t>(characters[i]);
                            }
                        } else {
                            try {
                                val = std::stoll(std::string(e.token.lexeme()), nullptr, 0);
                            } catch (const std::invalid_argument&) {
                                res.valid = false;
                                return res;
                            } catch (const std::out_of_range&) {
                                res.valid = false;
                                return res;
                            }
                        }
                    }
                    res.constant = val * mult;
                    return res;
                } 
                else if (e.type == parser::expression::identifier) {
                    res.symbols[std::string(e.token.lexeme())] += mult;
                    return res;
                } 
                else if (e.type == parser::expression::unary) {
                    if (e.token.type() == token::plus) {
                        return flatten(e.branches[0], mult);
                    } else if (e.token.type() == token::minus) {
                        return flatten(e.branches[0], -mult);
                    }
                    res.valid = false;
                    return res;
                } 
                else if (e.type == parser::expression::binary) {
                    auto op = e.token.type();
                    if (op == token::plus) {
                        auto left = flatten(e.branches[0], mult);
                        auto right = flatten(e.branches[1], mult);
                        if (!left.valid || !right.valid) { res.valid = false; return res; }
                        res.constant = left.constant + right.constant;
                        res.symbols = std::move(left.symbols);
                        for (auto& [sym, coeff] : right.symbols) {
                            res.symbols[sym] += coeff;
                        }
                        return res;
                    } 
                    else if (op == token::minus) {
                        auto left = flatten(e.branches[0], mult);
                        auto right = flatten(e.branches[1], -mult); // invert multiplier for RHS
                        if (!left.valid || !right.valid) { res.valid = false; return res; }
                        res.constant = left.constant + right.constant;
                        res.symbols = std::move(left.symbols);
                        for (auto& [sym, coeff] : right.symbols) {
                            res.symbols[sym] += coeff;
                        }
                        return res;
                    } 
                    else if (op == token::times) {
                        // Multiplication: one side MUST be a pure constant
                        auto left = flatten(e.branches[0], 1);
                        auto right = flatten(e.branches[1], 1);
                        if (!left.valid || !right.valid) { res.valid = false; return res; }

                        if (left.symbols.empty() && right.symbols.empty()) {
                            res.constant = left.constant * right.constant * mult;
                            return res;
                        } else if (left.symbols.empty()) {
                            if (left.constant == 0) return res;
                            res.constant = 0;
                            for (auto& [sym, coeff] : right.symbols) {
                                res.symbols[sym] = coeff * left.constant * mult;
                            }
                            return res;
                        } else if (right.symbols.empty()) {
                            if (right.constant == 0) return res;
                            res.constant = 0;
                            for (auto& [sym, coeff] : left.symbols) {
                                res.symbols[sym] = coeff * right.constant * mult;
                            }
                            return res;
                        } else {
                            res.valid = false;
                            return res;
                        }
                    }
                    // Division, Modulus, and Bitwise / Shift Operations (Constant-Only)
                    else if (op == token::slash || op == token::slash || op == token::modulo ||
                             op == token::_and || op == token::_or || op == token::_xor ||
                             op == token::lshift || op == token::rshift) {
                        
                        auto left = flatten(e.branches[0], 1);
                        auto right = flatten(e.branches[1], 1);
                        if (!left.valid || !right.valid) { res.valid = false; return res; }

                        // Both sides must be pure constants (no symbols allowed)
                        if (!left.symbols.empty() || !right.symbols.empty()) {
                            res.valid = false;
                            return res;
                        }

                        int64_t l_val = left.constant;
                        int64_t r_val = right.constant;
                        int64_t calc = 0;

                        if (op == token::slash || op == token::slash) {
                            if (r_val == 0) { res.valid = false; return res; } // Guard division by zero
                            calc = l_val / r_val;
                        } 
                        else if (op == token::modulo) {
                            if (r_val == 0) { res.valid = false; return res; } // Guard modulo by zero
                            calc = l_val % r_val;
                        } 
                        else if (op == token::_and) {
                            calc = l_val & r_val;
                        } 
                        else if (op == token::_or) {
                            calc = l_val | r_val;
                        } 
                        else if (op == token::_xor) {
                            calc = l_val ^ r_val;
                        } 
                        else if (op == token::lshift) {
                            if (r_val < 0 || r_val >= 64) { res.valid = false; return res; }
                            calc = l_val << r_val;
                        } 
                        else if (op == token::rshift) {
                            if (r_val < 0 || r_val >= 64) { res.valid = false; return res; }
                            calc = l_val >> r_val;
                        }

                        res.constant = calc * mult;
                        return res;
                    }
                }


                res.valid = false;
                return res;
            };

            // Handle memory operands (`call` expressions e.g. offset(base))
            if (expr.type == parser::expression::call) {
                if (expr.branches.empty()) return parser::none;

                parser::expression copy = expr;
                // Fold/validate the offset (branch 0)
                copy.branches[0] = self(expr.branches[0], cur_labels);
                if (copy.branches.size() > 1) {
                    // Base validation (branch 1)
                    parser::expression& base = copy.branches[1];
                    bool valid_base = false;
                    if (base.type == parser::expression::identifier) {
                        valid_base = true;
                    } else if (base.type == parser::expression::unary || base.type == parser::expression::binary) {
                        parser::expression& _base = base.branches[0];
                        valid_base = (base.token.type() == token::increment || base.token.type() == token::decrement) && _base.type == parser::expression::identifier;
                    }
                    if (!valid_base) return parser::none;
                }
                return copy;
            }

            // 2. Flatten standard expressions
            evaluated_expr ev = flatten(expr, 1);
            if (!ev.valid) return parser::none;

            // 3. Section-based Symbol Cancellation (`label - label` in same section)
            // Group symbols by section
            std::unordered_map<std::string, std::vector<std::pair<std::string, int64_t*>>> section_symbols;
            for (auto& [sym, coeff] : ev.symbols) {
                auto it = labels_info.find(sym);
                if (it != labels_info.end()) {
                    section_symbols[it->second.section].push_back({sym, &coeff});
                } else {
                    // Unknown/external symbol: keep track of it
                    section_symbols["__EXTERNAL__"].push_back({sym, &coeff});
                }
            }

            // Perform intra-section cancellation for paired +1 and -1 coefficients
            for (auto& [sec, syms] : section_symbols) {
                if (sec == "__EXTERNAL__") continue;

                // Find all +1 and -1 symbols in this section
                std::vector<std::string> pos_syms, neg_syms;
                for (auto& [sym, coeff_ptr] : syms) {
                    if (*coeff_ptr == 1) pos_syms.push_back(sym);
                    else if (*coeff_ptr == -1) neg_syms.push_back(sym);
                }

                // Pair them up to calculate static offset differences
                size_t pairs = std::min(pos_syms.size(), neg_syms.size());
                for (size_t i = 0; i < pairs; ++i) {
                    const std::string& p_sym = pos_syms[i];
                    const std::string& n_sym = neg_syms[i];

                    int64_t p_offset = labels_info[p_sym].offset;
                    int64_t n_offset = labels_info[n_sym].offset;

                    // Convert the label difference into a constant offset
                    ev.constant += (p_offset - n_offset);

                    // Zero out their coefficients
                    ev.symbols[p_sym] = 0;
                    ev.symbols[n_sym] = 0;
                }
            }

            // 4. Clean up zero-coefficient symbols and validate final net weights
            std::vector<std::string> active_symbols;
            for (auto& [sym, coeff] : ev.symbols) {
                if (coeff != 0) {
                    if (coeff != 1) {
                        // Net coefficient must be +1 for a valid relocatable symbol
                        return parser::none;
                    }
                    active_symbols.push_back(sym);
                }
            }
                        // Helper lambdas to cleanly construct expression nodes
            auto make_identifier_expr = [&](const std::string& name) {
                parser::expression node;
                node.type = parser::expression::identifier;
                // Construct a token for the identifier using the original source position
                node.token = token(token::identifier, name, expr.token.pos());
                return node;
            };

            auto make_literal_expr = [&](int64_t val) {
                parser::expression node;
                node.type = parser::expression::literal;
                // Construct a token for the numeric literal
                node.token = token(token::number, std::to_string(val), expr.token.pos());
                return node;
            };

            // 5. Reconstruct the clean expression form expected by encode_mnemonic:
            if (active_symbols.empty()) {
                // Pure constant -> literal (e.g., `42`)
                return make_literal_expr(ev.constant);
            } 
            else if (active_symbols.size() == 1) {
                const std::string& sym = active_symbols[0];
                if (ev.constant == 0) {
                    // Single symbol -> identifier (e.g., `.L0`)
                    return make_identifier_expr(sym);
                } 
                else if (ev.constant > 0) {
                    // Symbol + Constant -> binary `+` (e.g., `.L0 + 4`)
                    parser::expression node;
                    node.type = parser::expression::binary;
                    node.token = token(token::plus, "+", expr.token.pos());
                    node.branches.push_back(make_identifier_expr(sym));
                    node.branches.push_back(make_literal_expr(ev.constant));
                    return node;
                } 
                else {
                    // Symbol - Constant -> binary `-` (e.g., `.L0 - 4`)
                    parser::expression node;
                    node.type = parser::expression::binary;
                    node.token = token(token::minus, "-", expr.token.pos());
                    node.branches.push_back(make_identifier_expr(sym));
                    node.branches.push_back(make_literal_expr(-ev.constant)); // use absolute value
                    return node;
                }
            }

            // More than 1 uncancelled symbol remaining -> invalid expression for instruction encoding
            return parser::none;
        };

        auto itob = [&](uint64_t val, std::size_t size) -> std::vector<char> {
            std::vector<char> bytes;
            if (opts.little) {
                for (std::size_t i = 0; i < size; i++) {
                    bytes.push_back(static_cast<char>(val & 0xff));
                    val >>= 8;
                }
            } else {
                for (std::size_t i = 0; i < size; i++) {
                    bytes.push_back(static_cast<char>((val >> (8 * (size - i - 1))) & 0xff));
                }
            }
            return bytes;
        };

        auto require_absolute_integer = [&](const parser::expression& expr, std::string_view description) -> int64_t {
            parser::expression folded = constexpr_fold(expr, constexprs);
            if (folded.type == parser::expression::none)
                throw vhxcc::compile_error(description, expr.token);
            try {
                constexpr_value value(folded);
                if (!value.is_integer())
                    throw vhxcc::compile_error(description, expr.token);
                return value.as_integer();
            } catch (const std::invalid_argument&) {
                throw vhxcc::compile_error(description, expr.token);
            } catch (const std::out_of_range&) {
                throw vhxcc::compile_error(description, expr.token);
            }
        };

        auto add_data_value = [&](binary_node& bin, const parser::expression& expr, std::size_t width) {
            if (expr.type == parser::expression::literal &&
                expr.token.type() == token::string_literal) {
                if (width != 1) {
                    throw vhxcc::compile_error("string literal is not valid for this data directive", expr.token);
                }
                token value = reduce_string_or_char(expr.token);
                bin.encoding.bytes.insert(bin.encoding.bytes.end(), value.lexeme().begin(), value.lexeme().end());
                return;
            }

            parser::expression folded = fold_and_validate(expr);
            if (folded.type == parser::expression::none)
                throw vhxcc::compile_error("invalid data expression", expr.token);

            if (folded.type == parser::expression::literal) {
                int64_t value = 0;
                try {
                    value = std::stoll(folded.token.lexeme(), nullptr, 0);
                } catch (const std::invalid_argument&) {
                    throw vhxcc::compile_error("invalid integer value", expr.token);
                } catch (const std::out_of_range&) {
                    throw vhxcc::compile_error("integer value out of range", expr.token);
                }
                if (width < sizeof(value)) {
                    const unsigned bits = static_cast<unsigned>(width * 8);
                    const int64_t min_value = -(int64_t{1} << (bits - 1));
                    const uint64_t max_value = (uint64_t{1} << bits) - 1;
                    if (value < min_value || (value >= 0 && static_cast<uint64_t>(value) > max_value)) {
                        vhxcc::warn(std::format("value {} truncated to {} bytes", value, width), expr.token);
                    }
                }
                auto bytes = itob(static_cast<uint64_t>(value), width);
                bin.encoding.bytes.insert(bin.encoding.bytes.end(), bytes.begin(), bytes.end());
                return;
            }

            if ((width != 4 && width != 8) ||
                (folded.type != parser::expression::identifier &&
                 folded.type != parser::expression::binary)) {
                throw vhxcc::compile_error("data expression requires an unsupported relocation", expr.token);
            }

            std::string symbol_name;
            int64_t addend = 0;
            if (folded.type == parser::expression::identifier) {
                symbol_name = folded.token.lexeme();
            } else if (folded.branches.size() == 2 &&
                       folded.branches[0].type == parser::expression::identifier &&
                       folded.branches[1].type == parser::expression::literal &&
                       (folded.token.type() == token::plus || folded.token.type() == token::minus)) {
                symbol_name = folded.branches[0].token.lexeme();
                try {
                    addend = std::stoll(folded.branches[1].token.lexeme(), nullptr, 0);
                } catch (const std::invalid_argument&) {
                    throw vhxcc::compile_error("invalid relocation addend", expr.token);
                } catch (const std::out_of_range&) {
                    throw vhxcc::compile_error("relocation addend out of range", expr.token);
                }
                if (folded.token.type() == token::minus)
                    addend = -addend;
            } else {
                throw vhxcc::compile_error("invalid relocation expression", expr.token);
            }

            relocation reloc;
            reloc.type = width == 4 ? relocation::vhx_abs32 : relocation::vhx_abs64;
            reloc.symbol = std::move(symbol_name);
            reloc.offset = bin.encoding.bytes.size();
            reloc.addend = static_cast<std::size_t>(addend);
            bin.encoding.relocations.push_back(std::move(reloc));
            bin.encoding.bytes.resize(bin.encoding.bytes.size() + width, 0);
        };
        
        // first estimate
        for (auto& [section_name, asts] : sections_asts) {
            section_final& section = sections_final[section_name];
            for (auto& node : asts) {
                switch (node.type) {
                    case ast::directive: {
                        if (node.name == ".byte") {
                            binary_node bin;
                            bin.name = node.name;
                            bin.type = binary_node::binary;
                            bin.source_pos = node.pos;
                            bin.encoding.is_valid = true;
                            bin.encoding.can_relax = false;
                            bin.encoding.mneumonic = node.name;
                            bin.encoding.relax = 0;
                            bin.size = 0;
                            for (const ast& child : node.children) {
                                bin.operands.push_back(child.expr);
                            }
                            for (const auto& expr : bin.operands) {
                                if (expr.type == parser::expression::literal &&
                                    expr.token.type() == token::string_literal)
                                    bin.size += reduce_string_or_char(expr.token).lexeme().size();
                                else
                                    ++bin.size;
                            }
                            section.pointer += bin.size;
                            section.nodes.push_back(bin);
                        } else if (node.name == ".short" || node.name == ".half" || node.name == ".hword") {
                            binary_node bin;
                            bin.type = binary_node::binary;
                            bin.name = node.name;
                            bin.source_pos = node.pos;
                            bin.encoding.is_valid = true;
                            bin.encoding.can_relax = false;
                            bin.encoding.mneumonic = node.name;
                            bin.encoding.relax = 0;
                            bin.size = 0;
                            for (const ast& child : node.children) {
                                bin.operands.push_back(child.expr);
                            }
                            bin.size = bin.operands.size() * 2;
                            section.pointer += bin.size;
                            section.nodes.push_back(bin);
                        } else if (node.name == ".int" || node.name == ".word") {
                            binary_node bin;
                            bin.type = binary_node::binary;
                            bin.name = node.name;
                            bin.source_pos = node.pos;
                            bin.encoding.is_valid = true;
                            bin.encoding.can_relax = false;
                            bin.encoding.mneumonic = node.name;
                            bin.encoding.relax = 0;
                            bin.size = 0;
                            for (const ast& child : node.children) {
                                bin.operands.push_back(child.expr);
                            }
                            bin.size = bin.operands.size() * 4;
                            section.pointer += bin.size;
                            section.nodes.push_back(bin);
                        } else if (node.name == ".long") {
                            binary_node bin;
                            bin.type = binary_node::binary;
                            bin.name = node.name;
                            bin.source_pos = node.pos;
                            bin.encoding.is_valid = true;
                            bin.encoding.can_relax = false;
                            bin.encoding.mneumonic = node.name;
                            bin.encoding.relax = 0;
                            for (const ast& child : node.children)
                                bin.operands.push_back(child.expr);
                            bin.size = bin.operands.size() * 4;
                            section.pointer += bin.size;
                            section.nodes.push_back(bin);
                        } else if (node.name == ".dword" || node.name == ".quad" || node.name == ".qword") {
                            binary_node bin;
                            bin.type = binary_node::binary;
                            bin.name = node.name;
                            bin.source_pos = node.pos;
                            bin.encoding.is_valid = true;
                            bin.encoding.can_relax = false;
                            bin.encoding.mneumonic = node.name;
                            bin.encoding.relax = 0;
                            bin.size = 0;
                            for (const ast& child : node.children) {
                                bin.operands.push_back(child.expr);
                            }
                            bin.size = bin.operands.size() * 8;
                            section.pointer += bin.size;
                            section.nodes.push_back(bin);
                        } else if (node.name == ".float") {
                            binary_node bin;
                            bin.type = binary_node::binary;
                            bin.name = node.name;
                            bin.source_pos = node.pos;
                            bin.encoding.is_valid = true;
                            bin.encoding.can_relax = false;
                            bin.encoding.mneumonic = node.name;
                            for (const ast& child : node.children)
                                bin.operands.push_back(child.expr);
                            bin.size = bin.operands.size() * sizeof(uint32_t);
                            section.pointer += bin.size;
                            section.nodes.push_back(std::move(bin));
                        } else if (node.name == ".double") {
                            binary_node bin;
                            bin.type = binary_node::binary;
                            bin.name = node.name;
                            bin.source_pos = node.pos;
                            bin.encoding.is_valid = true;
                            bin.encoding.can_relax = false;
                            bin.encoding.mneumonic = node.name;
                            for (const ast& child : node.children)
                                bin.operands.push_back(child.expr);
                            bin.size = bin.operands.size() * sizeof(uint64_t);
                            section.pointer += bin.size;
                            section.nodes.push_back(std::move(bin));
                        } else if (node.name == ".ascii") {
                            binary_node bin;
                            bin.type = binary_node::binary;
                            bin.name = node.name;
                            bin.source_pos = node.pos;
                            bin.encoding.is_valid = true;
                            bin.encoding.can_relax = false;
                            bin.encoding.mneumonic = node.name;
                            for (const ast& child : node.children) {
                                bin.operands.push_back(child.expr);
                                if (child.expr.type == parser::expression::literal &&
                                    child.expr.token.type() == token::string_literal)
                                    bin.size += reduce_string_or_char(child.expr.token).lexeme().size();
                            }
                            section.pointer += bin.size;
                            section.nodes.push_back(std::move(bin));
                        } else if (node.name == ".asciz" || node.name == ".string") {
                            binary_node bin;
                            bin.type = binary_node::binary;
                            bin.name = node.name;
                            bin.source_pos = node.pos;
                            bin.encoding.is_valid = true;
                            bin.encoding.can_relax = false;
                            bin.encoding.mneumonic = node.name;
                            for (const ast& child : node.children) {
                                bin.operands.push_back(child.expr);
                                if (child.expr.type == parser::expression::literal &&
                                    child.expr.token.type() == token::string_literal)
                                    bin.size += reduce_string_or_char(child.expr.token).lexeme().size() + 1;
                            }
                            section.pointer += bin.size;
                            section.nodes.push_back(std::move(bin));
                        } else if (node.name == ".skip" || node.name == ".space") {
                            const int64_t count = require_absolute_integer(node.children.front().expr, "expected absolute size");
                            if (count < 0)
                                throw vhxcc::compile_error("space size cannot be negative", node.children.front().expr.token);
                            const int64_t fill_value = node.children.size() == 2
                                ? require_absolute_integer(node.children.back().expr, "expected absolute fill value")
                                : 0;
                            binary_node bin;
                            bin.type = binary_node::binary;
                            bin.name = node.name;
                            bin.source_pos = node.pos;
                            bin.encoding.is_valid = true;
                            bin.encoding.can_relax = false;
                            bin.encoding.mneumonic = node.name;
                            bin.encoding.relax = 0;
                            bin.size = static_cast<std::size_t>(count);
                            bin.encoding.bytes.insert(bin.encoding.bytes.end(), bin.size, static_cast<char>(fill_value));
                            bin.encoding.size = bin.size;
                            section.pointer += bin.size;
                            section.nodes.push_back(bin);
                        } else if (node.name == ".fill") {
                            const int64_t count = require_absolute_integer(node.children[0].expr, "expected absolute repeat count");
                            const int64_t unit_size = node.children.size() > 1
                                ? require_absolute_integer(node.children[1].expr, "expected absolute fill size")
                                : 1;
                            const int64_t value = node.children.size() > 2
                                ? require_absolute_integer(node.children[2].expr, "expected absolute fill value")
                                : 0;
                            if (count < 0 || unit_size <= 0 || unit_size > 8 ||
                                static_cast<uint64_t>(count) > std::vector<char>().max_size() / static_cast<uint64_t>(unit_size)) {
                                throw vhxcc::compile_error("invalid .fill size", token(token::identifier, node.name, node.pos));
                            }
                            binary_node bin;
                            bin.type = binary_node::binary;
                            bin.name = node.name;
                            bin.source_pos = node.pos;
                            bin.encoding.is_valid = true;
                            bin.encoding.can_relax = false;
                            bin.encoding.mneumonic = node.name;
                            bin.encoding.relax = 0;
                            const std::size_t total_size = static_cast<std::size_t>(count) * static_cast<std::size_t>(unit_size);
                            auto fill_bytes = itob(static_cast<uint64_t>(value), static_cast<std::size_t>(unit_size));
                            bin.encoding.bytes.reserve(total_size);
                            for (int64_t i = 0; i < count; ++i)
                                bin.encoding.bytes.insert(bin.encoding.bytes.end(), fill_bytes.begin(), fill_bytes.end());
                            bin.size = total_size;
                            bin.encoding.size = total_size;
                            section.pointer += bin.size;
                            section.nodes.push_back(std::move(bin));
                        } else if (node.name == ".offset") {
                            const int64_t target = require_absolute_integer(node.children.front().expr, "expected absolute offset");
                            if (target < 0)
                                throw vhxcc::compile_error("offset cannot be negative", node.children.front().expr.token);
                            binary_node bin;
                            bin.type = binary_node::org;
                            bin.name = node.name;
                            bin.source_pos = node.pos;
                            bin.size = static_cast<std::size_t>(target);
                            section.pointer = std::max(section.pointer, bin.size);
                            section.nodes.push_back(std::move(bin));
                        } else if (node.name == ".org") {
                            const int64_t target = require_absolute_integer(node.children.front().expr, "expected absolute origin");
                            if (target < 0)
                                throw vhxcc::compile_error("origin cannot be negative", node.children.front().expr.token);
                            binary_node bin;
                            bin.type = binary_node::org;
                            bin.name = node.name;
                            bin.source_pos = node.pos;
                            bin.size = static_cast<std::size_t>(target);
                            section.pointer = std::max(section.pointer, bin.size);
                            section.nodes.push_back(std::move(bin));
                        } else if (node.name == ".align" || node.name == ".balign") {
                            const int64_t alignment = require_absolute_integer(node.children.front().expr, "expected absolute alignment");
                            if (alignment <= 0)
                                throw vhxcc::compile_error("alignment must be positive", node.children.front().expr.token);
                            binary_node bin;
                            bin.type = binary_node::align;
                            bin.name = node.name;
                            bin.source_pos = node.pos;
                            bin.size = static_cast<std::size_t>(alignment);
                            const std::size_t remainder = section.pointer % bin.size;
                            if (remainder != 0)
                                section.pointer += bin.size - remainder;
                            section.nodes.push_back(std::move(bin));
                        } else if (node.name == ".incbin") {
                            std::filesystem::path filename = node.children.front().expr.token.lexeme();
                            std::filesystem::path resolved = filename;
                            std::ifstream input(resolved, std::ios::binary);
                            if (!input.is_open()) {
                                for (const auto& include_dir : opts.include_dirs) {
                                    resolved = include_dir / filename;
                                    input.clear();
                                    input.open(resolved, std::ios::binary);
                                    if (input.is_open())
                                        break;
                                }
                            }
                            if (!input.is_open())
                                throw vhxcc::compile_error(std::format("could not open binary file '{}'", filename.string()), token(token::string_literal, filename.string(), node.pos));
                            binary_node bin;
                            bin.type = binary_node::binary;
                            bin.name = node.name;
                            bin.source_pos = node.pos;
                            bin.encoding.is_valid = true;
                            bin.encoding.can_relax = false;
                            bin.encoding.mneumonic = node.name;
                            bin.encoding.bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
                            if (input.bad())
                                throw vhxcc::compile_error(std::format("could not read binary file '{}'", resolved.string()), token(token::string_literal, filename.string(), node.pos));
                            bin.size = bin.encoding.bytes.size();
                            bin.encoding.size = bin.size;
                            section.pointer += bin.size;
                            section.nodes.push_back(std::move(bin));
                        }
                    } break;
                    case ast::instruction: {
                        binary_node bin;
                        bin.type = binary_node::opcode;
                        bin.name = node.name;
                        bin.source_pos = node.pos;
                        for (const ast& child : node.children)
                            bin.operands.push_back(child.expr);
                        bin.encoding = query_mneumonic(node.name);
                        if (!bin.encoding.is_valid) {
                            vhxcc::error(std::format("unknown mneumonic `{}'", node.name), token(token::identifier, node.name, node.pos));
                            continue;
                        }
                        bin.size = bin.encoding.size;
                        section.pointer += bin.size;
                        section.nodes.push_back(bin);
                    } break;
                    case ast::label: {
                        binary_node bin;
                        bin.type = binary_node::label;
                        bin.name = node.name;
                        bin.source_pos = node.pos;
                        section.nodes.push_back(bin);
                    } break;
                    default:
                        break;
                }
            }
        }

        if (errored)
            return 1;

        bool changed = false;
        int step = 0;
        do {
            if ((step > 0 && !opts.relax) || step >= 128)
                break;
            
            labels_info.clear();
            changed = false;

            // 1. Calculate offsets pass: Walk all nodes and populate label addresses
            for (auto& [name, info] : sections_final) {
                std::size_t pointer = 0;
                for (auto& bin : info.nodes) {
                    if (bin.type == binary_node::label) {
                        labels_info[bin.name] = label_info{name, pointer};
                    } 
                    else if (bin.type == binary_node::org) {
                        pointer = std::max(pointer, bin.size);
                    } 
                    else if (bin.type == binary_node::align) {
                        // bin.size stores alignment boundary (e.g., 4, 8, 16)
                        std::size_t align_val = bin.size;
                        if (align_val > 0) {
                            std::size_t remainder = pointer % align_val;
                            if (remainder != 0) {
                                pointer += (align_val - remainder);
                            }
                        }
                    } 
                    else {
                        // binary data or opcode instruction contributes to size
                        pointer += bin.size;
                    }
                }
                info.pointer = pointer;
            }

            // If relaxation is turned off, we only needed Pass 1 to lock in label addresses
            if (!opts.relax) {
                break;
            }

            // 2. Relax pass: Re-evaluate instruction sizes now that label addresses are known
            for (auto& [name, info] : sections_final) {
                std::size_t pointer = 0;
                for (auto& bin : info.nodes) {
                    if (bin.type == binary_node::opcode) {
                        const int requested_relaxation = bin.encoding.relax + (bin.encoding.can_relax ? 1 : 0);
                        std::vector<parser::expression> operands;
                        for (const auto& expr : bin.operands)
                            operands.push_back(fold_and_validate(expr));
                        encode_result res = encode_mneumonic(bin.name, operands, pointer, requested_relaxation);
                        if (res.is_valid) {
                            const int previous_relaxation = bin.encoding.relax;
                            const std::size_t new_size = res.size ? res.size : res.bytes.size();
                            if (new_size != bin.size || res.relax > previous_relaxation)
                                changed = true;
                            bin.size = new_size;
                            res.size = new_size;
                            bin.encoding = res;
                        } // If the next relaxation is invalid, keep the last valid encoding.
                    }

                    // Keep pointer tracking in sync during the relaxation pass for PC-relative checks
                    if (bin.type == binary_node::label) {
                        // Labels contribute 0 size
                    } else if (bin.type == binary_node::org) {
                        pointer = std::max(pointer, bin.size);
                    } else if (bin.type == binary_node::align) {
                        std::size_t align_val = bin.size;
                        if (align_val > 0) {
                            std::size_t remainder = pointer % align_val;
                            if (remainder != 0) {
                                pointer += (align_val - remainder);
                            }
                        }
                    } else {
                        pointer += bin.size;
                    }
                }
            }

            step++;
        } while (changed);

        if (changed)
            vhxcc::error("instruction relaxation did not converge", lexer::none);

        labels_info.clear();
        for (auto& [name, info] : sections_final) {
            std::size_t pointer = 0;
            for (const binary_node& bin : info.nodes) {
                if (bin.type == binary_node::label) {
                    if (labels_info.contains(bin.name)) {
                        vhxcc::error(std::format("duplicate symbol `{}`", bin.name),
                                     token(token::identifier, bin.name, bin.source_pos));
                    } else {
                        labels_info.emplace(bin.name, label_info{name, pointer});
                    }
                } else if (bin.type == binary_node::org) {
                    if (bin.size < pointer) {
                        vhxcc::error(".org target precedes the current location",
                                     token(token::identifier, bin.name, bin.source_pos));
                    } else {
                        pointer = bin.size;
                    }
                } else if (bin.type == binary_node::align) {
                    const std::size_t remainder = pointer % bin.size;
                    if (remainder != 0)
                        pointer += bin.size - remainder;
                } else {
                    pointer += bin.size;
                }
            }
            info.pointer = pointer;
        }

        for (auto& [name, info] : sections_final) {
            std::size_t pointer = 0;
            for (binary_node& bin : info.nodes) {
                if (bin.type == binary_node::label)
                    continue;

                if (bin.type == binary_node::org) {
                    const std::size_t target = std::max(pointer, bin.size);
                    bin.encoding = {};
                    bin.encoding.is_valid = true;
                    bin.encoding.mneumonic = bin.name;
                    bin.encoding.bytes.resize(target - pointer, 0);
                    bin.encoding.size = bin.encoding.bytes.size();
                    pointer = target;
                    continue;
                }

                if (bin.type == binary_node::align) {
                    const std::size_t remainder = pointer % bin.size;
                    const std::size_t padding = remainder == 0 ? 0 : bin.size - remainder;
                    bin.encoding = {};
                    bin.encoding.is_valid = true;
                    bin.encoding.mneumonic = bin.name;
                    bin.encoding.bytes.resize(padding, 0);
                    bin.encoding.size = padding;
                    pointer += padding;
                    continue;
                }

                if (bin.type == binary_node::opcode) {
                    std::vector<parser::expression> operands;
                    for (const auto& expr : bin.operands) {
                        parser::expression folded = fold_and_validate(expr);
                        if (folded.type == parser::expression::none) {
                            vhxcc::error("invalid instruction operand", token(token::identifier, bin.name, expr.token.pos()));
                            continue;
                        }
                        operands.push_back(std::move(folded));
                    }
                    if (operands.size() != bin.operands.size()) {
                        pointer += bin.size;
                        continue;
                    }
                    encode_result encoding = encode_mneumonic(bin.name, operands, pointer, bin.encoding.relax);
                    if (!encoding.is_valid) {
                        vhxcc::error(std::format("illegal operands for `{}`", bin.name),
                                     token(token::identifier, bin.name, bin.source_pos));
                        pointer += bin.size;
                        continue;
                    }
                    const std::size_t encoded_size = encoding.size ? encoding.size : encoding.bytes.size();
                    if (encoded_size != bin.size || encoding.bytes.size() != encoded_size) {
                        vhxcc::error(std::format("inconsistent final encoding size for `{}`", bin.name),
                                     token(token::identifier, bin.name, bin.source_pos));
                    }
                    bin.encoding = std::move(encoding);
                    bin.size = encoded_size;
                    bin.encoding.size = bin.size;
                    pointer += bin.size;
                    continue;
                }

                if (bin.type == binary_node::binary) {
                    const bool fixed_fill = bin.name == ".space" || bin.name == ".skip" ||
                                             bin.name == ".fill" || bin.name == ".incbin";
                    if (!fixed_fill)
                        bin.encoding.bytes.clear();
                    bin.encoding.relocations.clear();
                    if (bin.name == ".byte" || bin.name == ".short" || bin.name == ".half" ||
                        bin.name == ".hword" || bin.name == ".int" || bin.name == ".word" ||
                        bin.name == ".long" || bin.name == ".dword" || bin.name == ".quad" ||
                        bin.name == ".qword") {
                        std::size_t width = 1;
                        if (bin.name == ".short" || bin.name == ".half" || bin.name == ".hword")
                            width = 2;
                        else if (bin.name == ".int" || bin.name == ".word" || bin.name == ".long")
                            width = 4;
                        else if (bin.name == ".dword" || bin.name == ".quad" || bin.name == ".qword")
                            width = 8;
                        for (const auto& expr : bin.operands)
                            add_data_value(bin, expr, width);
                    } else if (bin.name == ".ascii" || bin.name == ".asciz" || bin.name == ".string") {
                        for (const auto& expr : bin.operands) {
                            if (expr.type != parser::expression::literal ||
                                expr.token.type() != token::string_literal) {
                                throw vhxcc::compile_error("expected string literal", expr.token);
                            }
                            token value = reduce_string_or_char(expr.token);
                            bin.encoding.bytes.insert(bin.encoding.bytes.end(), value.lexeme().begin(), value.lexeme().end());
                            if (bin.name != ".ascii")
                                bin.encoding.bytes.push_back('\0');
                        }
                    } else if (bin.name == ".float" || bin.name == ".double") {
                        for (const auto& expr : bin.operands) {
                            parser::expression folded = constexpr_fold(expr, constexprs);
                            if (folded.type != parser::expression::literal)
                                throw vhxcc::compile_error("expected absolute floating-point value", expr.token);
                            double value = 0;
                            try {
                                value = constexpr_value(folded).as_double();
                            } catch (const std::invalid_argument&) {
                                throw vhxcc::compile_error("invalid floating-point value", expr.token);
                            } catch (const std::out_of_range&) {
                                throw vhxcc::compile_error("floating-point value out of range", expr.token);
                            }
                            if (bin.name == ".float") {
                                const auto bits = std::bit_cast<uint32_t>(static_cast<float>(value));
                                auto bytes = itob(bits, sizeof(bits));
                                bin.encoding.bytes.insert(bin.encoding.bytes.end(), bytes.begin(), bytes.end());
                            } else {
                                const auto bits = std::bit_cast<uint64_t>(value);
                                auto bytes = itob(bits, sizeof(bits));
                                bin.encoding.bytes.insert(bin.encoding.bytes.end(), bytes.begin(), bytes.end());
                            }
                        }
                    } else if (bin.name != ".space" && bin.name != ".skip" &&
                               bin.name != ".fill" && bin.name != ".incbin") {
                        vhxcc::error(std::format("unsupported data directive `{}`", bin.name),
                                     token(token::identifier, bin.name, bin.source_pos));
                    }
                    bin.size = bin.encoding.bytes.size();
                    bin.encoding.size = bin.size;
                    pointer += bin.size;
                    continue;
                }
            }
            info.pointer = pointer;

            // for (auto& node : asts) {
            //     switch (node.type) {
            //         case ast::directive: {
            //             if (node.name == ".byte") {
            //                 binary_node bin;
            //                 bin.name = node.name;
            //                 bin.type = binary_node::binary;
            //                 bin.encoding.is_valid = true;
            //                 bin.encoding.can_relax = false;
            //                 bin.encoding.mneumonic = node.name;
            //                 bin.encoding.relax = 0;
            //                 bin.size = 0;
            //                 for (const ast& child : node.children) {
            //                     bin.size += 1;
            //                     bin.operands.push_back(child.expr);
            //                     // parser::expression expr = fold_and_validate(child.expr);
            //                     // if (expr.type == parser::expression::literal) {
            //                     //     uint64_t val = std::stoll(expr.token.lexeme());
            //                     //     if (val > 0xff) {
            //                     //         vhxcc::warn(std::format("value {:#x} truncated to {:#x}", val, val & 0xff), expr.token);
            //                     //         val = val & 0xff;
            //                     //     }
            //                     //     std::vector<char> data = itob(val, 1);
            //                     //     bin.encoding.bytes.insert(bin.encoding.bytes.end(), data.begin(), data.end());
            //                     //     bin.encoding.size += data.size();
            //                     // } else {
            //                     //     vhxcc::error("illegal operands", expr.token);
            //                     //     continue;
            //                     // }
            //                 }
            //                 section.pointer += bin.size;
            //                 section.nodes.push_back(bin);
            //             } else if (node.name == ".short" || node.name == ".half" || node.name == ".hword") {
            //                 binary_node bin;
            //                 bin.type = binary_node::binary;
            //                 bin.name = node.name;
            //                 bin.encoding.is_valid = true;
            //                 bin.encoding.can_relax = false;
            //                 bin.encoding.mneumonic = node.name;
            //                 bin.encoding.relax = 0;
            //                 bin.size = 0;
            //                 for (const ast& child : node.children) {
            //                     bin.size += 2;
            //                     bin.operands.push_back(child.expr);
            //                     // parser::expression expr = fold_and_validate(child.expr);
            //                     // if (expr.type == parser::expression::literal) {
            //                     //     uint64_t val = std::stoll(expr.token.lexeme());
            //                     //     if (val > 0xffff) {
            //                     //         vhxcc::warn(std::format("value {:#x} truncated to {:#x}", val, val & 0xffff), expr.token);
            //                     //         val = val & 0xffff;
            //                     //     }
            //                     //     std::vector<char> data = itob(val, 2);
            //                     //     bin.encoding.bytes.insert(bin.encoding.bytes.end(), data.begin(), data.end());
            //                     //     bin.encoding.size += data.size();
            //                     // } else {
            //                     //     vhxcc::error("illegal operands", expr.token);
            //                     //     continue;
            //                     // }
            //                 }
            //                 section.pointer += bin.size;
            //                 section.nodes.push_back(bin);
            //             } else if (node.name == ".int" || node.name == ".word") {
            //                 binary_node bin;
            //                 bin.type = binary_node::binary;
            //                 bin.name = node.name;
            //                 bin.encoding.is_valid = true;
            //                 bin.encoding.can_relax = false;
            //                 bin.encoding.mneumonic = node.name;
            //                 bin.encoding.relax = 0;
            //                 bin.size = 0;
            //                 for (const ast& child : node.children) {
            //                     bin.size += 4;
            //                     bin.operands.push_back(child.expr);
            //                     // parser::expression expr = fold_and_validate(child.expr);
            //                     // if (expr.type == parser::expression::literal) {
            //                     //     uint64_t val = std::stoll(expr.token.lexeme());
            //                     //     if (val > 0xffffffffULL) {
            //                     //         vhxcc::warn(std::format("value {:#x} truncated to {:#x}", val, val & 0xffffffffULL), expr.token);
            //                     //         val = val & 0xffffffffULL;
            //                     //     }
            //                     //     std::vector<char> data = itob(val, 4);
            //                     //     bin.encoding.bytes.insert(bin.encoding.bytes.end(), data.begin(), data.end());
            //                     //     bin.encoding.size += data.size();
            //                     // } else if (expr.type != parser::expression::none) {
            //                     //     vhxcc::relocation reloc;
            //                     //     reloc.type = relocation::vhx_abs32;
            //                     //     reloc.offset = bin.size;
            //                     //     reloc.addend = 0;
            //                     //     if (expr.type == parser::expression::identifier) {
            //                     //         reloc.symbol = expr.token.lexeme();
            //                     //     } else if (expr.type == parser::expression::binary) {
            //                     //         reloc.symbol = expr.branches[0].token.lexeme();
            //                     //         reloc.addend = std::stoll(expr.branches[1].token.lexeme());
            //                     //         if (expr.token == token::minus)
            //                     //             reloc.addend = -reloc.addend;
            //                     //     }
            //                     //     bin.encoding.relocations.push_back(reloc);
            //                     //     std::vector<char> data = itob(0, 4);
            //                     //     bin.encoding.bytes.insert(bin.encoding.bytes.end(), data.begin(), data.end());
            //                     //     bin.encoding.size += data.size();
            //                     // }
            //                 }
            //                 section.pointer += bin.size;
            //                 section.nodes.push_back(bin);
            //             } else if (node.name == ".long" || node.name == ".quad" || node.name == ".qword") {
            //                 binary_node bin;
            //                 bin.type = binary_node::binary;
            //                 bin.name = node.name;
            //                 bin.encoding.is_valid = true;
            //                 bin.encoding.can_relax = false;
            //                 bin.encoding.mneumonic = node.name;
            //                 bin.encoding.relax = 0;
            //                 bin.size = 0;
            //                 for (const ast& child : node.children) {
            //                     bin.size += 8;
            //                     bin.operands.push_back(child.expr);
            //                     // parser::expression expr = fold_and_validate(child.expr);
            //                     // if (expr.type == parser::expression::literal) {
            //                     //     uint64_t val = std::stoll(expr.token.lexeme());
            //                     //     std::vector<char> data = itob(val, 8);
            //                     //     bin.encoding.bytes.insert(bin.encoding.bytes.end(), data.begin(), data.end());
            //                     //     bin.encoding.size += data.size();
            //                     // } else if (expr.type != parser::expression::none) {
            //                     //     vhxcc::relocation reloc;
            //                     //     reloc.type = relocation::vhx_abs64;
            //                     //     reloc.offset = bin.size;
            //                     //     reloc.addend = 0;
            //                     //     if (expr.type == parser::expression::identifier) {
            //                     //         reloc.symbol = expr.token.lexeme();
            //                     //     } else if (expr.type == parser::expression::binary) {
            //                     //         reloc.symbol = expr.branches[0].token.lexeme();
            //                     //         reloc.addend = std::stoll(expr.branches[1].token.lexeme());
            //                     //         if (expr.token == token::minus)
            //                     //             reloc.addend = -reloc.addend;
            //                     //     }
            //                     //     bin.encoding.relocations.push_back(reloc);
            //                     //     std::vector<char> data = itob(0, 8);
            //                     //     bin.encoding.bytes.insert(bin.encoding.bytes.end(), data.begin(), data.end());
            //                     //     bin.encoding.size += data.size();
            //                     // }
            //                 }
            //                 section.pointer += bin.size;
            //                 section.nodes.push_back(bin);
            //             } else if (node.name == ".float") {
            //                 // TODO
            //             } else if (node.name == ".double") {
            //                 // TODO
            //             } else if (node.name == ".ascii") {
            //                 // TODO
            //             } else if (node.name == ".asciz" || node.name == ".string") {
            //                 // TODO
            //             } else if (node.name == ".skip" || node.name == ".space") {
            //                 // can only take absexprs
            //                 parser::expression folded = constexpr_fold(node.children.front().expr, constexprs);
            //                 std::size_t size = constexpr_value(folded).as_integer();
            //                 char fill;
            //                 if (node.children.size() == 2) {
            //                     folded = constexpr_fold(node.children.back().expr, constexprs);
            //                     fill = constexpr_value(folded).as_integer();
            //                 } else {
            //                     fill = 0;
            //                 }
            //                 binary_node bin;
            //                 bin.type = binary_node::binary;
            //                 bin.name = node.name;
            //                 bin.encoding.is_valid = true;
            //                 bin.encoding.can_relax = false;
            //                 bin.encoding.mneumonic = node.name;
            //                 bin.encoding.relax = 0;
            //                 bin.size = size;
            //                 bin.encoding.bytes.insert(bin.encoding.bytes.end(), size, fill);
            //                 section.pointer += bin.size;
            //                 section.nodes.push_back(bin);
            //             } else if (node.name == ".offset") {
            //                 // TODO
            //             } else if (node.name == ".org") {
            //                 // TODO
            //             } else if (node.name == ".align") {
            //                 // TODO
            //             } else if (node.name == ".balign") {
            //                 // TODO
            //             } else if (node.name == ".incbin") {
            //                 // TODO
            //             }
            //         } break;
            //         case ast::instruction: {
            //             binary_node bin;
            //             bin.type = binary_node::opcode;
            //             bin.name = node.name;
            //             bin.encoding = query_mneumonic(node.name);
            //             if (!bin.encoding.is_valid) {
            //                 vhxcc::error(std::format("unknown mneumonic `{}'", node.name), token(token::identifier, node.name, node.pos));
            //                 continue;
            //             }
            //             bin.size = bin.encoding.size;
            //             section.pointer += bin.size;
            //             section.nodes.push_back(bin);
            //         } break;
            //         case ast::label: {
            //             binary_node bin;
            //             bin.type = binary_node::label;
            //             bin.name = node.name;
            //             label_info info;
            //             info.section = section_name;
            //             info.offset = section.pointer;
            //             section.nodes.push_back(bin);
            //         } break;
            //         default:
            //             break;
            //     }
            // }
        }

        if (warnings > 0 && opts.fatal_warnings) {
            if (warnings == 1)
                vhxcc::error("1 warning, treating warnings as errors");
            else
                vhxcc::error(std::format("{} warnings, treating warnings as errors", warnings));
        }
        
        if (errored)
            return 1;

    } catch (const vhxcc::compile_error& e) {
        std::cerr << std::format("{}{}:{}:{}: {}fatal error:{} {}", BOLD, e.token().pos().file, e.token().pos().line, e.token().pos().column, RED, RESET, e.what()) << std::endl;
        return 1;
    }
    
    // Proceed with the rest of the program using the parsed options
    return 0;
}