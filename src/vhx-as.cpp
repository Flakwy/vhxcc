
#include "error.hpp"
#include "token.hpp"
#include <cmath>
#include <cstddef>
#include <format>
#include <lexer.hpp>
#include <filesystem>
#include <iostream>
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
  -c
  -e <entry>              specify the entry point symbol <entry>
  -I <dir>                add <dir> to search list for .include directives
  -o <output>             name the object-file output <output> (default a.out)
  --version               print assembler version and exit
VHX options:
  -mabi=<abi name>        specify for ABI <abi name>
  -march=<arch name>      assemble for architecture <arch name>;
  -mrelax                 enable relax (default)
  -mno-relax              disable relax
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
};

#define BOLD "\033[1m"
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"

static std::string expr_to_string(enum parser::expression::type type) {
    switch (type) {
        case parser::expression::call: return "call";
        case parser::expression::binary: return "binary";
        case parser::expression::unary: return "unary";
        case parser::expression::literal: return "literal";
        case parser::expression::identifier: return "identifier";
        default: return "unknown";
    }
}

static std::string ast_type_str(enum ast::type type) {
    switch (type) {
        case ast::expression: return "expression";
        case ast::label: return "label";
        case ast::directive: return "directive";
        case ast::instruction: return "instruction";
        case ast::incbin: return "incbin";
        default: return "unknown";
    }
}

static void print_expression(const parser::expression& node, int indent = 0) {
    std::cout << std::string(indent*2, ' ');
    std::cout << "expression(" << expr_to_string(node.type) << "): " << node.token.lexeme() << std::endl;
    for (const auto& child : node.branches) {
        print_expression(child, indent + 1);
    }
}

static void print_ast(const ast& node, int indent = 0) {
    std::cout << std::string(indent*2, ' ');
    switch (node.type) {
        case ast::expression: {
            // std::cout << "ast(expression): " << node.expr.token.lexeme() << std::endl;
            print_expression(node.expr, indent);
        } break;
        case ast::label: std::cout << "ast(label): " << node.name << std::endl; break;
        case ast::directive: std::cout << "ast(directive): " << node.name << std::endl; break;
        case ast::instruction: std::cout << "ast(instruction): " << node.name << std::endl; break;
        case ast::incbin: std::cout << "ast(incbin)" << std::endl; break;
        default: std::cout << "unknown" << std::endl; break;
    }
    for (const auto& child : node.children) {
        print_ast(child, indent + 1);
    }
};

