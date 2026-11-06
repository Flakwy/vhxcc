
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <object.hpp>

static const std::string help = R"(Usage: vhx64-none-vxo-ld [options] file...
Options:
  -b TARGET, --format TARGET  Specify target for following input files
  -e ADDRESS, --entry ADDRESS Set start address
  --enable-non-contiguous-regions
                              Enable support of non-contiguous memory regions
  --enable-non-contiguous-regions-warnings
                              Enable warnings when --enable-non-contiguous-regions may cause unexpected behaviour
  --disable-linker-version    Ignored
  --enable-linker-version     Ignored
  -EB                         Link big-endian objects
  -EL                         Link little-endian objects
  -f SHLIB, --auxiliary SHLIB Ignored 
  -F SHLIB, --filter SHLIB    Ignored
  -g                          Ignored
  -G SIZE, --gpsize SIZE      Small data size (if no size, same as --shared) (not
                                implemented yet)
  -h FILENAME, --soname FILENAME
                              Set internal name of shared library (not implemented yet)
  -I PROGRAM, --dynamic-linker PROGRAM
                              Set PROGRAM as the dynamic linker to us (not implemented yet)
  --no-dynamic-linker         Produce an executable with no program interpreter header (not
                                implemented yet)
  -l LIBNAME, --library LIBNAME
                              Search for library LIBNAME (not implemented yet)
  -L DIRECTORY, --library-path DIRECTORY
                              Add DIRECTORY to library search path
  -o FILE, --output FILE      Set output file name
  -O                          Optimize output file (not implemented yet)
  -q, --emit-relocs           Generate relocations in final output
  -r, -i, --relocatable       Generate relocatable output
  -R FILE, --just symbols FILE
                              Just link symbols (if directory, same as --rpath)
  -s, --strip-all             Strip all symbols
  -S, --strip-debug           Ignored
  --strip-discarded           Ignored
  -T FILE, --script FILE      Read linker script
  --default-script FILE, -dT  Read default linker script
  -u SYMBOL, --undefinef SYMBOL
                              Start with undefined reference to SYMBOL
  --require-defined SYMBOL    Require SYMBOL to be defined in the final output
  --unique [=SECTION]         Don't merge inpht [SECTION | orphan] sections
  -v, --version               Print version information
  -V                          Print version and emulation information
  -x, --discard-all           Discard all local symbols
  -X, --discard-locals        Discard temporary local symbols (default)
  --discard-none              Don't discard any local symbols
  --accept-unknown-input-arch Accept input files whose architecture cannot be determined
  --no-accept-unknown-input-arch
                              Reject input files whose architecture is unknown
  --as-needed                 Ignored
  --no-as-needed              Ignored
  -Bdynamic, -dy, -call_shared
                              Link against shared libraries
  --Bstatic, -dn, -non_shared, -static
                              Do not link against shared libraries
  -Bno-symbolic               Don't bind global references locally
  -Bsymbolic                  Bind global references locally
  -Bsymbolic-functions        Ignored (use -Bsymbolic instead)
  --check-sections            Check section addresses for overlaps (default)
  --no-check-sections         Do not check section addresses for overlaps
  --defsym SYMBOL=EXPRESSION  Define a symbol
  --demangle [=STYLE]         Demangle symbol names [using STYLE] (not implemented yet)
  --disable-multiple-abs-defs Do not allow multiple definitions with symbols included
                                in filename invoked by -R or --just-symbols
  --embedded-relocs           Generate embedded relocs (not implemented yet)
  --fatal-warnings            Treat warnings as errors
  --no-fatal-warnings         Do not treat warnings as errors (default)
  --gc-sections               Remove unused sections
  --no-gc-sections            Don't remove unused sections (default)
  --print-gc-sections         List removed umused sections on stderr
  --no-print-gc-sections      Do not list removed unused sections
  --gc-keep-exported          Keep exported symbols when removing unused sections
  --help                      Print option help
  -init SYMBOL                Call SYMBOL at load-time
  -Map FILE/DIR               Write a linker map to FILE or DIR/<outputname>.map
  --no-define-common          Ignored
  --no-demangle               Do not demangle symbol names
  --no-keep-memory            Ignored
  --no-undefinef              Do not allow unresolved references in object files
  -w, --no-warnings           Do not dis'lay any warning or error messages
  --allow-shlib-undefined     Allow unresolved references in shared libraries (not
                                implemented yet)
  --no-allow-shlib-undefined  Do not allow unresolved references in shared libs (not
                                implemented yet)
  --allow-multiple-definition Allow multiple definitions
  --undefined-version         Ignored
  --no-undefined-version      Ignored
  --default-symver            Ignored for ELF compatibility
  --default-imported-symver   Ignored for ELF compatibility
  --no-warn-mismatch          Don't warn about mismatched input files
  --no-warn-search-mismatch   Don't warn on finding an incompatible library
  --no-whole-archive          Turn off --whole-archive
  --noinhibit-exec            Create an object file even if errors occur
  -nostdlib                   Only use library directories specified on
                                the command line
  --oformat TARGET            Specify target of output file
  --print-output-format       Print default output format
  --reduce-memory-overheads   Reduce memory overheads, possibly taking much longer
  --max-cache-size=SIZE       Ignored
  --relax                     Reduce code size by using target specific optimizations
  --no-relax                  Do not use relaxation techniques to reduce code size
  --retain-symbols-file FILE  Keep only symbols listed in FILE
  -rpath PATH                 Set runtime shared library search PATH
  -rpath-link PATH            Set linke time shared library search path
  -shared, -Bshareable        Create a shared library (not implemented yet)
  -pie, --pic-executable      Create a position independent executable
  -no-pie                     Create a position dependent executable (default)
  --sort-common [=ascending|descending]
                              Ignored for ELF compatibility
  --sort-section name|alignment
                              Sort sections by name or maximum alignment
  --section-ordering-file FILE
                              Sort sections by statements in FILE (not implemented yet)
  --split-by-file [=SIZE]     Split output sections every SIZE octets (not implemented yet)
  --split-by-reloc [=COUNT]   Split output sections every COUNT relocs (not implemented
                                yet)
  --stats                     Print resource usage statistics
  --no-stats                  Do not print resource usage statistics
  --target-help               Display target specific options
  --section-start SECTION=ADDRESS
                              Set address of named section
  --image-base ADDRESS        Set image base address
  -Tbss ADDRESS               Set address of .bss section
  -Tdata ADDRESS              Set address of .data section
  -Ttext ADDRESS              Set address of .text section
  -Ttext-segment ADDRESS      Ignored for ELF compatibility
  -Trodata-segment ADDRESS    Ignored for ELF compatibility
  -Tldata-segment ADDRESS     Ignored for ELF compatibility
  --unresolved-symbols=<method>
                              How to handle unresolved symbols. <method> is:
                                ignore-all, report-all, ignore-in-object-files,
                                ignore-in-shared-libs
  --verbose [=NUMBER]         Output lots of information during link
  --warn-common               Ignored
  --warn-multiple-gp          Warn if the multiple GP values are used
  --warn-once                 Warn only once per undefined symbol
  --warn-section-align        Warn if start of section changes due to alignment
  --warn-textrel              Ignored for ELF compatibility
  --warn-alternate-em         Ignored for ELF compatibility
  --warn-unresolved-symbols   Report unresolved symbols as warnings
  --error-unresolved-symbols  Report unresolved symbols as errors
  --whole-archive             Include all objects from following archives (not
                                implemented yet)
  --wrap SYMBOL               Ignored for GCC LTO compatibility
  --ignore-unresolved-symbol SYMBOL
                              Unresolved SYMBOL will not cause an error or warning
  --push-state                Push state of flags governing input file handling
  --pop-state                 Pop state of flags governing input file handling
  --print-memory-usage        Report target memory usage
  --print-map-discarded       Show discarded sections in map file output (default)
  --no-print-map-discarded    Do not show discarded sections in map file output
  --print-map-locals          Show local symbols in map file output
  --no-print-map-locals       Do not show local symbols in map file output (default)
  @FILE                       Read options from FILE
