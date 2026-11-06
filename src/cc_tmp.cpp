#include <isa.hpp>
#include <lexer.hpp>
#include <memory>
#include <parser.hpp>
#include <error.hpp>

#include <array>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
extern char** environ;
#endif

#ifndef HOSTNAME
#define HOSTNAME "unknown"
#endif

#ifndef VERSION
#define VERSION ""
#endif

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
    std::cerr << std::format("{}{}:{}:{}: {}warning:{} {}", BOLD, tok.pos().file, tok.pos().line, tok.pos().column, MAGENTA, RESET, msg) << std::endl;
    warnings++;
}

static void fatal_error(std::string_view msg, bool internal = false) {
    std::cerr << std::format("{}{}: {}fatal error:{} {}", BOLD, first_file, RED, RESET, msg) << std::endl;
    std::exit(internal ? 4 : 1);
}

namespace {

using vhxcc::lexer;
using vhxcc::parser;
using vhxcc::token;

const std::string help = R"(Usage: vhx64-none-vxo-cc [options] file...
Options:
  --help                Display this information.
  --target-help         Display target specific command line options (including assembler and linker options).
  --help={common|options|params|target|warnings|[^]{joined|separate|undocumented}}[,...].
                        Display specific types of command line options.
  (Use `-v --help` to display command line options of sub-processes).
  --version             Show version information
  -Wa,<options>         Pass comma-separated <options> on to the assembler.
  -Wp,<options>         Pass comma-separated <options> on to the preprocessor.
  -Wl,<options>         Pass comma-separated <options> on to the linker.
  -Xassembler <arg>     Pass <arg> to the assembler.
  -Xpreprocessor <arg>  Pass <arg> to the preprocessor.
  -Xlinker <arg>        Pass <arg> to the linker.
  -save-temps           Do not delete intermediate files.
  -pipe                 Accepted for GCC compatibility; intermediates use files.
  -time                 Time the execution of each subprocess.
  -v                    Display the programs invoked by the compiler.
  -E                    Preprocess only; do not compile, assemble, or link.
  -S                    Compile only; do not assemble or link.
  -c                    Compile and assemble only; do not link.
  -o <file>             Place the output into <file>.
  -pie                  Create a position-independent executable.
  -x <language>         Specify the language of the following input files.
                        Permitted values: c assembler none
                        'none' means revert to the default behavior of
                        guessing the language based on the file's extension.
  -march=ARCH           Set target architecture (default vhx64imsa)

Supported C subset:
  Integer function declarations and definitions, static functions, integer
  typedef aliases, up to eight register-passed parameters and locals, return
  statements, integer literals, +, -, &, |, ^, <<, >>, simple and compound
  assignments.
  Preprocessing, assembly, and linking are delegated to separate programs.

Integer arithmetic currently uses target-register width; C integer promotions,
overflow rules, narrower type conversions, calls, control flow, pointers,
aggregate types, and global data are not implemented yet.

Report bugs to <https://github.com/Flakwy/vhxcc/issues/>)";

const std::string version =
    "VHX C compiler (vhxcc for " HOSTNAME ") " VERSION "\n"
    "Copyright (C) 2026 Free Software Foundation Inc.\n"
    "This program is free software; you may redistribute it under the terms of\n"
    "the GNU General Public License version 3 or later.\n"
    "This program has absolutely no warranty.";

static struct options {
    struct input {
        std::filesystem::path path;
        std::string language = "none";
    };
    std::vector<input> inputs;
    std::filesystem::path output = "a.out";
    std::string arch = "vhx64imsa";
    std::string abi = "lp64";
    std::string language = "none";
    std::vector<std::string> preprocessor_args;
    std::vector<std::string> assembler_args;
    std::vector<std::string> linker_args;
    bool preprocess_only = false;
    bool assembly_only = false;
    bool compile_only = false;
    bool save_temps = false;
    bool verbose = false;
    bool syntax_only = false;
    bool output_set = false;
    bool help = false;
    bool version = false;
} opts; // use this from now on so static access to options can occur


enum regidx : unsigned {
    x0 = 0, zer = 0,
    x1 = 1, ra = 1,
    x2 = 2, sp = 2,
    x3 = 3, gp = 3,
    x4 = 4, tp = 4,
    x5 = 5, a0 = 5,
    x6 = 6, a1 = 6,
    x7 = 7, a2 = 7,
    x8 = 8, a3 = 8,
    x9 = 9, a4 = 9,
    x10 = 10, a5 = 10,
    x11 = 11, a6 = 11,
    x12 = 12, a7 = 12,
    x13 = 13, t0 = 13,
    x14 = 14, t1 = 14,
    x15 = 15, t2 = 15,
    x16 = 16, t3 = 16,
    x17 = 17, t4 = 17,
    x18 = 18, t5 = 18,
    x19 = 19, t6 = 19,
    x20 = 20, t7 = 20,
    x21 = 21, s0 = 21,
    x22 = 22, s1 = 22,
    x23 = 23, s2 = 23,
    x24 = 24, s3 = 24,
    x25 = 25, s4 = 25,
    x26 = 26, s5 = 26,
    x27 = 27, s6 = 27,
    x28 = 28, s7 = 28,
    x29 = 29, s8 = 29,
    x30 = 30, lp = 30, t8 = 30,
    x31 = 31, pc = 31
};

struct type {
    enum class kind {
        none,
        void_,
        char_,
        short_,
        int_,
        long_,
        long_long_,
        struct_
    } width = kind::none;

    std::string name; // for structs
    std::size_t sizeof_ = 0;
    std::size_t alignof_ = 0;
    std::size_t depth = 0; // pointer depth, 0 for non-pointer types

    std::size_t size() const {
        if (width == kind::none)
            return 0;
        else if (width == kind::struct_)
            return sizeof_;
        else if (depth > 0) {
            if (opts.abi == "ilp32")
                return 4;
            else if (opts.abi == "lp64")
                return 8;
        }
        else if (width == kind::char_)
            return 1;
        else if (width == kind::short_)
            return 2;
        else if (width == kind::int_)
            return 4;
        else if (width == kind::long_) {
            if (opts.abi == "ilp32")
                return 4;
            else if (opts.abi == "lp64")
                return 8;
        }
        else if (width == kind::long_long_)
            return 8;
        return sizeof_;
    }

