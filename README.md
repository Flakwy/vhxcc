# vhxcc
Experimental C compiler for the VHX architecture

Build the tools with `make debug` and run the persistent compiler tests with
`make test`. Use `vhx64-none-vxo-cc -S input.c` to emit assembly instead of a
VXO object file. The driver invokes the host `gcc -E` preprocessor, the VHX
assembler, and the VHX linker as separate programs. Set `VHXC_CPP`, `VHXC_AS`,
or `VHXC_LD` to override their executable names.

The C frontend supports an integer-focused subset with stack locals, direct
function calls (up to eight register arguments), `if`/`else`, and `while`.
Pointers, aggregate types, global variables, and several C control-flow
statements are still unsupported. Unsupported constructs produce diagnostics
rather than being silently miscompiled.