vhx64-none-vxo-ld: supported targets: vxo32-littlevhx vxo64-littlevhx vxo32-bigvhx vxo64-bigvhx vxo32-little vxo64-little vxo32-big vxo64-big binary

Report bugs to <https://github.com/Flakwy/vhxcc/issues/>)";


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

namespace {

struct input_file {
    std::string path;
    std::string target;
};

struct link_options {
    std::unordered_map<std::string, bool> flags;
    std::unordered_map<std::string, std::vector<std::string>> values;
    std::vector<input_file> inputs;
    std::vector<std::string> undefines;
    std::vector<std::string> require_defined;
    std::vector<std::string> ignored_unresolved;
    std::map<std::string, uint64_t> section_starts;
    std::string output = "a.out";
    std::string entry;
    std::string target;
    std::string output_format;
    std::string unresolved_method = "report-all";
    uint64_t image_base = 0;
    bool image_base_set = false;
    bool endian_set = false;
    bool big_endian = false;
    bool help = false;
    bool version = false;
    bool print_output_format = false;
};

static void record_flag(link_options& options, const std::string& name) {
    options.flags[name] = true;
}

static void record_value(link_options& options, const std::string& name, const std::string& value) {
    options.values[name].push_back(value);
}

static uint64_t parse_number(const std::string& value, const std::string& option) {
    if (value.empty() || value.front() == '-')
        throw std::runtime_error("invalid numeric value for " + option + ": " + value);
    std::size_t parsed = 0;
    uint64_t result;
    try {
        result = std::stoull(value, &parsed, 0);
    } catch (const std::exception&) {
        throw std::runtime_error("invalid numeric value for " + option + ": " + value);
    }
    if (parsed != value.size())
        throw std::runtime_error("invalid numeric value for " + option + ": " + value);
    return result;
}

static std::vector<std::string> tokenize_response_file(const std::string& text) {
    std::vector<std::string> result;
    std::string token;
    char quote = '\0';
    bool started = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '\\' && quote != '\'') {
            if (i + 1 < text.size() &&
                (text[i + 1] == '"' || text[i + 1] == '\\' ||
                 std::isspace(static_cast<unsigned char>(text[i + 1])))) {
                token += text[++i];
            } else {
                token += c;
            }
            started = true;
        } else if (quote != '\0') {
            if (c == quote)
                quote = '\0';
            else
                token += c;
        } else if (c == '"' || c == '\'') {
            quote = c;
            started = true;
        } else if (std::isspace(static_cast<unsigned char>(c))) {
            if (started) {
                result.push_back(std::move(token));
                token.clear();
                started = false;
            }
        } else {
            token += c;
            started = true;
        }
    }
    if (quote != '\0')
        throw std::runtime_error("unterminated quote in response file");
    if (started)
        result.push_back(std::move(token));
    return result;
}

