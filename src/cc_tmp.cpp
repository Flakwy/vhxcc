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

struct options {
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
        frame,
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
    statement body;
};

std::unordered_map<std::string, type> typedef_names;

void __init_types() {
    typedef_names["void"] = type{type::kind::void_, "void", 0, 0, 0};
    typedef_names["char"] = type{type::kind::char_, "char", 1, 1, 0};
    typedef_names["_Bool"] = type{type::kind::char_, "char", 1, 1, 0};
    // short, long, and long long should be handled as modifiers of int
    typedef_names["int"] = type{type::kind::int_, "int", 4, 4, 0};
    typedef_names["float"] = type{type::kind::int_, "float", 4, 4, 0};
    typedef_names["double"] = type{type::kind::int_, "double", 8, 8, 0};
}

using scope = std::unordered_map<std::string, value>;

expression parse_expression(lexer::iterator& it, const scope& scope) {

}

}