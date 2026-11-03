
#include "error.hpp"
#include "token.hpp"
#include <cstddef>
#include <format>
#include <lexer.hpp>
#include <filesystem>
#include <iostream>
#include <parser.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

using namespace vhxcc;

struct ast {
    enum type {
        none,
        directive,
        expression,
        instruction,
        label,
        string
    } type;

    vhxcc::position pos;
    std::string name;
    vhxcc::parser::expression expr;
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

static void _constexpr_fold(ast& node) {
    for (ast& child : node.children)
        _constexpr_fold(child);
    if (node.type == ast::expression) {
        if (node.expr.type == parser::expression::unary) {
            
        } else if (node.expr.type == parser::expression::binary) {

        } else if (node.expr.type == parser::expression::ternary) {

        }
    }
}

static ast constexpr_fold(const ast& node) {
    ast copy = node;

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
            vhxcc::parser::expression expr = vhxcc::parser::expect_expression(it);
            child.expr = expr;
            node.children.push_back(child);
        } while (lexer::match(it, vhxcc::token::comma));
        return node;
    };

    directives["byte"] = simple_data;
    directives["half"] = simple_data;
    directives["hword"] = simple_data;
    directives["word"] = simple_data;
    directives["dword"] = simple_data;
    directives["qword"] = simple_data;
    directives["quad"] = simple_data;
    directives["ascii"] = simple_data;
    directives["asciz"] = simple_data;
    directives["string"] = simple_data;
    directives["short"] = simple_data;
    directives["int"] = simple_data;
    directives["long"] = simple_data;
    directives["float"] = simple_data;
    directives["double"] = simple_data;

    auto one_argument = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        ast child;
        child.type = ast::type::expression;
        vhxcc::parser::expression expr = vhxcc::parser::expect_expression(it);
        child.expr = expr;
        node.children.push_back(child);
        return node;
    };

    directives["org"] = one_argument;
    directives["align"] = one_argument;
    directives["balign"] = one_argument;
    directives["offset"] = one_argument;
    directives["if"] = one_argument;

    auto no_argument = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        (void)it;
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        return node;
    };

    directives["else"] = no_argument;
    directives["endif"] = no_argument;
    directives["err"] = no_argument;

    auto one_string_argument = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        ast child;
        child.type = ast::type::string;
        child.name = lexer::expect(it, vhxcc::token::string_literal).lexeme();
        node.children.push_back(child);
        return node;
    };

    directives["include"] = one_string_argument;
    directives["incbin"] = one_string_argument;
    directives["file"] = one_string_argument;

    auto one_number_argument = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        ast child;
        child.type = ast::type::expression;
        vhxcc::token tok = lexer::expect(it, vhxcc::token::integer_literal);
        child.name = tok.lexeme();
        vhxcc::parser::expression expr;
        expr.type = vhxcc::parser::expression::literal;
        expr.token = tok;
        child.expr = expr;
        node.children.push_back(child);
        return node;
    };

    directives["ln"] = one_number_argument;
    directives["line"] = one_number_argument;

    auto one_identifier_argument = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        ast child;
        child.type = ast::type::label;
        child.name = "";
        vhxcc::token tok;
        if ((tok = lexer::match(it, token::dot))) {
            child.name += '.';
            tok = lexer::expect_after(it, "expected identifier", tok, vhxcc::token::identifier);
            child.name += tok.lexeme();
            child.pos = tok.pos();
        } else {
            tok = lexer::expect(it, "expected identifier", vhxcc::token::identifier);
            child.name += tok.lexeme();
            child.pos = tok.pos();
        }
        while ((tok = lexer::match_after(it, tok, token::dot))) {
            child.name += '.';
            tok = lexer::expect_after(it, "expected identifier", tok, vhxcc::token::identifier);
            child.name += tok.lexeme();
        }
        node.children.push_back(child);
        return node;
    };

    directives["section"] = one_identifier_argument;
    directives["global"] = one_identifier_argument;
    directives["globl"] = one_identifier_argument;
    directives["extern"] = one_identifier_argument;
    directives["ifdef"] = one_identifier_argument;
    directives["ifndef"] = one_identifier_argument;
    // for GNU as compatibility
    directives["ifnotdef"] = one_identifier_argument;

    auto one_identifier_and_expression_argument = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        ast label_child;
        label_child.type = ast::type::label;
        label_child.name = lexer::expect(it, vhxcc::token::identifier).lexeme();
        node.children.push_back(label_child);
        ast child;
        child.type = ast::type::expression;
        vhxcc::parser::expression expr = vhxcc::parser::expect_expression(it);
        child.expr = expr;
        node.children.push_back(child);
        return node;
    };

    directives["set"] = one_identifier_and_expression_argument;

    auto space_directive = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        ast child;
        child.type = ast::type::expression;
        vhxcc::parser::expression expr = vhxcc::parser::expect_expression(it);
        child.expr = expr;
        node.children.push_back(child);
        ast child2;
        child2.type = ast::type::expression;
        vhxcc::parser::expression expr2 = vhxcc::parser::parse_expression(it);
        child2.expr = expr2;
        if (expr2.type != vhxcc::parser::expression::none) {
            node.children.push_back(child2);
        }
        return node;
    };

    directives["space"] = space_directive;
    directives["skip"] = space_directive;

    auto equ_directive = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        ast label_child;
        label_child.type = ast::type::label;
        label_child.name = lexer::expect(it, vhxcc::token::identifier).lexeme();
        node.children.push_back(label_child);
        ast child;
        child.type = ast::type::expression;
        vhxcc::parser::expression expr = vhxcc::parser::parse_expression(it);
        child.expr = expr;
        node.children.push_back(child);
        return node;
    };

    directives["equ"] = equ_directive;
    directives["equiv"] = equ_directive;

    directives["fill"] = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.pos = name.pos();
        node.name = name.lexeme();
        node.type = ast::type::directive;
        ast repeat_child;
        repeat_child.type = ast::type::expression;
        vhxcc::parser::expression repeat_expr = vhxcc::parser::expect_expression(it);
        repeat_child.expr = repeat_expr;
        node.children.push_back(repeat_child);
        if (lexer::match(it, vhxcc::token::comma)) {
            ast size_child;
            size_child.type = ast::type::expression;
            vhxcc::parser::expression size_expr = vhxcc::parser::expect_expression(it);
            size_child.expr = size_expr;
            node.children.push_back(size_child);
            if (lexer::match(it, vhxcc::token::comma)) {
                ast value_child;
                value_child.type = ast::type::expression;
                vhxcc::parser::expression value_expr = vhxcc::parser::expect_expression(it);
                value_child.expr = value_expr;
                node.children.push_back(value_child);
            }
        }
        return node;
    };

    directives["arch"] = [](const vhxcc::token& name, vhxcc::lexer::iterator& it) -> ast {
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
            vhxcc::parser::expression arch_expr = vhxcc::parser::expect_expression(it);
            arch_child.expr = arch_expr;
            node.children.push_back(arch_child);
        }
        return node;
    };

    std::string current_label;

    try {
    
        std::vector<ast> asts;
        // Parse the source code into an abstract syntax tree (AST)
        vhxcc::lexer::iterator it = lexer.begin();
        while (it != lexer.end()) {
            // Process each token from the lexer

            while (lexer::match(it, vhxcc::token::semicolon))
                ;

            if (vhxcc::token tok = lexer::match(it, vhxcc::token::dot)) {
                // local label or directive
                vhxcc::token tok2 = lexer::expect_after(it, "expected identifier", tok, token::identifier);
                if (lexer::match(it, vhxcc::token::colon)) {
                    // local label
                    std::string label = current_label + "." + tok2.lexeme();
                    ast ast;
                    ast.name = label;
                    ast.type = ast::type::label;
                    asts.push_back(ast);
                } else {
                    // directive
                    ast ast;
                    ast.name = tok2.lexeme();
                    ast.type = ast::type::directive;
                    if (directives.find(ast.name) != directives.end()) {
                        ast = directives[ast.name](tok, it);
                    } else {
                        throw vhxcc::compile_error(std::format("unknown directive: '{}'", ast.name));
                    }
                    asts.push_back(ast);
                }
                continue;
            }

            if (vhxcc::token tok = lexer::match(it, vhxcc::token::identifier)) {
                // label or instruction
                if (lexer::match_after(it, vhxcc::token::colon)) {
                    // label
                    std::string label = tok.lexeme();
                    ast node;
                    node.name = label;
                    node.type = ast::type::label;
                    asts.push_back(node);
                    current_label = label;
                } else if (token tok2 = lexer::match_after(it, tok, token::dot)) {
                    // explicit local label
                    vhxcc::token tok3 = lexer::expect_after(it, "malformed label", token::identifier);
                    lexer::expect_after(it, "malformed label", tok2, vhxcc::token::colon);
                    std::string label = tok.lexeme() + "." + tok3.lexeme();
                    ast ast;
                    ast.name = label;
                    ast.type = ast::type::label;
                    asts.push_back(ast);
                } else if (vhxcc::token tok2 = lexer::match(it, vhxcc::token::assign)) {
                    // assignment to a label or symbol
                    ast node;
                    node.type = ast::type::directive;
                    node.name = "equ";
                    ast label;
                    label.type = ast::type::label;
                    label.name = tok.lexeme();
                    node.children.push_back(std::move(label));
                    ast child;
                    child.type = ast::type::expression;
                    child.expr = vhxcc::parser::expect_expression(it);
                    node.children.push_back(std::move(child));
                    asts.push_back(node);
                } else {
                    // instruction
                    ast node;
                    node.name = tok.lexeme();
                    node.type = ast::type::instruction;
                    do {
                        ast child;
                        child.type = ast::type::expression;
                        child.expr = vhxcc::parser::parse_expression(it);
                        node.children.push_back(std::move(child));
                    } while (lexer::match(it, vhxcc::token::comma));
                    asts.push_back(node);
                }
                continue;
            }

            // error matches GNU assembler error format
            throw vhxcc::compile_error(std::format("junk at end of line, first unrecognized token: `{}'", it->lexeme()), *it);
        }

        std::unordered_map<std::string, ast (*)(ast&)> builtins;

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
        size_t if_level = 0;
        bool if_cond = true;
        for (auto& ast : asts) {
            expand_builtins(ast, builtins);

            if (ast.type == ast::directive && ast.name == "ifdef") {
                if (if_cond) {
                    ++if_level;

                }
            }
            if (ast.type == ast::directive && ast.name == "section") {
                std::string section_name = ast.children.at(0).name;
                if (section_name.size() == 0)
                    throw vhxcc::compile_error("empty section name");
                section = section_name;
            } else if (ast.type == ast::label) {
                defined.insert(ast.name);
            } else {
                if (!sections.contains(section))
                    sections[section];
                sections.at(section).push_back(ast);
            }
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
                        if (node.name == "byte") {
                            section_sizes[section] += 1 * node.children.size();
                        } else if (node.name == "short" || node.name == "half" || node.name == "hword") {
                            section_sizes[section] += 2 * node.children.size();
                        } else if (node.name == "int" || node.name == "word") {
                            section_sizes[section] += 4 * node.children.size();
                        } else if (node.name == "long" || node.name == "quad" || node.name == "qword") {
                            section_sizes[section] += 8 * node.children.size();
                        } else if (node.name == "float") {
                            section_sizes[section] += 4 * node.children.size();
                        } else if (node.name == "double") {
                            section_sizes[section] += 8 * node.children.size();
                        } else if (node.name == "ascii") {
                            for (auto& child : node.children) {
                                if (child.type == ast::expression &&
                                    child.expr.type == parser::expression::literal &&
                                    child.expr.token.type() == token::string_literal) {

                                    section_sizes
                                } else {
                                    throw vhxcc::compile_error(std::format("unexpected {}", token::type_str(child.expr.token.type())));
                                }
                            }
                            section_sizes[section] += 1 * node.children.size();
                        } else if (node.name == "asciz" || node.name == "string") {

                        } else if (node.name == "skip" || node.name == "space") {

                        } else if (node.name == "offset") {

                        } else if (node.name == "org") {

                        } else if (node.name == "equ") {

                        } else if (node.name == "equiv") {

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