static void expand_response_files(const std::vector<std::string>& args,
                                  std::vector<std::string>& expanded, unsigned depth = 0) {
    if (depth > 16)
        throw std::runtime_error("response-file nesting exceeds 16 levels");
    for (const std::string& arg : args) {
        if (arg.size() <= 1 || arg.front() != '@') {
            expanded.push_back(arg);
            continue;
        }
        std::ifstream file(arg.substr(1), std::ios::binary);
        if (!file)
            throw std::runtime_error("cannot open response file: " + arg.substr(1));
        std::ostringstream contents;
        contents << file.rdbuf();
        expand_response_files(tokenize_response_file(contents.str()), expanded, depth + 1);
    }
}

static std::string require_value(const std::vector<std::string>& args, std::size_t& index,
                                 const std::string& option, const std::string& inline_value = "") {
    if (!inline_value.empty())
        return inline_value;
    if (index + 1 >= args.size())
        throw std::runtime_error("missing argument to " + option);
    return args[++index];
}

static std::string section_assignment(const std::string& value, const std::string& option) {
    const std::size_t equals = value.find('=');
    if (equals == std::string::npos || equals == 0 || equals + 1 == value.size())
        throw std::runtime_error("expected SECTION=ADDRESS after " + option);
    return value;
}

static void parse_arguments(int argc, const char* argv[], link_options& options) {
    std::vector<std::string> raw_args;
    for (int i = 1; i < argc; ++i)
        raw_args.emplace_back(argv[i]);
    std::vector<std::string> args;
    expand_response_files(raw_args, args);

    bool end_options = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        std::string arg = args[i];
        if (end_options || arg.empty() || arg[0] != '-') {
            options.inputs.push_back({arg, options.target});
            continue;
        }
        if (arg == "--") {
            end_options = true;
            continue;
        }

        std::size_t equals = arg.find('=');
        std::string name = arg.substr(0, equals);
        std::string inline_value = equals == std::string::npos ? "" : arg.substr(equals + 1);
        if (equals == std::string::npos) {
            static const std::vector<std::string> attached_options = {
                "-Map", "-dT", "-b", "-e", "-o", "-L", "-l", "-I", "-R",
                "-f", "-F", "-G", "-h", "-T"
            };
            for (const std::string& prefix : attached_options) {
                if (prefix == "-T" &&
                    (arg == "-Ttext" || arg == "-Tdata" || arg == "-Tbss" ||
                     arg == "-Ttext-segment" || arg == "-Trodata-segment" ||
                     arg == "-Tldata-segment"))
                    continue;
                if (arg.size() > prefix.size() && arg.compare(0, prefix.size(), prefix) == 0) {
                    name = prefix;
                    inline_value = arg.substr(prefix.size());
                    break;
                }
            }
        }

        if (name == "--help") {
            options.help = true;
            record_flag(options, name);
        } else if (name == "--version" || name == "-v") {
            options.version = true;
            record_flag(options, name);
        } else if (name == "--print-output-format") {
            options.print_output_format = true;
            record_flag(options, name);
        } else if (name == "-b" || name == "--format") {
            options.target = require_value(args, i, name, inline_value);
            record_value(options, name, options.target);
        } else if (name == "-e" || name == "--entry") {
            options.entry = require_value(args, i, name, inline_value);
            record_value(options, name, options.entry);
        } else if (name == "-o" || name == "--output") {
            options.output = require_value(args, i, name, inline_value);
            record_value(options, name, options.output);
        } else if (name == "--oformat") {
            options.output_format = require_value(args, i, name, inline_value);
            record_value(options, name, options.output_format);
            if (!options.endian_set)
                options.big_endian = options.output_format.find("big") != std::string::npos;
        } else if (name == "-EB" || name == "-EL") {
            options.endian_set = true;
            options.big_endian = name == "-EB";
            record_flag(options, name);
        } else if (name == "--image-base") {
            const std::string value = require_value(args, i, name, inline_value);
            options.image_base = parse_number(value, name);
            options.image_base_set = true;
            record_value(options, name, value);
        } else if (name == "--section-start") {
            const std::string assignment = section_assignment(require_value(args, i, name, inline_value), name);
            const std::size_t split = assignment.find('=');
            options.section_starts[assignment.substr(0, split)] = parse_number(assignment.substr(split + 1), name);
            record_value(options, name, assignment);
        } else if (name == "-Ttext" || name == "-Tdata" || name == "-Tbss") {
            const std::string value = require_value(args, i, name, inline_value);
            const std::string section = name == "-Ttext" ? ".text" : (name == "-Tdata" ? ".data" : ".bss");
            options.section_starts[section] = parse_number(value, name);
            record_value(options, name, value);
        } else if (name == "-T" || name == "--script" || name == "--default-script" || name == "-dT" ||
                   name == "-f" || name == "--auxiliary" || name == "-F" || name == "--filter" ||
                   name == "-G" || name == "--gpsize" || name == "-h" || name == "--soname" ||
                   name == "-I" || name == "--dynamic-linker" || name == "-l" || name == "--library" ||
                   name == "-L" || name == "--library-path" || name == "-R" || name == "--just-symbols" ||
                   name == "-u" || name == "--undefinef" || name == "--require-defined" ||
                   name == "--defsym" || name == "-Map" || name == "-init" ||
                   name == "--retain-symbols-file" || name == "--sort-section" ||
                   name == "--section-ordering-file" || name == "-rpath" || name == "-rpath-link" ||
                   name == "--ignore-unresolved-symbol" || name == "--wrap" ||
                   name == "--split-by-reloc" || name == "--split-by-file" ||
                   name == "--max-cache-size" || name == "--unresolved-symbols" ||
                   name == "--verbose" || name == "--demangle" || name == "--sort-common" ||
                   name == "-Ttext-segment" || name == "-Trodata-segment" || name == "-Tldata-segment") {
            std::string value;
            if (name == "--verbose" || name == "--demangle" || name == "--sort-common" ||
                name == "--split-by-reloc" || name == "--split-by-file") {
                value = inline_value;
            } else if ((name == "-G" || name == "--gpsize") && inline_value.empty() &&
                       (i + 1 >= args.size() || args[i + 1].empty() ||
                        !std::isdigit(static_cast<unsigned char>(args[i + 1][0])))) {
                value = "shared";
            } else {
                value = require_value(args, i, name, inline_value);
            }
            record_value(options, name, value);
            if (name == "-u" || name == "--undefinef")
                options.undefines.push_back(value);
            if (name == "--require-defined")
                options.require_defined.push_back(value);
            if (name == "--ignore-unresolved-symbol")
                options.ignored_unresolved.push_back(value);
            if (name == "--unresolved-symbols") {
                if (value != "ignore-all" && value != "report-all" &&
                    value != "ignore-in-object-files" && value != "ignore-in-shared-libs")
                    throw std::runtime_error("invalid unresolved-symbols method: " + value);
                options.unresolved_method = value;
            }
            if (name == "--defsym") {
                const std::size_t split = value.find('=');
                if (split == std::string::npos || split == 0)
                    throw std::runtime_error("expected SYMBOL=EXPRESSION after --defsym");
            }
        } else if (name == "--unique" || name == "--verbose" || name == "--demangle" ||
                   name == "--sort-common" || name == "--split-by-file" ||
                   name == "--split-by-reloc" || name == "--max-cache-size") {
            record_value(options, name, inline_value);
        } else {
            static const std::unordered_set<std::string> accepted_flags = {
                "--enable-non-contiguous-regions", "--enable-non-contiguous-regions-warnings",
                "--disable-linker-version", "--enable-linker-version", "-g", "-O", "-q",
                "--emit-relocs", "-r", "-i", "--relocatable", "-s", "--strip-all",
                "--no-dynamic-linker", "-x", "--discard-all", "-X", "--discard-locals",
                "--discard-none", "--accept-unknown-input-arch", "--no-accept-unknown-input-arch",
                "--as-needed", "--no-as-needed", "-Bdynamic", "-dy", "-call_shared",
                "--Bstatic", "-Bstatic", "-dn", "-non_shared", "-static", "-Bno-symbolic",
                "-Bsymbolic", "-Bsymbolic-functions", "--check-sections", "--no-check-sections",
                "--disable-multiple-abs-defs", "--embedded-relocs", "--fatal-warnings",
                "--no-fatal-warnings", "--gc-sections", "--no-gc-sections", "--print-gc-sections",
                "--no-print-gc-sections", "--gc-keep-exported", "--no-demangle",
                "--no-keep-memory", "--no-undefinef", "-w", "--no-warnings",
                "--allow-shlib-undefined", "--no-allow-shlib-undefined",
                "-S", "--strip-debug", "--strip-discarded",
                "--allow-multiple-definition", "--undefined-version", "--no-undefined-version",
                "--no-undefined",
                "--default-symver", "--default-imported-symver", "--no-warn-mismatch",
                "--no-warn-search-mismatch", "--no-whole-archive", "-nostdlib",
                "--shared", "-Bshareable", "--pie", "--pic-executable", "-no-pie",
                "--reduce-memory-overheads", "--relax", "--no-relax", "--stats", "--no-stats",
                "--target-help", "--no-define-common", "--no-inhibit-exec",
                "--warn-common", "--warn-multiple-gp", "--warn-once", "--warn-section-align",
                "--warn-textrel", "--warn-alternate-em", "--warn-unresolved-symbols",
                "--error-unresolved-symbols", "--whole-archive", "--print-memory-usage",
                "--print-map-discarded", "--no-print-map-discarded", "--print-map-locals",
                "--no-print-map-locals", "-V", "-EB", "-EL",
                "--push-state", "--pop-state"
            };
            if (!accepted_flags.contains(name))
                throw std::runtime_error("unrecognized option: " + arg);
            record_flag(options, name);
            if (name == "-r" || name == "-i" || name == "--relocatable")
                options.flags["--relocatable"] = true;
        }

        if (!options.endian_set && options.output_format.empty() && !options.target.empty() &&
            (options.target.find("big") != std::string::npos))
            options.big_endian = true;
    }
}

