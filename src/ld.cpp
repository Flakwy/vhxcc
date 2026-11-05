
#include <string>

static const std::string help = R"(Usage: vhx-ld [options] file...
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

int main(int argc, const char *argv[]) {
    
}