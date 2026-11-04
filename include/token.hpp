#pragma once

#include <string>
#include <string_view>

namespace vhxcc {

struct position {
    std::string file;
    std::size_t offset;
    std::size_t line;
    std::size_t column;

    bool operator==(const position& rhs) const {
        return (file == rhs.file) && (offset == rhs.offset);
    }
};

class token {
public:
    enum type {
        none,
        eof,
        comma,
        colon,
        semicolon,
        dot,
        rarrow,
        question,
        plus,
        minus,
        times,
        slash,
        modulo,
        increment,
        decrement,
        _and,
        _or,
        _xor,
        _not,
        lshift,
        rshift,
        less_than,
        less_equal,
        greater_than,
        greater_equal,
        equal,
        not_equal,
        land,
        lor,
        lnot,
        lparen,
        rparen,
        lbracket,
        rbracket,
        lcurly,
        rcurly,
        identifier,
        integer_literal,
        float_literal,
        char_literal,
        string_literal,
        assign,
        plus_assign,
        minus_assign,
        times_assign,
        slash_assign,
        modulo_assign,
        and_assign,
        or_assign,
        xor_assign,
        lshift_assign,
        rshift_assign,
        pointer,
        unknown
    };

    // using enum type;
private:
    token::type _type;
    std::string _lexeme;
    position _pos; 

public:
    token() : _type(none), _lexeme(""), _pos({"", 0, 0, 0}) {}
    token(token::type type, std::string lexeme = "", position pos = {"string", 0, 0, 0})
        : _type(type), _lexeme(lexeme), _pos(pos) {}

    token::type type() const {
        return _type;
    }

    const std::string& lexeme() const {
        return _lexeme;
    }

    const position& pos() const {
        return _pos;
    }

    operator bool() const {
        return _type != none;
    }

    explicit operator enum token::type() const {
        return _type;
    }

    explicit operator std::string() const {
        return _lexeme;
    }

    bool operator==(const token& rhs) const {
        return (_type == rhs._type) && (_lexeme == rhs._lexeme);
    }

    bool operator==(enum token::type type) const {
        return _type == type;
    }

    bool operator==(const std::string& lexeme) const {
        return _lexeme == lexeme;
    }

    static std::string_view type_str(enum token::type type) {
        switch (type) {
            case none:            return "none";
            case eof:             return "end of file";
            case comma:           return "comma";
            case colon:           return "colon";
            case semicolon:       return "semicolon";
            case dot:             return "dot";
            case rarrow:          return "rarrow";
            case question:        return "question";
            case plus:            return "plus";
            case minus:           return "minus";
            case times:           return "times";
            case slash:           return "slash";
            case modulo:          return "modulo";
            case increment:       return "increment";
            case decrement:       return "decrement";
            case _and:            return "and";
            case _or:             return "or";
            case _xor:            return "xor";
            case _not:            return "not";
            case lshift:          return "lshift";
            case rshift:          return "rshift";
            case less_than:       return "less_than";
            case less_equal:      return "less_equal";
            case greater_than:    return "greater_than";
            case greater_equal:   return "greater_equal";
            case equal:           return "equal";
            case not_equal:       return "not_equal";
            case land:            return "land";
            case lor:             return "lor";
            case lnot:            return "lnot";
            case lparen:          return "lparen";
            case rparen:          return "rparen";
            case lbracket:        return "lbracket";
            case rbracket:        return "rbracket";
            case lcurly:          return "lcurly";
            case rcurly:          return "rcurly";
            case identifier:      return "identifier";
            case integer_literal: return "integer literal";
            case float_literal:   return "floating-point literal";
            case char_literal:    return "character literal";
            case string_literal:  return "string literal";
            case assign:          return "assign";
            case plus_assign:     return "plus_assign";
            case minus_assign:    return "minus_assign";
            case times_assign:    return "times_assign";
            case slash_assign:    return "slash_assign";
            case modulo_assign:   return "modulo_assign";
            case and_assign:      return "and_assign";
            case or_assign:       return "or_assign";
            case xor_assign:      return "xor_assign";
            case lshift_assign:   return "lshift_assign";
            case rshift_assign:   return "rshift_assign";
            case pointer:         return "pointer";
            case unknown:         return "unknown token";
        }
        return "unknown token";
    }

    friend class lexer;
};

}