static bool has_flag(const link_options& options, const std::string& name) {
    const auto it = options.flags.find(name);
    return it != options.flags.end() && it->second;
}

static uint64_t checked_add(uint64_t left, uint64_t right, const std::string& what) {
    if (right > std::numeric_limits<uint64_t>::max() - left)
        throw std::runtime_error(what + " overflows address range");
    return left + right;
}

static uint64_t align_up(uint64_t value, uint64_t alignment) {
    if (alignment == 0)
        throw std::runtime_error("section has zero alignment");
    const uint64_t remainder = value % alignment;
    return remainder == 0 ? value : checked_add(value, alignment - remainder, "section alignment");
}

static std::vector<std::string> ordered_section_names(const vhxcc::object& object) {
    std::vector<std::string> names;
    names.reserve(object.sections().size());
    for (const auto& [name, unused] : object.sections()) {
        (void)unused;
        names.push_back(name);
    }
    auto rank = [](const std::string& name) {
        if (name == ".text") return 0;
        if (name == ".rodata") return 1;
        if (name == ".data") return 2;
        if (name == ".bss") return 3;
        return 4;
    };
    std::sort(names.begin(), names.end(), [&](const std::string& left, const std::string& right) {
        if (rank(left) != rank(right))
            return rank(left) < rank(right);
        return left < right;
    });
    return names;
}

