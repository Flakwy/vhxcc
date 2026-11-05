#pragma once

#include "token.hpp"
#include <lexer.hpp>
#include <initializer_list>
#include <string>
#include <unordered_set>
#include <vector>

namespace vhxcc {

class parser {
public:

    class typenames {
    private:
        std::unordered_set<std::string> _global;
        std::unordered_map<std::string, std::unordered_set<std::string>> _namespaces;

    public:
        typenames() {
            _global.insert("int");
            _global.insert("char");
            _global.insert("float");
            _global.insert("double");
            _global.insert("_Bool");

            // some non-standard C types I'm gonna add
            _global.insert("byte");

            _namespaces["struct"];
            _namespaces["union"];
            _namespaces["enum"];
        }

        void insert(const std::string& _typename) {
            _global.insert(_typename);
        }

        void insert(const std::string& _namespace, const std::string& _typename) {
            if (!_namespaces.contains(_namespace))
                throw vhxcc::compile_error(std::format("unknown namespace '{}'", _namespace));
            _namespaces.at(_namespace).insert(_typename);
        }

        const std::unordered_set<std::string>& globals() const {
            return _global;
        }

        const std::unordered_set<std::string>& structs() const {
            return _namespaces.at("struct");
        }

        const std::unordered_set<std::string>& unions() const {
            return _namespaces.at("union");
        }

        const std::unordered_set<std::string>& enums() const {
            return _namespaces.at("enum");
        }

        bool contains(const std::string& _typename) const {
            return _global.contains(_typename);
        }

        bool contains(const std::string& _namespace, const std::string& _typename) const {
            if (!_namespaces.contains(_namespace))
                throw vhxcc::compile_error(std::format("unknown namespace '{}'", _namespace));
            return _namespaces.at(_namespace).contains(_typename);
        }
    };

    struct expression {
        enum type {
            none,
            literal,
            identifier,
            unary,
            binary,
            ternary,
            call,
            asm_memory,
            _typename
        };

        type type;
        vhxcc::token token;
        std::vector<expression> branches;

        operator bool() const {
            return type != none;
        }
    };

    static inline const expression none = {expression::none, lexer::none, {}};

private:

    static expression parse_assign(lexer::iterator& it, const parser::typenames& typenames, bool assembly = false) {
        expression lhs = parse_lor(it, typenames, assembly);

        static const std::initializer_list<enum token::type> assign_ops = {
            token::assign, token::lshift_assign, token::and_assign,
            token::minus_assign, token::modulo_assign, token::or_assign,
            token::plus_assign, token::rshift_assign, token::slash_assign,
            token::times_assign, token::xor_assign
        };

        if (token tok = lexer::match(it, assign_ops)) {
            expression rhs;
            rhs.type = expression::binary;
            rhs.token = tok;
            rhs.branches.push_back(std::move(lhs));
            rhs.branches.push_back((parse_assign(it, typenames, assembly)));
            lhs = std::move(rhs);
        } else if (token tok = lexer::match(it, token::question)) {
            expression rhs;
            rhs.type = expression::ternary;
            rhs.token = tok;
            rhs.branches.push_back(std::move(lhs));
            rhs.branches.push_back(parse_assign(it, typenames, assembly));
            lexer::expect(it, "malformed ternary expression", token::colon);
            rhs.branches.push_back(parse_assign(it, typenames, assembly));
            lhs = std::move(rhs);
        }
        if (lhs.type == expression::none) {
            return parser::none;
        }
        for (auto& branch : lhs.branches) {
            if (branch.type == expression::none) {
                return parser::none;
            }
        }
        return lhs;
    }

    static expression parse_lor(lexer::iterator& it, const parser::typenames& typenames, bool assembly = false) {
        expression lhs = parse_land(it, typenames, assembly);
        while (token tok = lexer::match(it, token::lor)) {
            expression rhs;
            rhs.type = expression::binary;
            rhs.token = tok;
            rhs.branches.push_back(std::move(lhs));
            rhs.branches.push_back(parse_land(it, typenames, assembly));
            lhs = std::move(rhs);
        }
        if (lhs.type == expression::none) {
            return parser::none;
        }
        for (auto& branch : lhs.branches) {
            if (branch.type == expression::none) {
                return parser::none;
            }
        }
        return lhs;
    }