    bool is_integral() const {
        return (depth > 0) || (width != kind::none && width != kind::struct_);
    }
};

struct storage {
    enum class kind {
        none,
        register_,
        static_, // or global
        stack,
        temporary,
        constant
    } kind = kind::none;

    regidx reg = zer; // register number for register storage
    std::size_t offset = 0; // stack offset for stack storage
    std::string name; // symbol name for static/global storage
    
    bool const_ = false; // not planned to implement const correctness yet
    bool volatile_ = false; // not planned to implement non-volatile optimizations yet
    bool restrict_ = false; // not planned to implement restrict optimizations yet

    bool is_lvalue() const {
        return kind == kind::register_ || kind == kind::stack || kind == kind::static_;
    }

    // for knowing if the value is stored in memory (stack or static/global)
    // this is useful for deciding if we need to spill a register value to memory
    // for a function parameter pointer reference
    bool is_memory() const {
        return kind == kind::stack || kind == kind::static_;
    }

    bool is_constant() const {
        return kind == kind::constant;
    }
};

struct value {
    type type_;
    storage storage_;
    uint64_t constant_;

    std::size_t size() const {
        return type_.size();
    }

    bool is_lvalue() const {
        return storage_.is_lvalue();
    }

    bool is_memory() const {
        return storage_.is_memory();
    }

    bool is_integral() const {
        return type_.is_integral();
    }

    bool is_pointer() const {
        return type_.depth > 0;
    }

    bool is_constant() const {
        return storage_.is_constant();
    }

    operator bool() const {
        return type_.width != type::kind::none;
    }

    value() = default;
    value(const type& t, const storage& s) : type_(t), storage_(s) {}
    value(regidx reg, const type& t) : type_(t), storage_({storage::kind::register_, reg, 0, ""}) {}
    value(std::size_t offset, const type& t) : type_(t), storage_({storage::kind::stack, zer, offset, ""}) {}
    value(std::string name, const type& t) : type_(t), storage_({storage::kind::static_, zer, 0, name}) {}

    value cast(const type& new_type) const {
        // let the caller handle lvalue checks, we just return a new value with the new type and the same storage
        return value(new_type, storage_);
    }

    value decay() const {
        if (type_.depth > 0) {
            // pointer type, decay to pointer type with depth - 1
            type new_type = type_;
            new_type.depth--;
            return value(new_type, storage_);
        } else if (storage_.kind == storage::kind::register_ || storage_.kind == storage::kind::stack) {
            // lvalue, decay to rvalue of the same type
            return value(type_, storage_);
        } else {
            // already an rvalue, return as is
            return *this;
        }
    }

    value dereference() const {
        if (type_.depth == 0) {
            vhxcc::error("cannot dereference a non-pointer type");
            return *this;
        }
        type new_type = type_;
        new_type.depth--;
        return value(new_type, storage_);
    }

    value addressof() const {
        type new_type = type_;
        new_type.depth++;
        return value(new_type, storage_);
    }

    value promote() const {
        if (opts.arch.starts_with("vhx64") || opts.arch.starts_with("vhx128")) {
            // integer promotions: char and short are promoted to long
            if (type_.width == type::kind::char_ || type_.width == type::kind::short_ || type_.width == type::kind::int_) {
                type new_type = type_;
                new_type.width = type::kind::long_;
                return value(new_type, storage_);
            }
        } else if (opts.arch.starts_with("vhx32")) {
            // integer promotions: char and short are promoted to int
            if (type_.width == type::kind::char_ || type_.width == type::kind::short_) {
                type new_type = type_;
                new_type.width = type::kind::int_;
                return value(new_type, storage_);
            }
        } else {
            vhxcc::error("unknown architecture for integer promotions: " + opts.arch);
        }
        return *this;
    }
};

struct member {
    type type_;
    storage storage_; // reuse storage::offset
};

using struct_ = std::unordered_map<std::string, member>;
using union_ = std::unordered_map<std::string, member>;

// a more complex expression type for more than what the parser should handle
struct expression {
    enum class kind {
        none,
        unary,
        binary,
        ternary,
        call,
        value,
        member // for . and -> operators
    } type_ = kind::none;

    enum class unary_ {
        none,
        plus,
        minus,
        deref,
        addr,
        not_,
        lnot,
        inc,
        dec,
        postinc,
        postdec,
        cast
    } unary_op = unary_::none;

    enum class binary_ {
        none,
        add,
        sub,
        mult,
        div,
        mod,
        and_,
        or_,
        xor_,
        lshift,
        rshift,
        land,
        lor,
        equals,
        not_equal,
        less,
        less_equal,
        greater,
        greater_equal,
        assign,
        add_assign,
        sub_assign,
        mult_assign,
        div_assign,
        mod_assign,
        and_assign,
        or_assign,
        xor_assign,
        lshift_assign,
        rshift_assign,
        array_index,
        dot,
        arrow
    } binary_op = binary_::none;

    std::unique_ptr<expression> cond_;
    std::unique_ptr<expression> lhs_;
    std::unique_ptr<expression> rhs_;
    std::vector<expression> args;
    value value_;
    type cast_;

    bool can_fold() const {
        if (type_ == kind::unary)
            return lhs().can_fold() &&
                (unary_op != unary_::deref && unary_op != unary_::addr &&
                unary_op != unary_::inc && unary_op != unary_::dec &&
                unary_op != unary_::postdec && unary_op != unary_::postinc);
        else if (type_ == kind::binary)
            return lhs().can_fold() && rhs().can_fold() &&
                (binary_op != binary_::dot && binary_op != binary_::arrow &&
                binary_op != binary_::assign && binary_op != binary_::add_assign &&
                binary_op != binary_::sub_assign && binary_op != binary_::mult_assign &&
                binary_op != binary_::div_assign && binary_op != binary_::mod_assign &&
                binary_op != binary_::and_assign && binary_op != binary_::or_assign &&
                binary_op != binary_::xor_assign && binary_op != binary_::lshift_assign &&
                binary_op != binary_::rshift_assign && binary_op != binary_::array_index);
        else if (type_ == kind::ternary)
            return cond().can_fold() && lhs().can_fold() && rhs().can_fold();
        else if (type_ == kind::value)
            return value_.is_constant();
        return false;
    }

