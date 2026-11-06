#include <isa.hpp>
#include <lexer.hpp>
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

namespace vhxcc {

static bool errored = false;
static std::string first_file;

void error(std::string_view message) {
    std::cerr << "\033[1m" << first_file << ": \033[31merror:\033[0m "
              << message << '\n';
    errored = true;
}

void error(std::string_view message, const token& tok) {
    std::cerr << "\033[1m" << tok.pos().file << ':' << tok.pos().line << ':'
              << tok.pos().column << ": \033[31merror:\033[0m " << message << '\n';
    errored = true;
}

void warn(std::string_view message, const token& tok) {
    std::cerr << "\033[1m" << tok.pos().file << ':' << tok.pos().line << ':'
              << tok.pos().column << ": \033[35mwarning:\033[0m " << message << '\n';
}

}

namespace {

using vhxcc::lexer;
using vhxcc::parser;
using vhxcc::token;

const std::string help = R"(Usage: vhx64-none-vxo-gcc [options] file...
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
  typedef aliases, scalar stack locals, up to eight register-passed parameters
  and call arguments, function calls, if/else, while, and integer expressions.
  Scalar arithmetic includes +, -, *, /, %, &, |, ^, <<, >> and comparisons.
  Preprocessing, assembly, and linking are delegated to separate programs.

Pointer and aggregate types, global variables, explicit casts, for/do/switch,
break/continue, complete C integer promotions, and full ABI compatibility are
not implemented yet. Stack-frame size and scalar type conversions are limited.

Report bugs to <https://github.com/Flakwy/vhxcc/issues/>)";

const std::string version =
    "VHX C compiler (vhxcc for " HOSTNAME ") " VERSION "\n"
    "Copyright (C) 2026 Free Software Foundation Inc.\n"
    "This program is free software; you may redistribute it under the terms of\n"
    "the GNU General Public License version 3 or later.\n"
    "This program has absolutely no warranty.";

struct type {
    enum class kind { none, void_, char_, short_, int_, long_, long_long_ } width = kind::none;
    bool unsigned_ = false;
};

struct storage {
    enum class kind { none, register_, stack, static_ } type = kind::none;
    std::size_t offset = 0;
};

struct variable_storage {
    type type_;
    storage storage_;
};

static std::unordered_map<std::string, type> typedef_names;

struct options {
    struct input {
        std::filesystem::path path;
        std::string language = "none";
    };
    std::vector<input> inputs;
    std::filesystem::path output = "a.out";
    std::string arch = "vhx64imsa";
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
};

struct statement {
    enum class kind {
        return_value,
        declaration,
        expression,
        empty,
        block,
        if_,
        while_
    } type = kind::empty;