    static expression parse_land(lexer::iterator& it, const parser::typenames& typenames, bool assembly = false) {
        expression lhs = parse_or(it, typenames, assembly);
        while (token tok = lexer::match(it, token::land)) {
            expression rhs;
            rhs.type = expression::binary;
            rhs.token = tok;
            rhs.branches.push_back(std::move(lhs));
            rhs.branches.push_back(parse_or(it, typenames, assembly));
            lhs = std::move(rhs);
        }
        if (lhs.type == expression::none) {
            return parser::none;
        }
        for (auto& branch : lhs.branches) {
            if (branch.type == expression::none) {
                return parser::none;
            }
        }
        return lhs;
    }

    static expression parse_or(lexer::iterator& it, const parser::typenames& typenames, bool assembly = false) {
        expression lhs = parse_xor(it, typenames, assembly);
        while (token tok = lexer::match(it, token::_or)) {
            expression rhs;
            rhs.type = expression::binary;
            rhs.token = tok;
            rhs.branches.push_back(std::move(lhs));
            rhs.branches.push_back(parse_xor(it, typenames, assembly));
            lhs = std::move(rhs);
        }
        if (lhs.type == expression::none) {
            return parser::none;
        }
        for (auto& branch : lhs.branches) {
            if (branch.type == expression::none) {
                return parser::none;
            }
        }
        return lhs;
    }

    static expression parse_xor(lexer::iterator& it, const parser::typenames& typenames, bool assembly = false) {
        expression lhs = parse_and(it, typenames, assembly);
        while (token tok = lexer::match(it, token::_xor)) {
            expression rhs;
            rhs.type = expression::binary;
            rhs.token = tok;
            rhs.branches.push_back(std::move(lhs));
            rhs.branches.push_back(parse_and(it, typenames, assembly));
            lhs = std::move(rhs);
        }
        if (lhs.type == expression::none) {
            return parser::none;
        }
        for (auto& branch : lhs.branches) {
            if (branch.type == expression::none) {
                return parser::none;
            }
        }
        return lhs;
    }

    static expression parse_and(lexer::iterator& it, const parser::typenames& typenames, bool assembly = false) {
        expression lhs = parse_equal(it, typenames, assembly);
        while (token tok = lexer::match(it, token::_and)) {
            expression rhs;
            rhs.type = expression::binary;
            rhs.token = tok;
            rhs.branches.push_back(std::move(lhs));
            rhs.branches.push_back(parse_equal(it, typenames, assembly));
            lhs = std::move(rhs);
        }
        if (lhs.type == expression::none) {
            return parser::none;
        }
        for (auto& branch : lhs.branches) {
            if (branch.type == expression::none) {
                return parser::none;
            }
        }
        return lhs;
    }

    static expression parse_equal(lexer::iterator& it, const parser::typenames& typenames, bool assembly = false) {
        expression lhs = parse_compare(it, typenames, assembly);
        while (token tok = lexer::match(it, token::equal, token::not_equal)) {
            expression rhs;
            rhs.type = expression::binary;
            rhs.token = tok;
            rhs.branches.push_back(std::move(lhs));
            rhs.branches.push_back(parse_compare(it, typenames, assembly));
            lhs = std::move(rhs);
        }
        if (lhs.type == expression::none) {
            return parser::none;
        }
        for (auto& branch : lhs.branches) {
            if (branch.type == expression::none) {
                return parser::none;
            }
        }
        return lhs;
    }

    static expression parse_compare(lexer::iterator& it, const parser::typenames& typenames, bool assembly = false) {
        expression lhs = parse_shift(it, typenames, assembly);
        while (token tok = lexer::match(it, token::less_than, token::less_equal, token::greater_than, token::greater_equal)) {
            expression rhs;
            rhs.type = expression::binary;
            rhs.token = tok;
            rhs.branches.push_back(std::move(lhs));
            rhs.branches.push_back(parse_shift(it, typenames, assembly));
            lhs = std::move(rhs);
        }
        if (lhs.type == expression::none) {
            return parser::none;
        }
        for (auto& branch : lhs.branches) {
            if (branch.type == expression::none) {
                return parser::none;
            }
        }
        return lhs;
    }