    bool returns_lvalue() const {
        // TODO
        return false;
    }

    // helper functions
    static inline const expression& none() {
        static const expression none_{kind::none, unary_::none, binary_::none, nullptr, nullptr, nullptr, {}, value(), type()};
        return none_;
    }

    const expression& cond() const {
        if (!cond_)
            return expression::none();
        return *cond_;
    }

    const expression& lhs() const {
        if (!lhs_)
            return expression::none();
        return *lhs_;
    }

    const expression& rhs() const {
        if (!rhs_)
            return expression::none();
        return *rhs_;
    }
};

struct statement {
    enum class kind {
        expression,
        compound,
        selection,
        iteration,
        jump, // break, continue, goto, and return
        declaration,
        empty
    } type = kind::empty;

    token source_token;
    std::string name;

    // expression, selection condition, iteration condition statements, declaration initializer, or return value
    std::optional<expression> expr;

    // for compound
    std::vector<statement> body;
};

struct parameter {
    token name;
    value value;
};

struct function {
    token name;
    bool is_global = true;
    bool returns_value = true;
    bool definition = false;
    bool typedef_declaration = false;
    std::vector<parameter> parameters;
    std::optional<statement> body;
};

static std::unordered_map<std::string, type> typedef_names;

static void __init_types() {
    typedef_names["void"] = type{type::kind::void_, "void", 0, 0, 0};
    typedef_names["char"] = type{type::kind::char_, "char", 1, 1, 0};
    typedef_names["_Bool"] = type{type::kind::char_, "char", 1, 1, 0};
    // short, long, and long long should be handled as modifiers of int
    typedef_names["int"] = type{type::kind::int_, "int", 4, 4, 0};
    typedef_names["float"] = type{type::kind::int_, "float", 4, 4, 0};
    typedef_names["double"] = type{type::kind::int_, "double", 8, 8, 0};
}









// everything below is untouched/stale

static token expect_token(lexer::iterator& it, enum token::type type, std::string_view message) {
    if (*it != type)
        throw vhxcc::compile_error(message, *it);
    token result = *it;
    ++it;
    return result;
}

static bool is_type_word(const token& tok) {
    static const std::unordered_set<std::string> types = {
        "void", "char", "short", "int", "long", "signed", "unsigned", "_Bool"
    };
    return tok.type() == token::identifier &&
           (types.contains(tok.lexeme()) || typedef_names.contains(tok.lexeme()));
}

struct type_specifier {
    bool is_void = false;
};

static type_specifier parse_type_specifier(lexer::iterator& it) {
    bool saw_type = false;
    bool is_void = false;
    while (is_type_word(*it)) {
        const std::string& word = it->lexeme();
        if (word == "void") {
            if (saw_type)
                throw vhxcc::compile_error("invalid combination of type specifiers", *it);
            is_void = true;
        } else if (is_void) {
            throw vhxcc::compile_error("invalid combination of type specifiers", *it);
        }
        saw_type = true;
        ++it;
    }
    if (!saw_type)
        throw vhxcc::compile_error("expected an integer type", *it);
    return {is_void};
}

static parser::expression parse_expression(lexer::iterator& it) {
    return parser::expect_expression(it, false);
}

static statement parse_statement(lexer::iterator& it) {
    statement result;
    result.source_token = *it;
    if (*it == token::semicolon) {
        ++it;
        return result;
    }
    if (*it == token::lcurly)
        throw vhxcc::compile_error("nested blocks are not implemented yet", *it);
    if (it->type() == token::identifier) {
        const std::string keyword = it->lexeme();
        if (keyword == "if" || keyword == "else" || keyword == "while" ||
            keyword == "do" || keyword == "for" || keyword == "switch" ||
            keyword == "case" || keyword == "default" || keyword == "break" ||
            keyword == "continue" || keyword == "goto") {
            throw vhxcc::compile_error("control-flow statements are not implemented yet", *it);
        }
    }
    if (it->type() == token::identifier && it->lexeme() == "return") {
        result.type = statement::kind::return_value;
        result.source_token = *it;
        ++it;
        if (*it != token::semicolon)
            result.expr = parse_expression(it);
        expect_token(it, token::semicolon, "expected ';' after return statement");
        return result;
    }

    if (is_type_word(*it)) {
        const type_specifier type = parse_type_specifier(it);
        if (type.is_void)
            throw vhxcc::compile_error("variables of type void are not allowed", *it);
        result.type = statement::kind::declaration;
        result.name = expect_token(it, token::identifier, "expected variable name").lexeme();
        if (lexer::match(it, token::assign))
            result.expr = parse_expression(it);
        expect_token(it, token::semicolon, "expected ';' after declaration");
        return result;
    }

    result.type = statement::kind::expression;
    result.expr = parse_expression(it);
    expect_token(it, token::semicolon, "expected ';' after expression");
    return result;
}

static function parse_external_declaration(lexer::iterator& it) {
    function result;
    bool is_static = false;
    bool is_typedef = false;
    while (it->type() == token::identifier &&
           (it->lexeme() == "static" || it->lexeme() == "extern" ||
            it->lexeme() == "typedef")) {
        if (it->lexeme() == "static")
            is_static = true;
        else if (it->lexeme() == "typedef")
            is_typedef = true;
        ++it;
    }

    const type_specifier return_type = parse_type_specifier(it);
    if (is_typedef) {
        result.name = expect_token(it, token::identifier, "expected typedef name");
        expect_token(it, token::semicolon, "expected ';' after typedef declaration");
        if (!typedef_names.insert(result.name.lexeme()).second)
            throw vhxcc::compile_error("redefinition of typedef '" +
                                       result.name.lexeme() + "'", result.name);
        result.typedef_declaration = true;
        return result;
    }
    result.returns_value = !return_type.is_void;
    result.is_global = !is_static;
    result.name = expect_token(it, token::identifier, "expected function name");
    expect_token(it, token::lparen, "expected '(' after function name");

    if (!lexer::match(it, token::rparen)) {
        if (it->type() == token::identifier && it->lexeme() == "void") {
            ++it;
            expect_token(it, token::rparen, "expected ')' after void parameter list");
        } else {
            while (true) {
                const type_specifier parameter_type = parse_type_specifier(it);
                if (parameter_type.is_void)
                    throw vhxcc::compile_error("void parameter must be the entire parameter list", *it);
                token parameter_name;
                if (it->type() == token::identifier)
                    parameter_name = *it++;
                else
                    throw vhxcc::compile_error("expected parameter name", *it);
                result.parameters.push_back({std::move(parameter_name)});
                if (!lexer::match(it, token::comma))
                    break;
            }
            expect_token(it, token::rparen, "expected ')' after parameter list");
        }
    }
    if (result.parameters.size() > 8)
        throw vhxcc::compile_error("functions may have at most eight parameters", result.name);

    if (lexer::match(it, token::semicolon)) {
        result.definition = false;
        return result;
    }
    expect_token(it, token::lcurly, "expected ';' or function body after declaration");
    result.definition = true;
    while (*it != token::rcurly) {
        if (*it == token::eof)
            throw vhxcc::compile_error("unterminated function body", result.name);
        result.body.push_back(parse_statement(it));
    }
    ++it;
    return result;
}

static uint64_t parse_integer_literal(const token& tok) {
    std::string text = tok.lexeme();
    while (!text.empty() &&
           (text.back() == 'u' || text.back() == 'U' ||
            text.back() == 'l' || text.back() == 'L')) {
        text.pop_back();
    }
    if (text.empty())
        throw vhxcc::compile_error("invalid integer literal", tok);
    std::size_t parsed = 0;
    try {
        const uint64_t value = std::stoull(text, &parsed, 0);
        if (parsed != text.size())
            throw vhxcc::compile_error("invalid integer literal", tok);
        return value;
    } catch (const vhxcc::compile_error&) {
        throw;
    } catch (const std::exception&) {
        throw vhxcc::compile_error("integer literal is out of range", tok);
    }
}

static std::string assembler_integer(uint64_t value) {
    if (value <= static_cast<uint64_t>((std::numeric_limits<int64_t>::max)()))
        return std::to_string(value);
    const uint64_t magnitude = ~value + 1;
    if (magnitude == (uint64_t{1} << 63))
        return "(-9223372036854775807 - 1)";
    return "-" + std::to_string(magnitude);
}

static parser::expression register_expression(unsigned reg, const token& source) {
    parser::expression expr;
    expr.type = parser::expression::identifier;
    expr.token = token(token::identifier, "x" + std::to_string(reg), source.pos());
    return expr;
}

static parser::expression constant_expression(uint64_t value, const token& source) {
    parser::expression expr;
    expr.type = parser::expression::literal;
    expr.token = token(token::integer_literal, std::to_string(value), source.pos());
    return expr;
}

struct value {
    unsigned reg;
    bool owned;
};

struct generated_function {
    std::vector<char> code;
    std::string assembly;
};

class function_compiler {
    const function& _function;
    std::unordered_map<std::string, unsigned> _variables;
    std::array<bool, 8> _temporaries{};
    std::vector<char> _code;
    std::string _assembly;