    token source_token;
    std::string name;
    struct type declared_type;
    std::optional<parser::expression> expr;
    std::vector<statement> body;
    std::vector<statement> else_body;
    std::size_t stack_offset = 0;
};

struct parameter {
    token name;
    type type_;
};

struct function {
    token name;
    type return_type;
    bool is_global = true;
    bool returns_value = true;
    bool definition = false;
    bool typedef_declaration = false;
    type typedef_type;
    std::vector<parameter> parameters;
    std::vector<statement> body;
};

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

static void initialize_typedef_names() {
    initialize_typedef_names();
    typedef_names.emplace("void", type{type::kind::void_, false});
    typedef_names.emplace("char", type{type::kind::char_, false});
    typedef_names.emplace("_Bool", type{type::kind::char_, true});
    typedef_names.emplace("short", type{type::kind::short_, false});
    typedef_names.emplace("int", type{type::kind::int_, false});
    typedef_names.emplace("long", type{type::kind::long_, false});
}

static type parse_type_specifier(lexer::iterator& it) {
    type result;
    bool saw_any = false;
    bool saw_unsigned = false;
    bool saw_signed = false;
    unsigned long_count = 0;
    bool saw_short = false;
    bool saw_char = false;
    bool saw_void = false;

    while (is_type_word(*it)) {
        const std::string& word = it->lexeme();
        if (typedef_names.contains(word) && word != "void" && word != "char" &&
            word != "_Bool" && word != "short" && word != "int" && word != "long") {
            if (saw_any)
                throw vhxcc::compile_error("invalid combination of type specifiers", *it);
            result = typedef_names.at(word);
            ++it;
            return result;
        }
        if (word == "unsigned") {
            if (saw_unsigned || saw_signed)
                throw vhxcc::compile_error("invalid combination of type specifiers", *it);
            saw_unsigned = true;
        } else if (word == "signed") {
            if (saw_signed || saw_unsigned)
                throw vhxcc::compile_error("invalid combination of type specifiers", *it);
            saw_signed = true;
        } else if (word == "void") {
            if (saw_any)
                throw vhxcc::compile_error("invalid combination of type specifiers", *it);
            saw_void = true;
        } else if (word == "char") {
            if (saw_char || saw_short || long_count != 0 || saw_void)
                throw vhxcc::compile_error("invalid combination of type specifiers", *it);
            saw_char = true;
        } else if (word == "_Bool") {
            if (saw_any)
                throw vhxcc::compile_error("invalid combination of type specifiers", *it);
            saw_char = true;
            saw_unsigned = true;
        } else if (word == "short") {
            if (saw_short || saw_char || long_count != 0 || saw_void)
                throw vhxcc::compile_error("invalid combination of type specifiers", *it);
            saw_short = true;
        } else if (word == "long") {
            if (long_count == 2 || saw_short || saw_char || saw_void)
                throw vhxcc::compile_error("invalid combination of type specifiers", *it);
            ++long_count;
        } else if (word == "int" && (saw_char || saw_void)) {
            throw vhxcc::compile_error("invalid combination of type specifiers", *it);
        }
        saw_any = true;
        ++it;
    }
    if (!saw_any)
        throw vhxcc::compile_error("expected an integer type", *it);
    result.width = saw_void ? type::kind::void_ :
                   saw_char ? type::kind::char_ :
                   saw_short ? type::kind::short_ :
                   long_count == 2 ? type::kind::long_long_ :
                   long_count == 1 ? type::kind::long_ : type::kind::int_;
    result.unsigned_ = saw_unsigned;
    return result;
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
    if (*it == token::lcurly) {
        result.type = statement::kind::block;
        ++it;
        while (*it != token::rcurly) {
            if (*it == token::eof)
                throw vhxcc::compile_error("unterminated compound statement", result.source_token);
            result.body.push_back(parse_statement(it));
        }
        ++it;
        return result;
    }
    if (it->type() == token::identifier) {
        const std::string keyword = it->lexeme();
        if (keyword == "if") {
            result.type = statement::kind::if_;
            ++it;
            expect_token(it, token::lparen, "expected '(' after if");
            result.expr = parse_expression(it);
            expect_token(it, token::rparen, "expected ')' after if condition");
            result.body.push_back(parse_statement(it));
            if (it->type() == token::identifier && it->lexeme() == "else") {
                ++it;
                result.else_body.push_back(parse_statement(it));
            }
            return result;
        }
        if (keyword == "while") {
            result.type = statement::kind::while_;
            ++it;
            expect_token(it, token::lparen, "expected '(' after while");
            result.expr = parse_expression(it);
            expect_token(it, token::rparen, "expected ')' after while condition");
            result.body.push_back(parse_statement(it));
            return result;
        }
        if (keyword == "do" || keyword == "for" || keyword == "switch" ||
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
        const type declared_type = parse_type_specifier(it);
        if (declared_type.width == type::kind::void_)
            throw vhxcc::compile_error("variables of type void are not allowed", *it);
        result.type = statement::kind::declaration;
        result.declared_type = declared_type;
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

    const type return_type = parse_type_specifier(it);
    if (is_typedef) {
        result.name = expect_token(it, token::identifier, "expected typedef name");
        expect_token(it, token::semicolon, "expected ';' after typedef declaration");
        if (typedef_names.contains(result.name.lexeme()))
            throw vhxcc::compile_error("redefinition of typedef '" +
                                       result.name.lexeme() + "'", result.name);
        result.typedef_declaration = true;
        result.typedef_type = return_type;
        typedef_names.emplace(result.name.lexeme(), return_type);
        return result;
    }
    result.return_type = return_type;
    result.returns_value = return_type.width != type::kind::void_;
    result.is_global = !is_static;
    result.name = expect_token(it, token::identifier, "expected function name");
    expect_token(it, token::lparen, "expected '(' after function name");

    if (!lexer::match(it, token::rparen)) {
        if (it->type() == token::identifier && it->lexeme() == "void") {
            ++it;
            expect_token(it, token::rparen, "expected ')' after void parameter list");
        } else {
            while (true) {
                const type parameter_type = parse_type_specifier(it);
                if (parameter_type.width == type::kind::void_)
                    throw vhxcc::compile_error("void parameter must be the entire parameter list", *it);
                token parameter_name;
                if (it->type() == token::identifier)
                    parameter_name = *it++;
                else
                    throw vhxcc::compile_error("expected parameter name", *it);
                result.parameters.push_back({std::move(parameter_name), parameter_type});
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

static parser::expression register_expression(unsigned reg, const token& source) {
    parser::expression expr;
    expr.type = parser::expression::identifier;
    expr.token = token(token::identifier, "x" + std::to_string(reg), source.pos());
    return expr;
}

static parser::expression constant_expression(uint64_t value, const token& source) {
    parser::expression expr;
    expr.type = parser::expression::literal;
    std::string text = std::to_string(value);
    if (value > static_cast<uint64_t>((std::numeric_limits<int64_t>::max)())) {
        const uint64_t magnitude = ~value + 1;
        text = magnitude == (uint64_t{1} << 63)
            ? "-9223372036854775808" : "-" + std::to_string(magnitude);
    }
    expr.token = token(token::integer_literal, std::move(text), source.pos());
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
    const std::unordered_map<std::string, std::string>& _function_names;
    std::vector<std::unordered_map<std::string, variable_storage>> _scopes;
    std::array<bool, 8> _temporaries{};
    std::vector<char> _code;
    std::string _assembly;
    std::size_t _word_size;
    std::size_t _local_bytes = 0;
    std::size_t _max_call_depth = 1;
    std::size_t _call_depth = 0;
    std::size_t _frame_size = 0;
    std::size_t _outgoing_offset = 0;
    std::size_t _spill_offset = 0;
    std::size_t _return_address_offset = 0;
    std::vector<std::size_t> _parameter_offsets;
    std::string _epilogue_label;
    std::size_t _next_label = 0;
    bool _saw_return = false;

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
        if (!encoded.is_valid || encoded.bytes.size() != encoded.size)
            throw vhxcc::compile_error("internal error encoding instruction '" + mnemonic + "'", source);
        _code.insert(_code.end(), encoded.bytes.begin(), encoded.bytes.end());
        _assembly += "    " + mnemonic;
        if (!operands.empty())
            _assembly += ' ';
        for (std::size_t i = 0; i < operands.size(); ++i) {
            if (i != 0)
                _assembly += ", ";
            if (operands[i].type == parser::expression::call &&
                operands[i].branches.size() == 2) {
                _assembly += operands[i].branches[0].token.lexeme();
                _assembly += '(';
                _assembly += operands[i].branches[1].token.lexeme();
                _assembly += ')';
            } else {
                _assembly += operands[i].token.lexeme();
            }
        }
        _assembly += '\n';
    }

    void emit_rr(const std::string& mnemonic, unsigned rd, unsigned rm, unsigned rn,
                 const token& source) {
        emit(mnemonic, {register_expression(rd, source),
                        register_expression(rm, source),
                        register_expression(rn, source)}, source);
    }

    void emit_cmove(const std::string& mnemonic, unsigned rd, unsigned rn,
                    unsigned rl, unsigned rm, const token& source) {
        emit(mnemonic, {register_expression(rd, source),
                        register_expression(rn, source),
                        register_expression(rl, source),
                        register_expression(rm, source)}, source);
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

    parser::expression memory_operand(std::size_t offset, const token& source) {
        parser::expression memory;
        memory.type = parser::expression::call;
        memory.token = token(token::lparen, "(", source.pos());
        memory.branches.push_back(constant_expression(offset, source));
        memory.branches.push_back(register_expression(2, source));
        return memory;
    }

    void emit_memory(const std::string& mnemonic, unsigned reg, std::size_t offset,
                     const token& source) {
        emit(mnemonic, {register_expression(reg, source), memory_operand(offset, source)},
             source);
    }

    void emit_address(unsigned reg, const std::string& symbol, const token& source) {
        parser::expression target;
        target.type = parser::expression::identifier;
        target.token = token(token::identifier, symbol, source.pos());
        emit(_word_size == 4 ? "la32" : "la",
             {register_expression(reg, source), target}, source);
    }

    std::size_t type_size(const type& value) const {
        switch (value.width) {
            case type::kind::char_: return 1;
            case type::kind::short_: return 2;
            case type::kind::int_: return 4;
            case type::kind::long_:
                return _word_size == 4 ? 4 : 8;
            case type::kind::long_long_:
                if (_word_size == 4)
                    throw vhxcc::compile_error("long long is unsupported for the selected architecture",
                                               _function.name);
                return 8;
            case type::kind::void_:
            case type::kind::none:
                break;
        }
        throw vhxcc::compile_error("incomplete or unsupported object type",
                                   _function.name);
    }

    std::string load_mnemonic(const type& value) const {
        switch (type_size(value)) {
            case 1: return "lb";
            case 2: return "lh";
            case 4: return "lw";
            case 8: return "ld";
            default: break;
        }
        throw vhxcc::compile_error("unsupported load width", _function.name);
    }

    std::string store_mnemonic(const type& value) const {
        switch (type_size(value)) {
            case 1: return "sb";
            case 2: return "sh";
            case 4: return "sw";
            case 8: return "sd";
            default: break;
        }
        throw vhxcc::compile_error("unsupported store width", _function.name);
    }

    void normalize_register(unsigned reg, const type& value, const token& source) {
        const std::size_t size = type_size(value);
        if (size >= _word_size)
            return;
        std::string mnemonic;
        if (value.unsigned_) {
            if (size == 1) mnemonic = "zxtb";
            else if (size == 2) mnemonic = "zxth";
            else mnemonic = "zxtw";
        } else {
            if (size == 1) mnemonic = "sxtb";
            else if (size == 2) mnemonic = "sxth";
            else mnemonic = "sxtw";
        }
        emit(mnemonic, {register_expression(reg, source),
                        register_expression(reg, source)}, source);
    }

    void emit_label(const std::string& label) {
        _assembly += label + ":\n";
    }

    void emit_jump(const std::string& label, const token& source) {
        emit_address(30, label, source);
        emit("bl", {register_expression(30, source)}, source);
    }

    std::string make_label(const std::string& stem) {
        return "Lcc_" + _epilogue_label + "_" + stem + "_" +
               std::to_string(_next_label++);
    }

    const variable_storage& lookup_variable(const std::string& name,
                                            const token& source) const {
        for (auto scope = _scopes.rbegin(); scope != _scopes.rend(); ++scope) {
            const auto found = scope->find(name);
            if (found != scope->end())
                return found->second;
        }
        throw vhxcc::compile_error("unknown variable '" + name + "'", source);
    }

    bool expression_is_unsigned(const parser::expression& expr) const {
        if (expr.type == parser::expression::identifier)
            return lookup_variable(expr.token.lexeme(), expr.token).type_.unsigned_;
        if (expr.type == parser::expression::literal)
            return expr.token.lexeme().find_first_of("uU") != std::string::npos;
        if (expr.type == parser::expression::unary && !expr.branches.empty())
            return expression_is_unsigned(expr.branches.front());
        if (expr.type == parser::expression::binary && expr.branches.size() == 2)
            return expression_is_unsigned(expr.branches[0]) ||
                   expression_is_unsigned(expr.branches[1]);
        return false;
    }

    static std::size_t align_up(std::size_t value, std::size_t alignment) {
        const std::size_t remainder = value % alignment;
        return remainder == 0 ? value : value + alignment - remainder;
    }

    void allocate_local_slots(std::vector<statement>& statements,
                              std::size_t& offset) {
        for (statement& stmt : statements) {
            if (stmt.type == statement::kind::declaration) {
                const std::size_t size = type_size(stmt.declared_type);
                offset = align_up(offset, std::min(size, _word_size));
                stmt.stack_offset = offset;
                offset += size;
            }
            allocate_local_slots(stmt.body, offset);
            allocate_local_slots(stmt.else_body, offset);
        }
    }

    static std::size_t expression_call_depth(const parser::expression& expr) {
        std::size_t nested = 0;
        for (const parser::expression& branch : expr.branches)
            nested = std::max(nested, expression_call_depth(branch));
        if (expr.type == parser::expression::call)
            return nested + 1;
        return nested;
    }

    void measure_call_depth(const std::vector<statement>& statements) {
        for (const statement& stmt : statements) {
            if (stmt.expr)
                _max_call_depth = std::max(_max_call_depth,
                                           expression_call_depth(*stmt.expr));
            measure_call_depth(stmt.body);
            measure_call_depth(stmt.else_body);
        }
    }

    void emit_expression_branch(const parser::expression& condition,
                                const std::string& label, const token& source) {
        value cond = compile_expression(condition);
        emit_address(30, label, source);
        emit_cmove("mveq", 31, cond.reg, 0, 30, source);
        release(cond);
    }

    void compile_controlled_statement(const statement& stmt) {
        _scopes.emplace_back();
        compile_statement(stmt);
        _scopes.pop_back();
    }

    void compile_statement(const statement& stmt) {
        switch (stmt.type) {
            case statement::kind::empty:
                return;
            case statement::kind::block:
                _scopes.emplace_back();
                for (const statement& child : stmt.body)
                    compile_statement(child);
                _scopes.pop_back();
                return;
            case statement::kind::declaration: {
                auto& scope = _scopes.back();
                variable_storage variable{
                    stmt.declared_type,
                    storage{storage::kind::stack, stmt.stack_offset}
                };
                if (!scope.emplace(stmt.name, variable).second)
                    throw vhxcc::compile_error("redefinition of variable '" + stmt.name + "'",
                                               stmt.source_token);
                if (stmt.expr) {
                    value initial = compile_expression(*stmt.expr);
                    emit_memory(store_mnemonic(stmt.declared_type), initial.reg,
                                stmt.stack_offset, stmt.source_token);
                    release(initial);
                }
                return;
            }
            case statement::kind::expression: {
                value result = compile_expression(*stmt.expr);
                release(result);
                return;
            }
            case statement::kind::return_value: {
                _saw_return = true;
                if (_function.returns_value) {
                    if (!stmt.expr)
                        throw vhxcc::compile_error("non-void function must return a value",
                                                   stmt.source_token);
                    value result = compile_expression(*stmt.expr);
                    normalize_register(result.reg, _function.return_type,
                                       stmt.source_token);
                    emit_move(5, result.reg, stmt.source_token);
                    release(result);
                } else if (stmt.expr) {
                    throw vhxcc::compile_error("void function cannot return a value",
                                               stmt.source_token);
                }
                emit_jump(_epilogue_label, stmt.source_token);
                return;
            }
            case statement::kind::if_: {
                const std::string else_label = make_label("else");
                const std::string end_label = make_label("endif");
                emit_expression_branch(*stmt.expr, else_label, stmt.source_token);
                compile_controlled_statement(stmt.body.front());
                if (!stmt.else_body.empty()) {
                    emit_jump(end_label, stmt.source_token);
                    emit_label(else_label);
                    compile_controlled_statement(stmt.else_body.front());
                    emit_label(end_label);
                } else {
                    emit_label(else_label);
                }
                return;
            }
            case statement::kind::while_: {
                const std::string start_label = make_label("while");
                const std::string end_label = make_label("endwhile");
                emit_label(start_label);
                emit_expression_branch(*stmt.expr, end_label, stmt.source_token);
                compile_controlled_statement(stmt.body.front());
                emit_jump(start_label, stmt.source_token);
                emit_label(end_label);
                return;
            }
        }
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
            const variable_storage& variable =
                lookup_variable(expr.token.lexeme(), expr.token);
            const unsigned reg = acquire(expr.token);
            emit_memory(load_mnemonic(variable.type_), reg,
                        variable.storage_.offset, expr.token);
            normalize_register(reg, variable.type_, expr.token);
            return {reg, true};
        }

        if (expr.type == parser::expression::unary) {
            if (expr.branches.size() != 1)
                throw vhxcc::compile_error("malformed unary expression", expr.token);
            value operand = compile_expression(expr.branches[0]);
            if (expr.token.type() == token::plus)
                return operand;
            if (expr.token.type() == token::lnot) {
                const unsigned result = acquire(expr.token);
                emit_constant(result, 0, expr.token);
                emit_constant(30, 1, expr.token);
                emit_cmove("mveq", result, operand.reg, 0, 30, expr.token);
                release(operand);
                return {result, true};
            }
            if (expr.token.type() != token::minus) {
                release(operand);
                throw vhxcc::compile_error("this unary operator is not implemented yet", expr.token);
            }
            const unsigned result = acquire(expr.token);
            emit_rr("sub", result, 0, operand.reg, expr.token);
            release(operand);
            return {result, true};
        }

        if (expr.type == parser::expression::call) {
            if (expr.branches.empty() ||
                expr.branches.front().type != parser::expression::identifier)
                throw vhxcc::compile_error("only direct function calls are supported",
                                           expr.token);
            if (expr.branches.size() > 9)
                throw vhxcc::compile_error("functions may have at most eight arguments",
                                           expr.token);
            const std::size_t outgoing = _outgoing_offset +
                                         _call_depth * 8 * _word_size;
            ++_call_depth;
            for (std::size_t i = 1; i < expr.branches.size(); ++i) {
                value argument = compile_expression(expr.branches[i]);
                emit_memory(_word_size == 4 ? "sw" : "sd", argument.reg,
                            outgoing + (i - 1) * _word_size, expr.token);
                release(argument);
            }
            --_call_depth;

            for (std::size_t i = 0; i < _temporaries.size(); ++i) {
                if (_temporaries[i])
                    emit_memory(_word_size == 4 ? "sw" : "sd",
                                static_cast<unsigned>(13 + i),
                                _spill_offset + i * _word_size, expr.token);
            }
            for (std::size_t i = 1; i < expr.branches.size(); ++i) {
                emit_memory(_word_size == 4 ? "lw" : "ld",
                            static_cast<unsigned>(4 + i),
                            outgoing + (i - 1) * _word_size, expr.token);
            }
            const std::string callee = _function_names.contains(expr.branches[0].token.lexeme())
                ? _function_names.at(expr.branches[0].token.lexeme())
                : expr.branches[0].token.lexeme();
            parser::expression target;
            target.type = parser::expression::identifier;
            target.token = token(token::identifier, callee, expr.token.pos());
            emit("brl", {target}, expr.token);
            for (std::size_t i = 0; i < _temporaries.size(); ++i) {
                if (_temporaries[i])
                    emit_memory(_word_size == 4 ? "lw" : "ld",
                                static_cast<unsigned>(13 + i),
                                _spill_offset + i * _word_size, expr.token);
            }
            const unsigned result = acquire(expr.token);
            emit_move(result, 5, expr.token);
            return {result, true};
        }

        if (expr.type != parser::expression::binary || expr.branches.size() != 2)
            throw vhxcc::compile_error("this expression is not implemented yet", expr.token);

        const parser::expression& left_expr = expr.branches[0];
        const parser::expression& right_expr = expr.branches[1];
        const bool assignment = expr.token.type() == token::assign ||
            expr.token.type() == token::plus_assign ||
            expr.token.type() == token::minus_assign ||
            expr.token.type() == token::times_assign ||
            expr.token.type() == token::slash_assign ||
            expr.token.type() == token::modulo_assign ||
            expr.token.type() == token::and_assign ||
            expr.token.type() == token::or_assign ||
            expr.token.type() == token::xor_assign ||
            expr.token.type() == token::lshift_assign ||
            expr.token.type() == token::rshift_assign;
        if (assignment) {
            if (left_expr.type != parser::expression::identifier)
                throw vhxcc::compile_error("assignment target must be a local variable", left_expr.token);
            const variable_storage& variable =
                lookup_variable(left_expr.token.lexeme(), left_expr.token);
            const std::size_t offset = variable.storage_.offset;
            value right = compile_expression(right_expr);
            if (expr.token.type() != token::assign) {
                const unsigned previous = acquire(expr.token);
                emit_memory(load_mnemonic(variable.type_), previous, offset, expr.token);
                normalize_register(previous, variable.type_, expr.token);
                std::string mnemonic;
                switch (expr.token.type()) {
                    case token::plus_assign: mnemonic = "add"; break;
                    case token::minus_assign: mnemonic = "sub"; break;
                    case token::times_assign: mnemonic = "mul"; break;
                    case token::slash_assign:
                    case token::modulo_assign: mnemonic = "div"; break;
                    case token::and_assign: mnemonic = "and"; break;
                    case token::or_assign: mnemonic = "or"; break;
                    case token::xor_assign: mnemonic = "xor"; break;
                    case token::lshift_assign: mnemonic = "sll"; break;
                    case token::rshift_assign: mnemonic = "sra"; break;
                    default:
                        release({previous, true});
                        release(right);
                        throw vhxcc::compile_error("this compound assignment is not implemented yet",
                                                   expr.token);
                }
                if (expr.token.type() == token::times_assign) {
                    emit("mul", {register_expression(previous, expr.token),
                                 register_expression(0, expr.token),
                                 register_expression(previous, expr.token),
                                 register_expression(right.reg, expr.token)}, expr.token);
                } else if (expr.token.type() == token::slash_assign ||
                           expr.token.type() == token::modulo_assign) {
                    const std::string divide = variable.type_.unsigned_ ||
                                               expression_is_unsigned(right_expr)
                        ? "div" : "divs";
                    emit(divide, {register_expression(
                                     expr.token.type() == token::slash_assign ? previous : 30,
                                 expr.token),
                                 register_expression(
                                     expr.token.type() == token::slash_assign ? 0 : previous,
                                 expr.token),
                                 register_expression(previous, expr.token),
                                 register_expression(right.reg, expr.token)}, expr.token);
                } else {
                    emit_rr(mnemonic, previous, previous, right.reg, expr.token);
                }
                release(right);
                right = {previous, true};
            }
            emit_memory(store_mnemonic(variable.type_), right.reg, offset, expr.token);
            emit_memory(load_mnemonic(variable.type_), right.reg, offset, expr.token);
            normalize_register(right.reg, variable.type_, expr.token);
            return right;
        }

        value left = compile_expression(left_expr);
        value right = compile_expression(right_expr);
        const bool comparison =
            expr.token.type() == token::equal || expr.token.type() == token::not_equal ||
            expr.token.type() == token::less_than || expr.token.type() == token::less_equal ||
            expr.token.type() == token::greater_than || expr.token.type() == token::greater_equal;
        std::string mnemonic;
        switch (expr.token.type()) {
            case token::plus: mnemonic = "add"; break;
            case token::minus: mnemonic = "sub"; break;
            case token::times: break;
            case token::slash: break;
            case token::modulo: break;
            case token::_and: mnemonic = "and"; break;
            case token::_or: mnemonic = "or"; break;
            case token::_xor: mnemonic = "xor"; break;
            case token::lshift: mnemonic = "sll"; break;
            case token::rshift: mnemonic = "sra"; break;
            case token::equal:
            case token::not_equal:
            case token::less_than:
            case token::less_equal:
            case token::greater_than:
            case token::greater_equal:
                break;
            default:
                release(left);
                release(right);
                throw vhxcc::compile_error("this binary operator is not implemented yet", expr.token);
        }
        const unsigned result = acquire(expr.token);
        if (comparison) {
            std::string comparison;
            unsigned compare_left = left.reg;
            unsigned compare_right = right.reg;
            switch (expr.token.type()) {
                case token::equal: comparison = "mveq"; break;
                case token::not_equal: comparison = "mvne"; break;
                case token::less_than:
                    comparison = expression_is_unsigned(left_expr) ||
                                 expression_is_unsigned(right_expr) ? "mvlo" : "mvlt";
                    break;
                case token::less_equal:
                    comparison = expression_is_unsigned(left_expr) ||
                                 expression_is_unsigned(right_expr) ? "mvls" : "mvle";
                    break;
                case token::greater_than:
                    comparison = expression_is_unsigned(left_expr) ||
                                 expression_is_unsigned(right_expr) ? "mvlo" : "mvlt";
                    std::swap(compare_left, compare_right);
                    break;
                case token::greater_equal:
                    comparison = expression_is_unsigned(left_expr) ||
                                 expression_is_unsigned(right_expr) ? "mvls" : "mvle";
                    std::swap(compare_left, compare_right);
                    break;
                default: break;
            }
            emit_constant(result, 0, expr.token);
            emit_constant(30, 1, expr.token);
            emit_cmove(comparison, result, compare_left, compare_right, 30, expr.token);
        } else if (expr.token.type() == token::times) {
            emit("mul", {register_expression(result, expr.token),
                         register_expression(0, expr.token),
                         register_expression(left.reg, expr.token),
                         register_expression(right.reg, expr.token)}, expr.token);
        } else if (expr.token.type() == token::slash ||
                   expr.token.type() == token::modulo) {
            const std::string divide = expression_is_unsigned(left_expr) ||
                                       expression_is_unsigned(right_expr) ? "div" : "divs";
            emit(divide, {register_expression(expr.token.type() == token::slash ? result : 30,
                                              expr.token),
                         register_expression(expr.token.type() == token::slash ? 0 : result,
                                              expr.token),
                         register_expression(left.reg, expr.token),
                         register_expression(right.reg, expr.token)}, expr.token);
        } else {
            emit_rr(mnemonic, result, left.reg, right.reg, expr.token);
        }
        release(left);
        release(right);
        return {result, true};
    }

public:
    explicit function_compiler(
        function& fn, const std::unordered_map<std::string, std::string>& function_names,
        const std::string& arch, const std::string& output_name)
        : _function(fn), _function_names(function_names),
          _word_size(arch.starts_with("vhx32") ? 4 : 8),
          _epilogue_label(output_name + "_return") {
        if (!arch.starts_with("vhx32") && !arch.starts_with("vhx64"))
            throw vhxcc::compile_error("code generation supports only VHX32 and VHX64",
                                       fn.name);
        std::size_t cursor = 0;
        for (const parameter& parameter : fn.parameters) {
            const std::size_t size = type_size(parameter.type_);
            cursor = align_up(cursor, std::min(size, _word_size));
            _parameter_offsets.push_back(cursor);
            cursor += size;
        }
        allocate_local_slots(fn.body, cursor);
        _local_bytes = cursor;
        measure_call_depth(fn.body);
        _outgoing_offset = align_up(_local_bytes, _word_size);
        _spill_offset = _outgoing_offset + 8 * _word_size * _max_call_depth;
        _return_address_offset = _spill_offset + 8 * _word_size;
        _frame_size = (_return_address_offset + _word_size + 15) & ~std::size_t(15);
        if (_frame_size > 2047 || _return_address_offset > 2047 ||
            _spill_offset + 7 * _word_size > 2047)
            throw vhxcc::compile_error("function stack frame exceeds the supported size",
                                       fn.name);
    }

    generated_function compile() {
        _scopes.emplace_back();
        emit_constant(30, _frame_size, _function.name);
        emit_rr("sub", 2, 2, 30, _function.name);
        emit_memory(_word_size == 4 ? "sw" : "sd", 1, _return_address_offset,
                    _function.name);
        for (std::size_t i = 0; i < _function.parameters.size(); ++i) {
            const std::string& name = _function.parameters[i].name.lexeme();
            const variable_storage variable{
                _function.parameters[i].type_,
                storage{storage::kind::stack, _parameter_offsets[i]}
            };
            if (!_scopes.back().emplace(name, variable).second)
                throw vhxcc::compile_error("duplicate parameter name '" + name + "'",
                                           _function.parameters[i].name);
            emit_memory(store_mnemonic(_function.parameters[i].type_),
                        static_cast<unsigned>(5 + i), _parameter_offsets[i], _function.name);
        }
        emit_constant(5, 0, _function.name);
        for (const statement& stmt : _function.body)
            compile_statement(stmt);
        if (_function.returns_value && !_saw_return)
            throw vhxcc::compile_error("non-void function must return a value",
                                       _function.name);
        emit_label(_epilogue_label);
        emit_memory(_word_size == 4 ? "lw" : "ld", 1, _return_address_offset,
                    _function.name);
        emit_constant(30, _frame_size, _function.name);
        emit_rr("add", 2, 2, 30, _function.name);
        emit("brlr", {}, _function.name);
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

static std::string compile(const std::vector<source_unit>& inputs,
                           const std::string& arch) {
    if (inputs.empty())
        throw std::runtime_error("no C input files");

    std::string assembly = ".text\n";
    struct parsed_function {
        std::size_t file_index;
        function value;
        std::string output_name;
    };
    std::vector<parsed_function> functions;
    std::unordered_map<std::string, bool> definitions;

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
                continue;
            }

            if (definitions.contains(output_name))
                throw vhxcc::compile_error("multiple definition of function '" + external_name + "'",
                                           fn.name);
            definitions.emplace(output_name, fn.is_global);
            functions.push_back({file_index, std::move(fn), output_name});
        }
    }

    for (parsed_function& parsed : functions) {
        std::unordered_map<std::string, std::string> local_function_names;
        for (const parsed_function& candidate : functions) {
            if (candidate.file_index == parsed.file_index &&
                !candidate.value.is_global)
                local_function_names.emplace(candidate.value.name.lexeme(),
                                             candidate.output_name);
        }
        function_compiler compiler(parsed.value, local_function_names, arch,
                                   parsed.output_name);
        generated_function generated = compiler.compile();
        if (parsed.value.is_global)
            assembly += ".global " + parsed.output_name + '\n';
        assembly += parsed.output_name + ":\n" + generated.assembly;
    }
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
        const std::string assembly = compile(sources, opts.arch);
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