    static expression parse_shift(lexer::iterator& it, const parser::typenames& typenames, bool assembly = false) {
        expression lhs = parse_addition(it, typenames, assembly);
        while (token tok = lexer::match(it, token::lshift, token::rshift)) {
            expression rhs;
            rhs.type = expression::binary;
            rhs.token = tok;
            rhs.branches.push_back(std::move(lhs));
            rhs.branches.push_back(parse_addition(it, typenames, assembly));
            lhs = std::move(rhs);
        }
        if (lhs.type == expression::none) {
            return parser::none;
        }
        for (auto& branch : lhs.branches) {
            if (branch.type == expression::none) {
                return parser::none;
            }
        }
        return lhs;
    }

    static expression parse_addition(lexer::iterator& it, const parser::typenames& typenames, bool assembly = false) {
        expression lhs = parse_multiplication(it, typenames, assembly);
        while (token tok = lexer::match(it, token::plus, token::minus)) {
            expression rhs;
            rhs.type = expression::binary;
            rhs.token = tok;
            rhs.branches.push_back(std::move(lhs));
            rhs.branches.push_back(parse_multiplication(it, typenames, assembly));
            lhs = std::move(rhs);
        }
        if (lhs.type == expression::none) {
            return parser::none;
        }
        for (auto& branch : lhs.branches) {
            if (branch.type == expression::none) {
                return parser::none;
            }
        }
        return lhs;
    }

    static expression parse_multiplication(lexer::iterator& it, const parser::typenames& typenames, bool assembly = false) {
        expression lhs = parse_prefix(it, typenames, assembly);
        while (token tok = lexer::match(it, token::times, token::slash, token::modulo)) {
            expression rhs;
            rhs.type = expression::binary;
            rhs.token = tok;
            rhs.branches.push_back(std::move(lhs));
            rhs.branches.push_back(parse_prefix(it, typenames, assembly));
            lhs = std::move(rhs);
        }
        if (lhs.type == expression::none) {
            return parser::none;
        }
        for (auto& branch : lhs.branches) {
            if (branch.type == expression::none) {
                return parser::none;
            }
        }
        return lhs;
    }

    static expression parse_prefix(lexer::iterator& it, const parser::typenames& typenames, bool assembly = false) {
        static const std::initializer_list<enum token::type> prefix_ops = {
            token::plus, token::minus, token::_not, token::_and,
            token::lnot, token::times, token::increment, token::decrement
        };

        if (token tok = lexer::match(it, prefix_ops)) {
            expression node;
            node.type = expression::unary;
            node.token = tok;
            node.branches.push_back(parse_prefix(it, typenames, assembly)); // Right-to-left chaining
            return node;
        } else {
            if (token tok = lexer::match(it, token::lparen)) {
                if (expression type_val = parse_typename(it, typenames, assembly)) {
                    lexer::expect(it, "expected ')' in type cast", token::rparen);
                    expression node;
                    node.type = expression::binary;
                    node.token = tok;
                    node.branches.push_back(type_val);
                    node.branches.push_back(parse_prefix(it, typenames, assembly));
                    if (node.type == expression::none) {
                        return parser::none;
                    }
                    for (auto& branch : node.branches) {
                        if (branch.type == expression::none) {
                            return parser::none;
                        }
                    }
                    return node;
                }
            }
            return parse_postfix(it, typenames, assembly);
        }
    }