    unsigned acquire(const token& source) {
        for (std::size_t i = 0; i < _temporaries.size(); ++i) {
            if (!_temporaries[i]) {
                _temporaries[i] = true;
                return static_cast<unsigned>(13 + i);
            }
        }
        throw vhxcc::compile_error("expression requires too many temporary registers", source);
    }

    void release(value val) {
        if (val.owned)
            _temporaries.at(val.reg - 13) = false;
    }

    void emit(const std::string& mnemonic, const std::vector<parser::expression>& operands,
              const token& source) {
        vhxcc::encode_result encoded =
            vhxcc::encode_mneumonic(mnemonic, operands, _code.size());
        if (!encoded.is_valid || encoded.bytes.size() != 4)
            throw vhxcc::compile_error("internal error encoding instruction '" + mnemonic + "'", source);
        _code.insert(_code.end(), encoded.bytes.begin(), encoded.bytes.end());
        _assembly += "    " + mnemonic;
        if (!operands.empty())
            _assembly += ' ';
        for (std::size_t i = 0; i < operands.size(); ++i) {
            if (i != 0)
                _assembly += ", ";
            _assembly += operands[i].token.lexeme();
        }
        _assembly += '\n';
    }

    void emit_rr(const std::string& mnemonic, unsigned rd, unsigned rm, unsigned rn,
                 const token& source) {
        emit(mnemonic, {register_expression(rd, source),
                        register_expression(rm, source),
                        register_expression(rn, source)}, source);
    }

    void emit_move(unsigned rd, unsigned rm, const token& source) {
        if (rd != rm)
            emit("mv", {register_expression(rd, source),
                          register_expression(rm, source)}, source);
    }

    void emit_constant(unsigned rd, uint64_t number, const token& source) {
        emit("li", {register_expression(rd, source),
                      constant_expression(number, source)}, source);
    }