struct symbol_source {
    std::size_t input_index;
    std::string original_name;
    vhxcc::symbol value;
};

static std::string unique_local_name(const std::string& name, std::size_t input_index,
                                     std::unordered_set<std::string>& claimed) {
    if (!claimed.contains(name)) {
        claimed.insert(name);
        return name;
    }
    const std::string base = ".Lld." + std::to_string(input_index) + "." + name;
    std::string unique = base;
    for (std::size_t suffix = 1; claimed.contains(unique); ++suffix)
        unique = base + "." + std::to_string(suffix);
    claimed.insert(unique);
    return unique;
}

static int64_t signed_difference(uint64_t target, uint64_t place) {
    if (target >= place) {
        const uint64_t difference = target - place;
        if (difference > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
            throw std::runtime_error("relocation value is out of range");
        return static_cast<int64_t>(difference);
    }
    const uint64_t difference = place - target;
    const uint64_t min_magnitude = uint64_t{1} << 63;
    if (difference > min_magnitude)
        throw std::runtime_error("relocation value is out of range");
    if (difference == min_magnitude)
        return std::numeric_limits<int64_t>::min();
    return -static_cast<int64_t>(difference);
}

static uint32_t read_word_le(const std::vector<char>& data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 4)
        throw std::runtime_error("relocation field extends beyond its section");
    uint32_t word = 0;
    for (std::size_t i = 0; i < 4; ++i)
        word |= static_cast<uint32_t>(static_cast<uint8_t>(data[offset + i])) << (i * 8);
    return word;
}

static void write_word_le(std::vector<char>& data, std::size_t offset, uint32_t word) {
    if (offset > data.size() || data.size() - offset < 4)
        throw std::runtime_error("relocation field extends beyond its section");
    for (std::size_t i = 0; i < 4; ++i)
        data[offset + i] = static_cast<char>((word >> (i * 8)) & 0xff);
}