static void expand_builtins(ast& node, const std::unordered_map<std::string, ast (*)(ast&)>& builtins) {
    for (auto& child : node.children) {
        expand_builtins(child, builtins);
    }
    if (node.type == ast::expression && node.expr.type == parser::expression::call) {
        std::string builtin = node.expr.token.lexeme();
        if (!builtins.contains(builtin))
            throw vhxcc::compile_error(std::format("unknown builtin `{}'", builtin), node.expr.token);
        node = builtins.at(builtin)(node);
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
    vhxcc::position pos;
    std::variant<long long, double, std::string> value;

    constexpr_value() = default;
    constexpr_value(long long v) : value(v), pos() {}
    constexpr_value(int v) : value(v), pos() {}
    constexpr_value(double v) : value(v), pos() {}
    constexpr_value(const vhxcc::token& tok) : pos(tok.pos()) {
        if (tok.type() == token::integer_literal)
            value = std::stoll(tok.lexeme());
        else if (tok.type() == token::float_literal)
            value = std::stod(tok.lexeme());
        else if (tok.type() == token::char_literal) {
            long long value = 0;
            for (char c : tok.lexeme()) {
                value = (value << 8) | static_cast<unsigned char>(c);
            }
            this->value = value;
        }
    }
    constexpr_value(const parser::expression& expr) : pos(expr.token.pos()) {
        if (expr.token.type() == token::integer_literal)
            value = std::stoll(expr.token.lexeme());
        else if (expr.token.type() == token::float_literal)
            value = std::stod(expr.token.lexeme());
        else if (expr.token.type() == token::string_literal)
            value = expr.token.lexeme();
        else
            throw vhxcc::compile_error("expression must be constant", expr.token);
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
        else if (is_string() && !other.is_string() || !is_string() && other.is_string())
            return false;
        return as_string() == other.as_string();
    }
    bool operator!= (const constexpr_value& other) const {
        return !(*this == other);
    }
    bool operator< (const constexpr_value& other) const {
        if (is_string() && other.is_string())
            return std::get<std::string>(value) < std::get<std::string>(other.value);
        else if (is_string() && !other.is_string() || !is_string() && other.is_string())
            return false;
        if (is_double() || other.is_double())
            return as_double() < other.as_double();
        return as_integer() < other.as_integer();
    }
    bool operator<= (const constexpr_value& other) const {
        if (is_string() && other.is_string())
            return std::get<std::string>(value) <= std::get<std::string>(other.value);
        else if (is_string() && !other.is_string() || !is_string() && other.is_string())
            return false;
        if (is_double() || other.is_double())
            return as_double() <= other.as_double();
        return as_integer() <= other.as_integer();
    }
    bool operator> (const constexpr_value& other) const {
        if (is_string() && other.is_string())
            return std::get<std::string>(value) > std::get<std::string>(other.value);
        else if (is_string() && !other.is_string() || !is_string() && other.is_string())
            return false;
        if (is_double() || other.is_double())
            return as_double() > other.as_double();
        return as_integer() > other.as_integer();
    }
    bool operator>= (const constexpr_value& other) const {
        if (is_string() && other.is_string())
            return std::get<std::string>(value) >= std::get<std::string>(other.value);
        else if (is_string() && !other.is_string() || !is_string() && other.is_string())
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
            throw vhxcc::compile_error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::_and, "&", pos));
    }
    constexpr_value operator| (const constexpr_value& other) const {
        if (is_integer() && other.is_integer())
            return constexpr_value(as_integer() | other.as_integer());
        else
            throw vhxcc::compile_error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::_or, "|", pos));
    }
    constexpr_value operator^ (const constexpr_value& other) const {
        if (is_integer() && other.is_integer())
            return constexpr_value(as_integer() ^ other.as_integer());
        else
            throw vhxcc::compile_error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::_xor, "^", pos));
    }
    constexpr_value operator<< (const constexpr_value& other) const {
        if (is_integer() && other.is_integer())
            return constexpr_value(as_integer() << other.as_integer());
        else
            throw vhxcc::compile_error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::lshift, "<<", pos));
    }
    constexpr_value operator>> (const constexpr_value& other) const {
        if (is_integer() && other.is_integer())
            return constexpr_value(as_integer() >> other.as_integer());
        else
            throw vhxcc::compile_error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::rshift, ">>", pos));
    }
    constexpr_value operator~ () const {
        if (is_integer())
            return constexpr_value(~as_integer());
        else
            throw vhxcc::compile_error(std::format("invalid operand to unary expression ('{}')", type()), token(token::_not, "~", pos));
    }
    constexpr_value operator+ () const {
        if (is_integer())
            return constexpr_value(+as_integer());
        else if (is_double())
            return constexpr_value(+as_double());
        else
            throw vhxcc::compile_error(std::format("invalid operand to unary expression ('{}')", type()), token(token::plus, "+", pos));
    }
    constexpr_value operator-() const {
        if (is_integer())
            return constexpr_value(-as_integer());
        else if (is_double())
            return constexpr_value(-as_double());
        else
            throw vhxcc::compile_error(std::format("invalid operand to unary expression ('{}')", type()), token(token::minus, "-", pos));
    }
    constexpr_value operator+ (const constexpr_value& other) const {
        if (is_number() && other.is_number()) {
            if (is_double() || other.is_double())
                return constexpr_value(as_double() + other.as_double());
            return constexpr_value(as_integer() + other.as_integer());
        } else
            throw vhxcc::compile_error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::plus, "+", pos));
    }
    constexpr_value operator-(const constexpr_value& other) const {
        if (is_number() && other.is_number()) {
            if (is_double() || other.is_double())
                return constexpr_value(as_double() - other.as_double());
            return constexpr_value(as_integer() - other.as_integer());
        } else
            throw vhxcc::compile_error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::minus, "-", pos));
    }
    constexpr_value operator* (const constexpr_value& other) const {
        if (is_number() && other.is_number()) {
            if (is_double() || other.is_double())
                return constexpr_value(as_double() * other.as_double());
            return constexpr_value(as_integer() * other.as_integer());
        } else
            throw vhxcc::compile_error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::times, "*", pos));
    }
    constexpr_value operator/ (const constexpr_value& other) const {
        if (is_number() && other.is_number()) {
            if (is_double() || other.is_double())
                return constexpr_value(as_double() / other.as_double());
            return constexpr_value(as_integer() / other.as_integer());
        } else
            throw vhxcc::compile_error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::slash, "/", pos));
    }
    constexpr_value operator% (const constexpr_value& other) const {
        if (is_number() && other.is_number()) {
            if (is_double() || other.is_double())
                return constexpr_value(std::fmod(as_double(), other.as_double()));
            return constexpr_value(as_integer() % other.as_integer());
        } else
            throw vhxcc::compile_error(std::format("invalid operands to binary expression ('{}' and '{}')", type(), other.type()), token(token::modulo, "%", pos));
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
    opts opts;
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
        } else if (arg[0] == '-') {
            std::cerr << BOLD << "vhx-as:" << RED << " error:" << RESET << " unknown option '" << arg << "'" << std::endl;
        } else {
            // Handle input file
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
        ast child2;
        child2.type = ast::type::expression;
        child2.expr = parser::parse_expression(it, true);
        if (child2.expr.type != parser::expression::none) {
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
            if (tok != "push" && tok != "pop")
                throw vhxcc::compile_error(std::format("unknown directive: arch '{}'", tok.lexeme()));
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
        // Parse the source code into an abstract syntax tree (AST)
        vhxcc::lexer::iterator it = lexer.begin();
        while (it != lexer.end()) {
            // Process each token from the lexer

            while (lexer::match(it, vhxcc::token::semicolon))
                ;

            parser::expression expr = parser::parse_identifier(it, true);
            if (expr.type == parser::expression::identifier) {
                if (lexer::match(it, vhxcc::token::colon)) {
                    // label
                    std::string label = expr.token.lexeme();
                    ast ast;
                    ast.name = label;
                    ast.type = ast::type::label;
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
                    asts.push_back(node);
                    continue;
                } else {
                    // instruction
                    ast node;
                    node.name = expr.token.lexeme();
                    node.type = ast::type::instruction;
                    do {
                        ast child;
                        child.type = ast::type::expression;
                        child.expr = parser::parse_expression(it, true);
                        node.children.push_back(std::move(child));
                    } while (lexer::match(it, vhxcc::token::comma));
                    asts.push_back(node);
                    continue;
                }
            }

            // error matches GNU assembler error format
            throw vhxcc::compile_error(std::format("junk at end of line, first unrecognized token: `{}'", it->lexeme()), *it);
        }

        // for (const auto& ast : asts) {
        //     print_ast(ast, 0);
        // }

        std::unordered_map<std::string, ast (*)(ast&)> builtins;

        static auto make_unary = [](enum token::type op, const parser::expression& operand) {
            parser::expression expr;
            expr.type = parser::expression::unary;
            expr.token = token(op);
            expr.branches.push_back(operand);
            return expr;
        };

        static auto make_binary = [](const parser::expression& lhs, enum token::type op, const parser::expression& rhs) {
            parser::expression expr;
            expr.type = parser::expression::binary;
            expr.token = token(op);
            expr.branches.push_back(lhs);
            expr.branches.push_back(rhs);
            return expr;
        };

        static auto make_literal = [](long x) {
            parser::expression expr;
            expr.type = parser::expression::literal;
            expr.token = token(token::integer_literal, std::to_string(x));
            return expr;
        };

        static auto make_pointer = []() {
            parser::expression expr;
            expr.type = parser::expression::identifier;
            expr.token = token(token::pointer, "$");
            return expr;
        };

        static auto make_identifier = [](const std::string& id) {
            parser::expression expr;
            expr.type = parser::expression::identifier;
            expr.token = token(token::pointer, id);
            return expr;
        };

        builtins["hi"] = [](ast& node) -> ast {
            // hi as well!
            if (node.children.size() < 1)
                throw vhxcc::compile_error("expected expression", node.expr.token);
            ast result;
            result.type = ast::expression;
            result.expr = make_binary(make_binary(node.children[0].expr, token::rshift, make_literal(16)), token::_and, make_literal(0xffff));
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
        std::unordered_map<std::string, std::vector<ast>> sections;
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
                    if (cond_stack.size() <= 1)
                        throw vhxcc::compile_error(".else without matching .if");
                    
                    auto& current = cond_stack.back();
                    // If the 'if' branch was NOT taken, activate the else branch
                    current.active = parent_active && !current.taken;
                    current.taken = true; // Mark chain as fully exhausted
                    continue;
                }
                else if (ast.name == ".endif") {
                    if (cond_stack.size() <= 1)
                        throw vhxcc::compile_error("mismatched .endif");
                    cond_stack.pop_back();
                    continue;
                }
            }

            // Only process nodes if the current conditional scope allows emission
            if (cond_stack.back().active) {
                if (ast.type == ast::directive && ast.name == ".err") {
                    throw vhxcc::compile_error(".err encountered", token(token::identifier, ast.name, ast.pos));
                } else if (ast.type == ast::directive && ast.name == ".section") {
                    std::string section_name = ast.children.front().expr.token.lexeme();
                    if (section_name.empty())
                        throw vhxcc::compile_error("empty section name");
                    section = section_name;
                } else if (ast.type == ast::directive && (ast.name == ".set" || ast.name == ".equ" || ast.name == ".equiv")) {
                    std::string symbol_name = ast.children.front().expr.token.lexeme();
                    if (ast.name == ".equiv" && defined.contains(symbol_name))
                        throw vhxcc::compile_error(std::format("symbol `{}' is already defined", symbol_name), token(token::identifier, ast.name, ast.pos));
                    defined.insert(symbol_name);
                    parser::expression folded = constexpr_fold(ast.children[1].expr, constexprs);
                    if (folded.type != parser::expression::none) {
                        constexpr_value folded_value = folded;
                        constexprs[symbol_name] = folded;
                    }
                    sections[section].push_back(ast);
                } else if (ast.type == ast::label) {
                    defined.insert(ast.name);
                    sections[section].push_back(ast);
                } else {
                    sections[section].push_back(ast);
                }
            }
        }

        if (cond_stack.size() > 1) {
            throw vhxcc::compile_error("unclosed conditional block (missing .endif)");
        }

        std::unordered_map<std::string, std::size_t> section_sizes;
        std::unordered_map<std::string, std::unordered_map<std::string, std::size_t>> section_labels;
        std::size_t offset = 0;
        for (auto& [section, asts] : sections) {
            section_sizes[section] = 0;
            section_labels[section];
            for (auto& node : asts) {
                switch (node.type) {
                    case ast::directive: {
                        if (node.name == ".byte") {
                            section_sizes[section] += 1 * node.children.size();
                        } else if (node.name == ".short" || node.name == ".half" || node.name == ".hword") {
                            section_sizes[section] += 2 * node.children.size();
                        } else if (node.name == ".int" || node.name == ".word") {
                            section_sizes[section] += 4 * node.children.size();
                        } else if (node.name == ".long" || node.name == ".quad" || node.name == ".qword") {
                            section_sizes[section] += 8 * node.children.size();
                        } else if (node.name == ".float") {
                            section_sizes[section] += 4 * node.children.size();
                        } else if (node.name == ".double") {
                            section_sizes[section] += 8 * node.children.size();
                        } else if (node.name == ".ascii") {
                            for (auto& child : node.children) {
                                if (child.type == ast::expression &&
                                    child.expr.type == parser::expression::literal &&
                                    child.expr.token.type() == token::string_literal) {
                                    
                                    section_sizes[section] += child.expr.token.lexeme().size();
                                } else {
                                    throw vhxcc::compile_error(std::format("unexpected {}", token::type_str(child.expr.token.type())), child.expr.token);
                                }
                            }
                        } else if (node.name == ".asciz" || node.name == ".string") {
                            for (auto& child : node.children) {
                                if (child.type == ast::expression &&
                                    child.expr.type == parser::expression::literal &&
                                    child.expr.token.type() == token::string_literal) {
                                    
                                    section_sizes[section] += child.expr.token.lexeme().size();
                                } else {
                                    throw vhxcc::compile_error(std::format("unexpected {}", token::type_str(child.expr.token.type())), child.expr.token);
                                }
                            }
                        } else if (node.name == ".skip" || node.name == ".space") {

                        } else if (node.name == ".offset") {

                        } else if (node.name == ".org") {

                        } else if (node.name == ".equ") {

                        } else if (node.name == ".equiv") {

                        }
                    } break;
                    case ast::instruction: {

                    } break;
                    case ast::label: {

                    } break;
                    default:
                        break;
                }
            }
        }
        

    } catch (const vhxcc::compile_error& e) {
        std::cerr << std::format("{}{}:{}:{}: {}error:{} {}", BOLD, e.token().pos().file, e.token().pos().line, e.token().pos().column, RED, RESET, e.what()) << std::endl;
        return 1;
    }
    
    // Proceed with the rest of the program using the parsed options
    return 0;
}