    value compile_expression(const parser::expression& expr) {
        if (expr.type == parser::expression::literal) {
            if (expr.token.type() != token::integer_literal)
                throw vhxcc::compile_error("only integer literals are supported", expr.token);
            const unsigned reg = acquire(expr.token);
            emit_constant(reg, parse_integer_literal(expr.token), expr.token);
            return {reg, true};
        }

        if (expr.type == parser::expression::identifier) {
            const auto found = _variables.find(expr.token.lexeme());
            if (found == _variables.end())
                throw vhxcc::compile_error("unknown variable '" + expr.token.lexeme() + "'", expr.token);
            return {found->second, false};
        }

        if (expr.type == parser::expression::unary) {
            if (expr.branches.size() != 1)
                throw vhxcc::compile_error("malformed unary expression", expr.token);
            value operand = compile_expression(expr.branches[0]);
            if (expr.token.type() == token::plus)
                return operand;
            if (expr.token.type() != token::minus) {
                release(operand);
                throw vhxcc::compile_error("this unary operator is not implemented yet", expr.token);
            }
            const unsigned result = acquire(expr.token);
            emit_rr("sub", result, 0, operand.reg, expr.token);
            release(operand);
            return {result, true};
        }

        if (expr.type != parser::expression::binary || expr.branches.size() != 2)
            throw vhxcc::compile_error("this expression is not implemented yet", expr.token);

        const parser::expression& left_expr = expr.branches[0];
        const parser::expression& right_expr = expr.branches[1];
        if (expr.token.type() == token::assign ||
            expr.token.type() == token::plus_assign ||
            expr.token.type() == token::minus_assign) {
            if (left_expr.type != parser::expression::identifier)
                throw vhxcc::compile_error("assignment target must be a local variable", left_expr.token);
            const auto variable = _variables.find(left_expr.token.lexeme());
            if (variable == _variables.end())
                throw vhxcc::compile_error("unknown variable '" + left_expr.token.lexeme() + "'", left_expr.token);
            value right = compile_expression(right_expr);
            const unsigned destination = variable->second;
            if (expr.token.type() == token::plus_assign ||
                expr.token.type() == token::minus_assign) {
                const unsigned result = acquire(expr.token);
                emit_rr(expr.token.type() == token::plus_assign ? "add" : "sub",
                        result, destination, right.reg, expr.token);
                release(right);
                right = {result, true};
            }
            emit_move(destination, right.reg, expr.token);
            release(right);
            return {destination, false};
        }

        value left = compile_expression(left_expr);
        value right = compile_expression(right_expr);
        std::string mnemonic;
        switch (expr.token.type()) {
            case token::plus: mnemonic = "add"; break;
            case token::minus: mnemonic = "sub"; break;
            case token::_and: mnemonic = "and"; break;
            case token::_or: mnemonic = "or"; break;
            case token::_xor: mnemonic = "xor"; break;
            case token::lshift: mnemonic = "sll"; break;
            case token::rshift: mnemonic = "sra"; break;
            default:
                release(left);
                release(right);
                throw vhxcc::compile_error("this binary operator is not implemented yet", expr.token);
        }
        const unsigned result = acquire(expr.token);
        emit_rr(mnemonic, result, left.reg, right.reg, expr.token);
        release(left);
        release(right);
        return {result, true};
    }

public:
    explicit function_compiler(const function& fn) : _function(fn) {
        for (std::size_t i = 0; i < fn.parameters.size(); ++i) {
            if (!_variables.emplace(fn.parameters[i].name.lexeme(),
                                    static_cast<unsigned>(5 + i)).second)
                throw vhxcc::compile_error("duplicate parameter name '" +
                                           fn.parameters[i].name.lexeme() + "'",
                                           fn.parameters[i].name);
        }

        for (const statement& stmt : fn.body) {
            if (stmt.type != statement::kind::declaration)
                continue;
            if (_variables.contains(stmt.name))
                throw vhxcc::compile_error("redefinition of variable '" + stmt.name + "'", stmt.source_token);
            if (_variables.size() >= 8)
                throw vhxcc::compile_error("functions may use at most eight parameters and local variables",
                                           stmt.source_token);
            _variables.emplace(stmt.name, static_cast<unsigned>(5 + _variables.size()));
        }
    }

    generated_function compile() {
        bool returned = false;
        for (const statement& stmt : _function.body) {
            switch (stmt.type) {
                case statement::kind::empty:
                    break;
                case statement::kind::declaration: {
                    const unsigned reg = _variables.at(stmt.name);
                    if (stmt.expr) {
                        value init = compile_expression(*stmt.expr);
                        emit_move(reg, init.reg, stmt.source_token);
                        release(init);
                    } else {
                        emit_move(reg, 0, stmt.source_token);
                    }
                    break;
                }
                case statement::kind::expression: {
                    value result = compile_expression(*stmt.expr);
                    release(result);
                    break;
                }
                case statement::kind::return_value: {
                    if (_function.returns_value) {
                        if (!stmt.expr)
                            throw vhxcc::compile_error("non-void function must return a value", stmt.source_token);
                        value result = compile_expression(*stmt.expr);
                        emit_move(5, result.reg, stmt.source_token);
                        release(result);
                    } else if (stmt.expr) {
                        throw vhxcc::compile_error("void function cannot return a value", stmt.source_token);
                    }
                    emit("brlr", {}, stmt.source_token);
                    returned = true;
                    break;
                }
            }
        }
        if (!returned) {
            if (_function.returns_value)
                throw vhxcc::compile_error("non-void function must end with a return statement", _function.name);
            emit("brlr", {}, _function.name);
        }
        return {std::move(_code), std::move(_assembly)};
    }
};

static void parse_options(int argc, char* argv[], options& opts) {
    auto take_value = [&](int& index, const std::string& option) -> std::string {
        if (index + 1 >= argc)
            throw std::runtime_error("missing argument after " + option);
        return argv[++index];
    };
    auto split_commas = [](const std::string& text, std::vector<std::string>& output) {
        std::size_t begin = 0;
        while (begin <= text.size()) {
            const std::size_t end = text.find(',', begin);
            if (end == std::string::npos) {
                if (begin < text.size())
                    output.push_back(text.substr(begin));
                break;
            }
            if (end > begin)
                output.push_back(text.substr(begin, end - begin));
            begin = end + 1;
        }
    };

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-help") {
            opts.help = true;
        } else if (arg == "--version" || arg == "-version") {
            opts.version = true;
        } else if (arg == "--target-help") {
            std::cout << "VHX C compiler target options:\n  -march=ARCH\n";
            std::exit(0);
        } else if (arg == "-c") {
            opts.compile_only = true;
        } else if (arg == "-E") {
            opts.preprocess_only = true;
        } else if (arg == "-S") {
            opts.assembly_only = true;
        } else if (arg == "-fsyntax-only") {
            opts.syntax_only = true;
        } else if (arg == "-save-temps" || arg == "--save-temps") {
            opts.save_temps = true;
        } else if (arg == "-pipe") {
            // intentionally keeps intermediates in files for now.
        } else if (arg == "-v") {
            opts.verbose = true;
        } else if (arg == "-o") {
            if (++i >= argc)
                throw std::runtime_error("missing output file after -o");
            opts.output = argv[i];
            opts.output_set = true;
        } else if (arg.rfind("-o", 0) == 0 && arg.size() > 2) {
            opts.output = arg.substr(2);
            opts.output_set = true;
        } else if (arg == "-x") {
            opts.language = take_value(i, arg);
            if (opts.language != "c" && opts.language != "assembler" &&
                opts.language != "none")
                throw std::runtime_error("unsupported input language: " + opts.language);
        } else if (arg.rfind("-Wp,", 0) == 0) {
            split_commas(arg.substr(4), opts.preprocessor_args);
        } else if (arg.rfind("-Wa,", 0) == 0) {
            split_commas(arg.substr(4), opts.assembler_args);
        } else if (arg.rfind("-Wl,", 0) == 0) {
            split_commas(arg.substr(4), opts.linker_args);
        } else if (arg == "-Xpreprocessor") {
            opts.preprocessor_args.push_back(take_value(i, arg));
        } else if (arg == "-Xassembler") {
            opts.assembler_args.push_back(take_value(i, arg));
        } else if (arg == "-Xlinker") {
            opts.linker_args.push_back(take_value(i, arg));
        } else if (arg == "-I" || arg == "-D" || arg == "-U" ||
                   arg == "-include" || arg == "-isystem" || arg == "-iquote") {
            opts.preprocessor_args.push_back(arg);
            opts.preprocessor_args.push_back(take_value(i, arg));
        } else if ((arg.rfind("-I", 0) == 0 && arg.size() > 2) ||
                   (arg.rfind("-D", 0) == 0 && arg.size() > 2) ||
                   (arg.rfind("-U", 0) == 0 && arg.size() > 2) ||
                   arg.rfind("-std=", 0) == 0 ||
                   arg.rfind("-W", 0) == 0 ||
                   arg.rfind("-f", 0) == 0) {
            opts.preprocessor_args.push_back(arg);
        } else if (arg.rfind("-march=", 0) == 0) {
            opts.arch = arg.substr(7);
            if (opts.arch.empty())
                throw std::runtime_error("missing architecture after -march=");
        } else if (arg == "-march") {
            opts.arch = take_value(i, arg);
        } else if (arg == "-g" || arg == "-O0" || arg == "-O1" ||
                   arg == "-O2" || arg == "-O3" || arg == "-Os" ||
                   arg == "-w") {
            if (arg == "-w")
                opts.assembler_args.push_back(arg);
        } else if (!arg.empty() && arg.front() == '-') {
            throw std::runtime_error("unsupported option: " + arg);
        } else {
            opts.inputs.push_back({arg, opts.language});
        }
    }
    if (opts.preprocess_only && (opts.assembly_only || opts.compile_only))
        throw std::runtime_error("-E cannot be combined with -S or -c");
}