static void write_integer(std::vector<char>& data, std::size_t offset, uint64_t value,
                          std::size_t width, bool big_endian) {
    if (offset > data.size() || data.size() - offset < width)
        throw std::runtime_error("relocation field extends beyond its section");
    for (std::size_t i = 0; i < width; ++i) {
        const std::size_t shift = big_endian ? (width - 1 - i) * 8 : i * 8;
        data[offset + i] = static_cast<char>((value >> shift) & 0xff);
    }
}

static void apply_relocation(vhxcc::section& section, const vhxcc::relocation& reloc,
                             uint64_t symbol_value, bool big_endian) {
    const uint64_t target = checked_add(symbol_value, static_cast<uint64_t>(reloc.addend),
                                        "relocation target");
    const uint64_t place = checked_add(section.virt_addr, static_cast<uint64_t>(reloc.offset),
                                       "relocation place");
    switch (reloc.type) {
        case vhxcc::relocation::vhx_pc12: {
            const int64_t displacement = signed_difference(target, place);
            if (displacement < -2048 || displacement > 2047)
                throw std::runtime_error("PC12 relocation is out of range");
            uint32_t word = read_word_le(section.data, reloc.offset);
            word = (word & 0x000fffffU) |
                   ((static_cast<uint32_t>(displacement) & 0xfffU) << 20);
            write_word_le(section.data, reloc.offset, word);
            break;
        }
        case vhxcc::relocation::vhx_pca8:
        case vhxcc::relocation::vhx_pcs8: {
            const int64_t displacement = signed_difference(target, place);
            const bool adding = reloc.type == vhxcc::relocation::vhx_pca8;
            if ((adding && (displacement < 1 || displacement > 256)) ||
                (!adding && (displacement > -1 || displacement < -256)))
                throw std::runtime_error("PC8 relocation is out of range");
            const uint32_t magnitude = static_cast<uint32_t>(adding ? displacement : -displacement);
            const uint32_t bits = magnitude - 1;
            uint32_t word = read_word_le(section.data, reloc.offset);
            word = (word & ~((0x1fU << 11) | (0x3U << 30) | (1U << 19))) |
                   ((bits & 0x1fU) << 11) |
                   (((bits >> 5) & 0x3U) << 30) |
                   (((bits >> 7) & 0x1U) << 19);
            write_word_le(section.data, reloc.offset, word);
            break;
        }
        case vhxcc::relocation::vhx_rel32:
        case vhxcc::relocation::vhx_rel64: {
            const std::size_t width = reloc.type == vhxcc::relocation::vhx_rel32 ? 4 : 8;
            const int64_t displacement = signed_difference(target, place);
            if (width == 4 && (displacement < std::numeric_limits<int32_t>::min() ||
                               displacement > std::numeric_limits<int32_t>::max()))
                throw std::runtime_error("REL32 relocation is out of range");
            write_integer(section.data, reloc.offset, static_cast<uint64_t>(displacement),
                          width, big_endian);
            break;
        }
        case vhxcc::relocation::vhx_abs32:
            if (target > std::numeric_limits<uint32_t>::max())
                throw std::runtime_error("ABS32 relocation is out of range");
            write_integer(section.data, reloc.offset, target, 4, big_endian);
            break;
        case vhxcc::relocation::vhx_abs64:
            write_integer(section.data, reloc.offset, target, 8, big_endian);
            break;
        case vhxcc::relocation::vhx_abs_lo16:
        case vhxcc::relocation::vhx_abs_hi16:
        case vhxcc::relocation::vhx_abs_hlo16:
        case vhxcc::relocation::vhx_abs_hhi16: {
            const unsigned shift =
                reloc.type == vhxcc::relocation::vhx_abs_lo16 ? 0 :
                reloc.type == vhxcc::relocation::vhx_abs_hi16 ? 16 :
                reloc.type == vhxcc::relocation::vhx_abs_hlo16 ? 32 : 48;
            uint32_t word = read_word_le(section.data, reloc.offset);
            word = (word & 0xffffU) | (static_cast<uint32_t>((target >> shift) & 0xffffU) << 16);
            write_word_le(section.data, reloc.offset, word);
            break;
        }
    }
}

