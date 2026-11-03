
#include <cstddef>
#include <format>
#include <lexer.hpp>
#include <filesystem>
#include <iostream>
#include <parser.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
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

    std::string name;
    vhxcc::parser::expression expr;
    std::vector<ast> children;
};

static const std::string help = R"(Usage: vhx64-as [option...] [asmfile...]
Options:
  --help                  show all assembler options and exit
  -e <entry>              specify the entry point symbol <entry>
  -I <dir>                add <dir> to search list for .include directives
  -o <output>             name the object-file output <output> (default a.out)
  --version               print assembler version and exit
  -mabi=<abi name>        specify for ABI <abi name>
  -march=<arch name>      assemble for architecture <arch name>)";

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

    std::unordered_map<std::string, ast (*)(const std::string&, vhxcc::lexer::iterator&)> directives;

    auto simple_data = [](const std::string& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.name = name;
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

    auto one_argument = [](const std::string& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.name = name;
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

    auto no_argument = [](const std::string& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.name = name;
        node.type = ast::type::directive;
        return node;
    };

    directives["else"] = no_argument;
    directives["endif"] = no_argument;
    directives["err"] = no_argument;

    auto one_string_argument = [](const std::string& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.name = name;
        node.type = ast::type::directive;
        ast child;
        child.type = ast::type::string;
        child.name = lexer::expect(it, vhxcc::token::string_literal).lexeme();
        node.children.push_back(child);
        return node;
    };

    directives["section"] = one_string_argument;
    directives["include"] = one_string_argument;
    directives["incbin"] = one_string_argument;
    directives["file"] = one_string_argument;

    auto one_number_argument = [](const std::string& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.name = name;
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

    auto one_identifier_argument = [](const std::string& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.name = name;
        node.type = ast::type::directive;
        ast child;
        child.type = ast::type::label;
        child.name = lexer::expect(it, vhxcc::token::identifier).lexeme();
        node.children.push_back(child);
        return node;
    };

    directives["global"] = one_identifier_argument;
    directives["globl"] = one_identifier_argument;
    directives["extern"] = one_identifier_argument;
    directives["ifdef"] = one_identifier_argument;
    directives["ifndef"] = one_identifier_argument;
    // for GNU as compatibility
    directives["ifnotdef"] = one_identifier_argument;

    auto one_identifier_and_expression_argument = [](const std::string& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.name = name;
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

    auto space_directive = [](const std::string& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.name = name;
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

    auto equ_directive = [](const std::string& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.name = name;
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

    directives["fill"] = [](const std::string& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.name = name;
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

    directives["arch"] = [](const std::string& name, vhxcc::lexer::iterator& it) -> ast {
        ast node;
        node.name = name;
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
        std::string previous_file = "";
        while (it != lexer.end()) {
            std::string current_file = it->pos().file;
            if (current_file != previous_file) {
                previous_file = current_file;
                current_label = current_file;
            }

            // Process each token from the lexer

            if (vhxcc::token tok = lexer::match(it, vhxcc::token::dot)) {
                // local label or directive
                auto is_identifier_and_after = [&](const vhxcc::token& _tok) {
                    return _tok == vhxcc::token::identifier && (_tok.pos().column == tok.pos().column + 1);
                };
                vhxcc::token tok2 = lexer::expect(it, "expected identifier", is_identifier_and_after);
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
                        ast = directives[ast.name](ast.name, it);
                    } else {
                        throw vhxcc::compile_error(std::format("unknown directive: '{}'", ast.name));
                    }
                    asts.push_back(ast);
                }
                continue;
            }

            if (vhxcc::token tok = lexer::match(it, vhxcc::token::identifier)) {
                // label or instruction
            }
        }
    
    } catch (const vhxcc::compile_error& e) {
        std::cerr << std::format("{}{}:{}:{}: {}error:{} {}", BOLD, e.token().pos().file, e.token().pos().line, e.token().pos().column, RED, RESET, e.what()) << std::endl;
        return 1;
    }
    
    // Proceed with the rest of the program using the parsed options
    return 0;
}