static std::string quote_argument(const std::string& value) {
#ifdef _WIN32
    std::string quoted = "\"";
    std::size_t backslashes = 0;
    for (char c : value) {
        if (c == '\\') {
            ++backslashes;
        } else if (c == '"') {
            quoted.append(backslashes * 2 + 1, '\\');
            quoted += '"';
            backslashes = 0;
        } else {
            quoted.append(backslashes, '\\');
            backslashes = 0;
            quoted += c;
        }
    }
    quoted.append(backslashes * 2, '\\');
    quoted += '"';
    return quoted;
#else
    std::string quoted = "'";
    for (char c : value) {
        if (c == '\'')
            quoted += "'\\''";
        else
            quoted += c;
    }
    quoted += '\'';
    return quoted;
#endif
}

static int run_process(const std::vector<std::string>& arguments, bool verbose) {
    if (arguments.empty())
        throw std::runtime_error("attempted to run an empty command");
    if (verbose) {
        std::cerr << "vhx-cc: ";
        for (const std::string& arg : arguments)
            std::cerr << quote_argument(arg) << ' ';
        std::cerr << '\n';
    }

#ifdef _WIN32
    std::string command_line;
    for (const std::string& arg : arguments) {
        if (!command_line.empty())
            command_line += ' ';
        command_line += quote_argument(arg);
    }
    std::vector<char> mutable_command(command_line.begin(), command_line.end());
    mutable_command.push_back('\0');
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessA(nullptr, mutable_command.data(), nullptr, nullptr, TRUE, 0,
                        nullptr, nullptr, &startup, &process))
        throw std::runtime_error("could not start " + arguments.front() +
                                 " (Windows error " + std::to_string(GetLastError()) + ")");
    CloseHandle(process.hThread);
    const DWORD wait_result = WaitForSingleObject(process.hProcess, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        const DWORD error = GetLastError();
        CloseHandle(process.hProcess);
        throw std::runtime_error("failed waiting for " + arguments.front() +
                                 " (Windows error " + std::to_string(error) + ")");
    }
    DWORD exit_code = 0;
    if (!GetExitCodeProcess(process.hProcess, &exit_code)) {
        const DWORD error = GetLastError();
        CloseHandle(process.hProcess);
        throw std::runtime_error("could not read exit status from " + arguments.front() +
                                 " (Windows error " + std::to_string(error) + ")");
    }
    CloseHandle(process.hProcess);
    return static_cast<int>(exit_code);
#else
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (const std::string& arg : arguments)
        argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);
    pid_t process = 0;
    const int spawn_error = posix_spawnp(&process, argv.front(), nullptr, nullptr,
                                         argv.data(), environ);
    if (spawn_error != 0)
        throw std::runtime_error("could not start " + arguments.front() + ": " +
                                 std::strerror(spawn_error));
    int status = 0;
    while (waitpid(process, &status, 0) < 0) {
        if (errno == EINTR)
            continue;
        throw std::runtime_error("failed waiting for " + arguments.front() + ": " +
                                 std::strerror(errno));
    }
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return 1;
#endif
}

static std::string configured_program(const char* variable, const std::string& fallback) {
    if (const char* value = std::getenv(variable); value != nullptr && value[0] != '\0')
        return value;
    return fallback;
}

class temporary_directory {
    std::filesystem::path _path;
    bool _keep;

public:
    explicit temporary_directory(bool keep) : _keep(keep) {
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        std::mt19937_64 random(static_cast<uint64_t>(timestamp));
        for (unsigned attempt = 0; attempt < 64; ++attempt) {
            _path = std::filesystem::temp_directory_path() /
                    ("vhxcc-" + std::to_string(timestamp) + "-" +
                     std::to_string(random()));
            std::error_code error;
            if (std::filesystem::create_directory(_path, error))
                return;
            if (error && error != std::errc::file_exists)
                throw std::runtime_error("could not create temporary directory: " +
                                         error.message());
        }
        throw std::runtime_error("could not create a unique temporary directory");
    }