static int link(const link_options& options) {
    if (options.inputs.empty())
        throw std::runtime_error("no input files");

    std::vector<vhxcc::object> inputs;
    inputs.reserve(options.inputs.size());
    for (const input_file& input : options.inputs) {
        std::ifstream stream(input.path, std::ios::binary);
        if (!stream)
            throw std::runtime_error("cannot open input file: " + input.path);
        inputs.emplace_back(stream);
        if (!stream.eof() && stream.fail())
            throw std::runtime_error("failed reading input file: " + input.path);
    }

    const std::string arch = inputs.front().arch();
    const std::string platform = inputs.front().platform();
    for (std::size_t i = 1; i < inputs.size(); ++i) {
        if (inputs[i].arch() != arch)
            throw std::runtime_error("incompatible architecture in " + options.inputs[i].path);
        if (inputs[i].platform() != platform)
            throw std::runtime_error("incompatible platform in " + options.inputs[i].path);
    }

    vhxcc::object output;
    output.arch(arch);
    output.platform(platform);
    std::string entry = options.entry;
    if (entry.empty()) {
        for (const vhxcc::object& input : inputs) {
            if (!input.entry().empty()) {
                entry = input.entry();
                break;
            }
        }
    }
    output.entry(entry);

    std::unordered_set<std::string> global_names;
    for (const vhxcc::object& input : inputs) {
        for (const auto& [name, symbol] : input.symbols()) {
            if (symbol.is_global)
                global_names.insert(name);
        }
    }
    global_names.insert(options.undefines.begin(), options.undefines.end());

    std::vector<std::unordered_map<std::string, std::string>> symbol_names(inputs.size());
    std::unordered_set<std::string> claimed_names = global_names;
    std::unordered_map<std::string, symbol_source> symbols;
    const bool allow_multiple = has_flag(options, "--allow-multiple-definition");
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        for (const auto& [name, symbol] : inputs[i].symbols()) {
            const std::string output_name = symbol.is_global
                ? name : unique_local_name(name, i, claimed_names);
            symbol_names[i].emplace(name, output_name);
            symbol_source candidate{i, name, symbol};
            auto found = symbols.find(output_name);
            if (found == symbols.end()) {
                symbols.emplace(output_name, candidate);
            } else if (symbol.is_global && symbol.is_defined) {
                if (found->second.value.is_defined) {
                    if (!allow_multiple)
                        throw std::runtime_error("multiple definition of symbol: " + name);
                } else {
                    found->second = candidate;
                }
            }
            if (options.entry.empty()) {
                for (std::size_t i = 0; i < inputs.size(); ++i) {
                    const std::string input_entry = inputs[i].entry();
                    const auto mapped = symbol_names[i].find(input_entry);
                    if (!input_entry.empty() && mapped != symbol_names[i].end()) {
                        output.entry(mapped->second);
                        break;
                    }
                }
            } else {
                for (std::size_t i = 0; i < inputs.size(); ++i) {
                    const auto mapped = symbol_names[i].find(options.entry);
                    if (mapped != symbol_names[i].end()) {
                        output.entry(mapped->second);
                        break;
                    }
                }
            }
        }
    }

    std::vector<std::unordered_map<std::string, std::size_t>> section_offsets(inputs.size());
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        for (const std::string& name : ordered_section_names(inputs[i])) {
            const vhxcc::section& source = inputs[i].sections().at(name);
            if (source.alignment == 0)
                throw std::runtime_error("section " + name + " has zero alignment");
            vhxcc::section& destination = output.section(name, source.alignment);
            const std::size_t offset = destination.data.size();
            section_offsets[i].emplace(name, offset);
            destination.data.insert(destination.data.end(), source.data.begin(), source.data.end());
        }
    }

    for (const auto& [name, source] : symbols) {
        const vhxcc::symbol& symbol = source.value;
        if (!symbol.is_defined) {
            output.define(name, symbol.is_global);
            continue;
        }
        if (symbol.is_absolute) {
            output.define(name, symbol.value, symbol.is_global);
            continue;
        }
        const auto offset = section_offsets[source.input_index].find(symbol.section);
        if (offset == section_offsets[source.input_index].end())
            throw std::runtime_error("symbol " + source.original_name + " refers to missing section " + symbol.section);
        if (symbol.value > std::numeric_limits<std::size_t>::max() - offset->second)
            throw std::runtime_error("symbol value overflows in section " + symbol.section);
        output.define(symbol.section, name, symbol.value + offset->second, symbol.is_global);
    }

    for (const std::string& name : options.undefines) {
        output.require(name);
        output.global(name);
    }

    for (std::size_t i = 0; i < inputs.size(); ++i) {
        for (const std::string& name : ordered_section_names(inputs[i])) {
            const vhxcc::section& source = inputs[i].sections().at(name);
            const std::size_t base = section_offsets[i].at(name);
            vhxcc::section& destination = output.section(name);
            for (const vhxcc::relocation& reloc : source.relocations) {
                if (reloc.offset > std::numeric_limits<std::size_t>::max() - base)
                    throw std::runtime_error("relocation offset overflows in section " + name);
                auto symbol = symbol_names[i].find(reloc.symbol);
                const std::string output_name = symbol == symbol_names[i].end()
                    ? reloc.symbol : symbol->second;
                if (symbol == symbol_names[i].end())
                    output.require(output_name);
                destination.relocations.emplace_back(reloc.type, output_name, base + reloc.offset, reloc.addend);
            }
        }
    }

    if (!has_flag(options, "--relocatable")) {
        std::vector<std::string> names;
        for (const auto& [name, unused] : output.sections()) {
            (void)unused;
            names.push_back(name);
        }
        auto rank = [](const std::string& name) {
            if (name == ".text") return 0;
            if (name == ".rodata") return 1;
            if (name == ".data") return 2;
            if (name == ".bss") return 3;
            return 4;
        };
        std::sort(names.begin(), names.end(), [&](const std::string& left, const std::string& right) {
            if (rank(left) != rank(right))
                return rank(left) < rank(right);
            return left < right;
        });
        uint64_t next_address = options.image_base_set ? options.image_base : 0;
        std::vector<std::pair<uint64_t, uint64_t>> ranges;
        for (const std::string& name : names) {
            vhxcc::section& section = output.section(name);
            const auto fixed = options.section_starts.find(name);
            const uint64_t address = fixed == options.section_starts.end()
                ? align_up(next_address, section.alignment) : fixed->second;
            const uint64_t end = checked_add(address, section.data.size(), "section " + name);
            if (!has_flag(options, "--no-check-sections") &&
                !has_flag(options, "--enable-non-contiguous-regions")) {
                for (const auto& [other_start, other_end] : ranges) {
                    if (address < other_end && other_start < end)
                        throw std::runtime_error("section " + name + " overlaps another output section");
                }
            }
            if (address > std::numeric_limits<std::size_t>::max())
                throw std::runtime_error("section address exceeds host size range: " + name);
            section.virt_addr = static_cast<std::size_t>(address);
            ranges.emplace_back(address, end);
            next_address = std::max(next_address, end);
        }
    } else {
        for (const auto& [name, unused] : output.sections()) {
            (void)unused;
            output.section(name).virt_addr = 0;
        }
    }

    for (const std::string& name : options.require_defined) {
        const auto symbol = output.symbols().find(name);
        if (symbol == output.symbols().end() || !symbol->second.is_defined)
            throw std::runtime_error("required symbol is not defined: " + name);
    }

    if (!has_flag(options, "--relocatable")) {
        for (const std::string& name : ordered_section_names(output)) {
            vhxcc::section& section = output.section(name);
            std::vector<vhxcc::relocation> remaining;
            for (const vhxcc::relocation& reloc : section.relocations) {
                const auto symbol_it = output.symbols().find(reloc.symbol);
                if (symbol_it == output.symbols().end() || !symbol_it->second.is_defined) {
                    const bool explicitly_ignored =
                        std::find(options.ignored_unresolved.begin(), options.ignored_unresolved.end(),
                                  reloc.symbol) != options.ignored_unresolved.end();
                    const bool ignore = explicitly_ignored ||
                        options.unresolved_method == "ignore-all" ||
                        options.unresolved_method == "ignore-in-object-files";
                    const bool must_error = has_flag(options, "--no-undefined") ||
                        has_flag(options, "--no-undefinef") ||
                        has_flag(options, "--error-unresolved-symbols");
                    const bool warn = has_flag(options, "--warn-unresolved-symbols");
                    if (!ignore && (must_error || !warn))
                        throw std::runtime_error("undefined symbol: " + reloc.symbol);
                    if (!ignore && warn)
                        std::cerr << "vhx-ld: warning: undefined symbol: " << reloc.symbol << '\n';
                    remaining.push_back(reloc);
                    continue;
                }
                const vhxcc::symbol& symbol = symbol_it->second;
                uint64_t value = symbol.value;
                if (!symbol.is_absolute) {
                    const auto target_section = output.sections().find(symbol.section);
                    if (target_section == output.sections().end())
                        throw std::runtime_error("symbol " + reloc.symbol + " refers to missing section " + symbol.section);
                    value = checked_add(target_section->second.virt_addr, value, "symbol address");
                }
                apply_relocation(section, reloc, value, options.big_endian);
                if (has_flag(options, "--emit-relocs"))
                    remaining.push_back(reloc);
            }
            section.relocations = std::move(remaining);
        }
    }

    std::ofstream stream(options.output, std::ios::binary);
    if (!stream)
        throw std::runtime_error("cannot open output file: " + options.output);
    output.write(stream);
    if (!stream)
        throw std::runtime_error("failed writing output file: " + options.output);
    return 0;
}

} // namespace

int main(int argc, const char *argv[]) {
    link_options options;
    try {
        parse_arguments(argc, argv, options);
        if (options.help) {
            std::cout << help << '\n';
            return 0;
        }
        if (options.version) {
            std::cout << version << '\n';
            return 0;
        }
        if (options.print_output_format) {
            std::cout << (options.output_format.empty() ? "vxo" : options.output_format) << '\n';
            return 0;
        }
        return link(options);
    } catch (const std::exception& error) {
        std::cerr << "vhx-ld: error: " << error.what() << '\n';
        return 1;
    }
}