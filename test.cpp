#include "lexer.hpp"
#include "parser.hpp"

using namespace vhxcc;

static std::string format_token(const token& token) {
    return std::format("<{}: {}>", token::type_str(token.type()), token.lexeme());
}

static void print_expr(const parser::expression& expr, size_t depth = 0) {
    std::string name;
    switch(expr.type) {
        case parser::expression::none:
            name = "none";
            break;
        case parser::expression::literal:
            name = "literal";
            break;
        case parser::expression::identifier:
            name = "identifier";
            break;
        case parser::expression::unary:
            name = "unary";
            break;
        case parser::expression::binary:
            name = "binary";
            break;
        case parser::expression::ternary:
            name = "ternary";
            break;
        case parser::expression::call:
            name = "call";
            break;
        case parser::expression::_typename:
            name = "typename";
            break;
        default:
            name = "unknown";
            break;
    }
    std::cout << std::format("{}{}: {}", std::string(depth * 2, ' '), name, format_token(expr.token)) << std::endl;
    for (auto leaf : expr.branches)
        print_expr(leaf, depth+1);
}

int main() {
    vhxcc::lexer lexer;
    parser::typenames typenames;

    lexer << " * (int*)(a + b);";

    std::cout << "TOKENS:" << std::endl;
    for (auto token : lexer)
        std::cout << format_token(token) << std::endl;
    
    std::cout << "EXPRESSIONS:" << std::endl;
    lexer::iterator it = lexer.begin();
    parser::expression expr = parser::parse_expression(it, typenames);
    print_expr(expr);
}