    ~temporary_directory() {
        if (!_keep) {
            std::error_code error;
            std::filesystem::remove_all(_path, error);
        }
    }

    const std::filesystem::path& path() const { return _path; }
};

static std::filesystem::path tool_path(const std::string& program) {
    const std::filesystem::path candidate(program);
    if (candidate.has_parent_path() || std::filesystem::exists(candidate))
        return std::filesystem::absolute(candidate);
#ifdef _WIN32
    if (!candidate.has_extension())
        return program + ".exe";
#endif
    return candidate;
}

static std::string input_language(const options::input& input) {
    if (input.language != "none")
        return input.language;
    const std::string extension = input.path.extension().string();
    if (extension == ".c" || extension == ".i")
        return "c";
    if (extension == ".s" || extension == ".S")
        return "assembler";
    if (extension == ".o" || extension == ".vxo")
        return "object";
    return "unknown";
}

static std::filesystem::path default_output_for_input(
    const std::filesystem::path& input, const std::string& extension) {
    const std::string input_text = input.string();
    const std::size_t separator = input_text.find_last_of("/\\");
    std::filesystem::path output = separator == std::string::npos
        ? std::filesystem::path(input_text)
        : std::filesystem::path(input_text.substr(separator + 1));
    output.replace_extension(extension);
    return output;
}

static std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot open intermediate file: " + path.string());
    std::string contents((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
    if (input.bad())
        throw std::runtime_error("failed reading intermediate file: " + path.string());
    return contents;
}

static void check_process(const std::vector<std::string>& arguments, bool verbose,
                          const std::string& stage) {
    const int status = run_process(arguments, verbose);
    if (status != 0)
        throw std::runtime_error(stage + " failed with exit status " +
                                 std::to_string(status));
}

static std::string internal_name(const std::string& name, std::size_t input_index) {
    return "Lcc_" + std::to_string(input_index) + "_" + name;
}

struct source_unit {
    std::filesystem::path path;
    std::string text;
};

static void append_preprocessed_source(vhxcc::source& source,
                                       const std::filesystem::path& fallback_path,
                                       const std::string& contents) {
    std::string current_path = fallback_path.string();
    std::size_t next_line = 1;
    std::size_t start = 0;
    while (start < contents.size()) {
        const std::size_t end = contents.find('\n', start);
        const std::size_t line_end = end == std::string::npos ? contents.size() : end;
        std::string_view line(contents.data() + start, line_end - start);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);

        if (line.size() > 2 && line.front() == '#') {
            std::size_t digit = 1;
            while (digit < line.size() && std::isspace(static_cast<unsigned char>(line[digit])))
                ++digit;
            if (digit < line.size() && std::isdigit(static_cast<unsigned char>(line[digit]))) {
                std::size_t number_end = digit;
                while (number_end < line.size() &&
                       std::isdigit(static_cast<unsigned char>(line[number_end])))
                    ++number_end;
                std::size_t quote = line.find('"', number_end);
                if (quote != std::string_view::npos) {
                    std::string filename;
                    ++quote;
                    while (quote < line.size() && line[quote] != '"') {
                        if (line[quote] == '\\' && quote + 1 < line.size())
                            ++quote;
                        filename += line[quote++];
                    }
                    try {
                        next_line = static_cast<std::size_t>(
                            std::stoull(std::string(line.substr(digit, number_end - digit))));
                    } catch (const std::exception&) {
                        throw std::runtime_error("invalid preprocessor line marker");
                    }
                    current_path = std::move(filename);
                    start = end == std::string::npos ? contents.size() : end + 1;
                    continue;
                }
            }
        }

        source.append(vhxcc::source::file(current_path, next_line, 1));
        if (!line.empty())
            source.append(std::string(line));
        if (end != std::string::npos)
            source.append('\n');
        ++next_line;
        start = end == std::string::npos ? contents.size() : end + 1;
    }
}

static std::string compile(const std::vector<source_unit>& inputs) {
    if (inputs.empty())
        throw std::runtime_error("no C input files");

    std::string assembly = ".text\n";

    std::unordered_map<std::string, bool> definitions;
    std::unordered_set<std::string> declarations;

    for (std::size_t file_index = 0; file_index < inputs.size(); ++file_index) {
        typedef_names.clear();
        const source_unit& input = inputs[file_index];
        vhxcc::source source;
        append_preprocessed_source(source, input.path, input.text);
        lexer tokens(source);
        lexer::iterator it = tokens.begin();

        while (*it != token::eof) {
            function fn = parse_external_declaration(it);
            if (fn.typedef_declaration)
                continue;
            const std::string external_name = fn.name.lexeme();
            const std::string output_name = fn.is_global
                ? external_name : internal_name(external_name, file_index);
            if (!fn.definition) {
                if (fn.is_global)
                    declarations.insert(external_name);
                continue;
            }

            if (definitions.contains(output_name))
                throw vhxcc::compile_error("multiple definition of function '" + external_name + "'",
                                           fn.name);

            function_compiler compiler(fn);
            generated_function generated = compiler.compile();
            definitions.emplace(output_name, fn.is_global);
            if (fn.is_global)
                assembly += ".global " + output_name + '\n';
            assembly += output_name + ":\n" + generated.assembly;
        }
    }
    (void)declarations;
    return assembly;
}

