
#include <filesystem>
#include <iostream>
#include <parser.hpp>
#include <string>
#include <utility>

using namespace vhxcc;

struct ast {
    enum type {
        none,
        directive,
        expression,
        instruction,
        label,
        _extern,
        _global,
    } type;

    std::string name;
    vhxcc::parser::expression expr;
    std::vector<ast> children;
};

static const std::string help = R"(Usage: vhx64-as [option...] [asmfile...]
Options:
  --help                  show all assembler options and exit
  -I <dir>                add <dir> to search list for .include directives
  -o <output>             name the object-file output <output> (default a.out)
  --version               print assembler version and exit
  -mabi=<abi name>        specify for ABI <abi name>
  -march=<arch name>      assemble for architecture <arch name>)";

#ifndef HOSTNAME
#define HOSTNAME "unknown"
#endif

#ifndef VHXCC_VERSION
#define VHXCC_VERSION ""
#endif

static const std::string version = R"(VHX assembler (vhxcc for )" HOSTNAME ") " VHXCC_VERSION R"(
Copyright (C) 2026 Free Software Foundation Inc.
This program is free software; you may redistribute it under the terms of
the GNU General Public License version 3 or later.
This program has absolutely no warranty.)";

struct opts {
    std::vector<std::filesystem::path> inputs;
    std::filesystem::path output = "a.out";
    std::string arch = "vhx64imsa";
    std::vector<std::pair<std::string, std::string>> defines;
    
    bool help = false;
    bool version = false;
};

int main(int argc, const char *argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-version" || arg == "--version") {
            std::cout << version << std::endl;
            return 0;
        } else if (arg == "-help" || arg == "--help") {
            std::cout << help << std::endl;
        }
    }
}