    static expression parse_postfix(lexer::iterator& it, const parser::typenames& typenames, bool assembly = false) {
        expression expr = parse_primary(it, typenames, assembly);

        lexer::iterator old = it;
        while (true) {
            if (lexer::match(it, token::lbracket)) { // Array indexing: expr [ index ]
                expression node;
                node.type = expression::binary;
                node.token = token(token::lbracket, "[");
                node.branches.push_back(std::move(expr));
                node.branches.push_back(parse_expression(it, typenames, assembly));
                lexer::expect(it, token::rbracket);
                expr = std::move(node);
            } 
            else if (lexer::match(it, token::lparen)) { // Function call: expr ( args... )
                expression node;
                node.type = expression::call;
                node.token = expr.token; // Keep callee reference
                node.branches.push_back(std::move(expr));
                
                if (!lexer::match(it, token::rparen)) {
                    do {
                        node.branches.push_back(expect_expression(it, typenames, assembly));
                    } while (lexer::match(it, token::comma));
                    lexer::expect(it, token::rparen);
                }
                expr = std::move(node);
            } 
            else if (token tok = lexer::match(it, {token::dot, token::rarrow})) { // Member access: expr . member
                if (!assembly) {
                    expression node;
                    node.type = expression::binary;
                    node.token = tok;
                    node.branches.push_back(std::move(expr));
                    
                    token member = lexer::expect(it, "expected member name", token::identifier);
                    expression mem_node;
                    mem_node.type = expression::identifier;
                    mem_node.token = member;
                    node.branches.push_back(std::move(mem_node));
                    expr = std::move(node);
                } else {
                    it = old; // Reset the iterator to the previous position
                    return expr;
                }
            } 
            else if (token tok = lexer::match(it, {token::increment, token::decrement})) { // Postfix ++ / --
                expression node;
                node.type = expression::binary;
                node.token = tok;
                node.branches.push_back(std::move(expr));
                expr = std::move(node);
            } 
            else {
                break;
            }
        }
        // check if any is none
        if (expr.type == expression::none) {
            return parser::none;
        }
        for (auto& branch : expr.branches) {
            if (branch.type == expression::none) {
                return parser::none;
            }
        }
        return expr;
    }

    static expression parse_primary(lexer::iterator& it, const parser::typenames& typenames, bool assembly = false) {
        // 1. Literals
        if (token tok = lexer::match(it, {token::integer_literal, token::float_literal, token::string_literal, token::char_literal})) {
            expression node;
            node.type = expression::literal;
            node.token = tok;
            return node;
        }

        // 2. Identifiers
        if (assembly) {
            token tok;
            std::string name = "";
            expression node;
            vhxcc::position pos;
            node.type = expression::identifier;
            tok = lexer::match(it, token::identifier, token::dot);
            if (tok) {
                name += tok.lexeme();
                while ((tok = lexer::match_after(it, tok, token::identifier, token::dot)))
                    name += tok.lexeme();
                if (!name.empty()) {
                    node.token = token(token::identifier, name, pos);
                    return node;
                }
            }
        } else {
            if (token tok = lexer::match(it, token::identifier)) {
                expression node;
                node.type = expression::identifier;
                node.token = tok;
                return node;
            }
        }

        // 3. Parentheses & Casts
        if (token tok = lexer::match(it, token::lparen)) {
            expression inner = parse_expression(it, typenames, assembly);
            lexer::expect(it, token::rparen);

            return inner;
        }

        // 4. Pointer symbol for assembly
        if (token tok = lexer::match(it, token::pointer)) {
            expression node;
            node.type = expression::identifier;
            node.token = tok;
            return node;
        }
        
        return parser::none;
    }