static int run_driver(const options& opts) {
    if (opts.inputs.empty())
        throw std::runtime_error("no input files");
    vhxcc::first_file = opts.inputs.front().path.string();
    if ((opts.preprocess_only || opts.assembly_only || opts.compile_only) &&
        opts.inputs.size() != 1)
        throw std::runtime_error("-E, -S, and -c currently require exactly one input file");
    if (opts.syntax_only && opts.output_set)
        throw std::runtime_error("-fsyntax-only cannot be combined with -o");

    std::vector<std::string> languages;
    languages.reserve(opts.inputs.size());
    bool has_c_input = false;
    for (const options::input& input : opts.inputs) {
        const std::string language = input_language(input);
        if (language == "unknown")
            throw std::runtime_error("cannot determine input language for " + input.path.string() +
                                     "; use -x c or -x assembler");
        if (language == "c")
            has_c_input = true;
        if (opts.preprocess_only && language != "c")
            throw std::runtime_error("-E currently accepts C source files only");
        languages.push_back(language);
    }

    if (opts.preprocess_only) {
        std::vector<std::string> command = {
            configured_program("VHXC_CPP", "gcc"), "-E", "-x", "c"
        };
        command.insert(command.end(), opts.preprocessor_args.begin(),
                       opts.preprocessor_args.end());
        if (opts.output_set) {
            command.push_back("-o");
            command.push_back(opts.output.string());
        }
        command.push_back(opts.inputs.front().path.string());
        const int status = run_process(command, opts.verbose);
        if (status != 0)
            throw std::runtime_error("preprocessor failed with exit status " +
                                     std::to_string(status));
        return 0;
    }

    if (opts.assembly_only && (!has_c_input || languages.front() != "c"))
        throw std::runtime_error("-S currently requires a C source file");
    if (opts.syntax_only && !has_c_input)
        throw std::runtime_error("-fsyntax-only requires a C source file");

    temporary_directory temporary(opts.save_temps);
    const std::filesystem::path cpp_program =
        tool_path(configured_program("VHXC_CPP", "gcc"));
    std::vector<source_unit> sources;
    std::vector<std::filesystem::path> objects;

    for (std::size_t i = 0; i < opts.inputs.size(); ++i) {
        if (languages[i] != "c")
            continue;
        const std::filesystem::path preprocessed =
            temporary.path() / ("input-" + std::to_string(i) + ".i");
        std::vector<std::string> command = {
            cpp_program.string(), "-E", "-x", "c", "-o", preprocessed.string()
        };
        command.insert(command.end(), opts.preprocessor_args.begin(),
                       opts.preprocessor_args.end());
        command.push_back(opts.inputs[i].path.string());
        check_process(command, opts.verbose, "preprocessor");
        sources.push_back({opts.inputs[i].path, read_text_file(preprocessed)});
    }

    if (has_c_input) {
        const std::string assembly = compile(sources);
        if (opts.syntax_only) {
            if (vhxcc::errored)
                return 1;
            if (opts.save_temps)
                std::cerr << "vhx-cc: intermediate files retained in "
                          << temporary.path().string() << '\n';
            return 0;
        }
        if (opts.assembly_only) {
            std::filesystem::path output = opts.output;
            if (!opts.output_set)
                output = default_output_for_input(opts.inputs.front().path, ".s");
            std::ofstream stream(output, std::ios::binary);
            if (!stream)
                throw std::runtime_error("cannot open output file: " + output.string());
            stream << assembly;
            if (!stream)
                throw std::runtime_error("failed writing output file: " + output.string());
            if (opts.save_temps)
                std::cerr << "vhx-cc: intermediate files retained in "
                          << temporary.path().string() << '\n';
            return 0;
        }

        const std::filesystem::path assembly_path = temporary.path() / "output.s";
        const std::filesystem::path object_path = temporary.path() / "output.vxo";
        {
            std::ofstream stream(assembly_path, std::ios::binary);
            if (!stream)
                throw std::runtime_error("cannot create intermediate assembly file");
            stream << assembly;
            if (!stream)
                throw std::runtime_error("failed writing intermediate assembly file");
        }

        std::vector<std::string> command = {
            tool_path(configured_program("VHXC_AS", "vhx64-none-vxo-as")).string(),
            "-c", "-march=" + opts.arch
        };
        command.insert(command.end(), opts.assembler_args.begin(),
                       opts.assembler_args.end());
        command.push_back("-o");
        command.push_back(object_path.string());
        command.push_back(assembly_path.string());
        check_process(command, opts.verbose, "assembler");
        objects.push_back(object_path);
    }

    for (std::size_t i = 0; i < opts.inputs.size(); ++i) {
        if (languages[i] == "c")
            continue;
        if (languages[i] == "object") {
            objects.push_back(opts.inputs[i].path);
            continue;
        }
        const std::filesystem::path object_path =
            temporary.path() / ("input-" + std::to_string(i) + ".vxo");
        std::vector<std::string> command = {
            tool_path(configured_program("VHXC_AS", "vhx64-none-vxo-as")).string(),
            "-c", "-march=" + opts.arch
        };
        command.insert(command.end(), opts.assembler_args.begin(),
                       opts.assembler_args.end());
        command.push_back("-o");
        command.push_back(object_path.string());
        command.push_back(opts.inputs[i].path.string());
        check_process(command, opts.verbose, "assembler");
        objects.push_back(object_path);
    }

    if (opts.syntax_only) {
        if (vhxcc::errored)
            return 1;
        return 0;
    }

    if (opts.compile_only) {
        std::filesystem::path output = opts.output;
        if (!opts.output_set)
            output = default_output_for_input(opts.inputs.front().path, ".vxo");
        if (objects.size() != 1)
            throw std::runtime_error("-c requires one object output");
        if (!std::filesystem::copy_file(objects.front(), output,
                                        std::filesystem::copy_options::overwrite_existing))
            throw std::runtime_error("could not write object file: " + output.string());
    } else {
        const std::filesystem::path output = opts.output;
        std::vector<std::string> command = {
            tool_path(configured_program("VHXC_LD", "vhx64-none-vxo-ld")).string(),
            "-o", output.string()
        };
        command.insert(command.end(), opts.linker_args.begin(), opts.linker_args.end());
        for (const std::filesystem::path& object : objects)
            command.push_back(object.string());
        check_process(command, opts.verbose, "linker");
    }

    if (opts.save_temps)
        std::cerr << "vhx-cc: intermediate files retained in "
                  << temporary.path().string() << '\n';
    return vhxcc::errored ? 1 : 0;
}

}

int main(int argc, char* argv[]) {
    try {
        options opts;
        parse_options(argc, argv, opts);
        if (opts.help) {
            std::cout << help << '\n';
            return 0;
        }
        if (opts.version) {
            std::cout << version << '\n';
            return 0;
        }
        return run_driver(opts);
    } catch (const vhxcc::compile_error& error) {
        const vhxcc::token& tok = error.token();
        if (tok.pos().line != 0)
            vhxcc::error(error.what(), tok);
        else
            vhxcc::error(error.what());
        return 1;
    } catch (const std::exception& error) {
        vhxcc::error(error.what());
        return 1;
    }
}
