
#include <parser.hpp>

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

    vhxcc::parser::expression expr;
    std::string name;
    std::vector<ast> children;
};