    static expression parse_typename(lexer::iterator& it, const parser::typenames& typenames, [[maybe_unused]] bool assembly = false) {
        lexer::iterator old = it;
        expression type_val;
        type_val.type = expression::_typename;

        static auto is_qualifier_or_modifier = [](const token& token) -> bool {
            return (token == token::identifier) && ((token == "const" || token == "volatile" || token == "restrict") || (token == "short" || token == "long" || token == "signed" || token == "unsigned"));
        };

        static auto is_modifier = [](const token& token) -> bool {
            return (token == token::identifier) && (token == "short" || token == "long" || token == "signed" || token == "unsigned");
        };

        static auto is_namespace = [](const token& token) -> bool {
            return (token == token::identifier) && (token == "struct" || token == "union" || token == "enum");
        };

        static auto is_typename = [](const parser::typenames& typenames) {
            return [&](const token& token) -> bool {
                return (token == token::identifier) && typenames.contains(token.lexeme());
            };
        };

        static auto is_typename_namespace = [](const std::string& _namespace, const parser::typenames& typenames) {
            return [&](const token& token) -> bool {
                return (token == token::identifier) && typenames.contains(_namespace, token.lexeme());
            };
        };

        auto _is_typename = is_typename(typenames);

        bool valid = false;
        bool first = true;
        while (token tok = lexer::match(it, is_qualifier_or_modifier)) {
            if (first) {
                type_val.token = tok;
                first = false;
            }

            expression node;
            node.type = expression::identifier;
            node.token = tok;
            type_val.branches.push_back(std::move(node));

            if (is_modifier(tok))
                valid = true;
        }

        if (token tok = lexer::match(it, is_namespace)) {
            expression node;
            node.type = expression::identifier;
            node.token = tok;

            auto _is_typename_namespace = is_typename_namespace(tok.lexeme(), typenames);

            if (token _tok = lexer::match(it, _is_typename_namespace)) {
                expression _node;
                _node.type = expression::identifier;
                _node.token = _tok;
                node.branches.push_back(std::move(_node));
            } else {
                it = old;
                return parser::none;
            }

            type_val.branches.push_back(std::move(node));
        } else if (token tok = lexer::match(it, _is_typename)) {
            expression node;
            node.type = expression::identifier;
            node.token = tok;
            type_val.branches.push_back(std::move(node));
        } else if (!valid) {
            it = old;
            return parser::none;
        }

        while (token tok = lexer::match(it, token::times)) {
            expression node;
            node.type = expression::identifier; // temporary, I hope
            node.token = tok;
            type_val.branches.push_back(std::move(node));
        }

        return type_val;
    }

    static expression _parse_expression(lexer::iterator& it, const parser::typenames& typenames, bool assembly = false) {
        lexer::iterator old = it;
        expression expr = parse_assign(it, typenames, assembly);
        if (expr.type == expression::none) {
            it = old;
        }
        return expr;
    }

public:

    static expression parse_expression(lexer::iterator& it, const parser::typenames& typenames, bool assembly = false) {
        return _parse_expression(it, typenames, assembly);
    }

    static expression parse_expression(lexer::iterator& it, bool assembly = false) {
        return _parse_expression(it, {}, assembly);
    }

    static expression expect_expression(lexer::iterator& it, const std::string message, const parser::typenames& typenames, bool assembly = false) {
        lexer::iterator old = it;
        expression expr = _parse_expression(it, typenames, assembly);
        if (expr.type == expression::none) {
            throw vhxcc::compile_error(message, *old);
        }
        return expr;
    }

    static expression expect_expression(lexer::iterator& it, const std::string message, bool assembly = false) {
        lexer::iterator old = it;
        expression expr = _parse_expression(it, {}, assembly);
        if (expr.type == expression::none) {
            throw vhxcc::compile_error(message, *old);
        }
        return expr;
    }

    static expression expect_expression(lexer::iterator& it, const parser::typenames& typenames, bool assembly = false) {
        return expect_expression(it, "expected expression", typenames, assembly);
    }

    static expression expect_expression(lexer::iterator& it, bool assembly = false) {
        return expect_expression(it, "expected expression", assembly);
    }

    static expression expect_identifier(lexer::iterator& it, const std::string message, bool assembly = false) {
        lexer::iterator old = it;
        expression expr = _parse_expression(it, {}, assembly);
        if (expr.type != expression::identifier) {
            throw vhxcc::compile_error(message, *old);
        }
        return expr;
    }

    static expression expect_identifier(lexer::iterator& it, bool assembly = false) {
        return expect_identifier(it, "expected identifier", assembly);
    }

    static expression parse_identifier(lexer::iterator& it, bool assembly = false) {
        lexer::iterator old = it;
        expression expr = _parse_expression(it, {}, assembly);
        if (expr.type != expression::identifier) {
            it = old;
            return parser::none;
        }
        return expr;
    }
};

}