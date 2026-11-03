#pragma once
#include <error.hpp>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <unordered_map>

namespace vhxcc {

class source {
    struct file_source {
        std::string path;
        std::string source;
        std::size_t line;
        std::size_t column;
    };

    std::vector<file_source> _source;

public:
    struct file {
        std::string path;
        std::size_t line;
        std::size_t column;
        file(const std::string& path, std::size_t line, std::size_t column) :
            path(path), line(line), column(column) {}
    };

    static constexpr int eof = -1;

    source& operator=(const source&) = delete;
    source& operator=(source&&) = delete;

    source() = default;

    source(const source& other) {
        _source = other._source;
    }

    source(source&& other) {
        _source = std::move(other._source);
    }

    ~source() = default;

    source& append(char c) {
        if (_source.size() == 0) {
            file_source file {"string", "", 1, 1};
            _source.push_back(file);
        }
        _source.back().source += c;
        return *this;
    }

    source& append(const std::string& source) {
        if (_source.size() == 0) {
            file_source file {"string", source, 1, 1};
            _source.push_back(file);
        } else {
            _source.back().source += source;
        }
        return *this;
    }

    source& append(std::filesystem::path path) {
        std::ifstream handle(path);
        if (!handle.is_open())
            throw vhxcc::compile_error(std::format("could not open file: {}", path.string()));
        std::stringstream buffer;
        buffer << handle.rdbuf();

        file_source file;
        file.path = path.relative_path().string();
        file.source = buffer.str();
        file.line = 1;
        file.column = 1;
        _source.push_back(file);
        return *this;
    }

    source& append(const source& other) {
        _source.insert(_source.end(), other._source.cbegin(), other._source.cend());
        return *this;
    }

    source& append(const source::file& file) {
        file_source source;
        source.path = file.path;
        source.source = "";
        source.line = file.line;
        source.column = file.column;
        _source.push_back(source);
        return *this;
    }

    template <typename T>
    source& operator<<(const T& rhs) {
        return this->append(rhs);
    }

    class iterator {
    private:
        const source* _source;
        std::size_t _index;
        position _pos;

        iterator() = delete;
        iterator(const source& source, std::size_t index) : _source(&source), _index(index) {
            _pos.offset = 0;
            if (_index >= _source->_source.size()) {
                _pos.file = "<eof>";
                _pos.line = 1;
                _pos.column = 1;
                _pos.offset = 0;
            } else {
                _pos.file = _source->_source.at(_index).path;
                _pos.line = _source->_source.at(_index).line;
                _pos.column = _source->_source.at(_index).column;
                _pos.offset = 0;
            }
        }

    public:
        int operator*() const {
            if (_index >= _source->_source.size())
                return eof;
            if (_pos.offset >= _source->_source.at(_index).source.size())
                return eof;
            return _source->_source.at(_index).source.at(_pos.offset);
        }

        const position* operator->() const {
            return &_pos;
        }
        
        const position& pos() const {
            return _pos;
        }

        iterator& operator++() {
            if (_index >= _source->_source.size())
                return *this;
            int c = **this;
            if (++_pos.offset >= _source->_source.at(_index).source.size()) {
                ++_index;
                if (_index >= _source->_source.size()) {
                    _pos.file = "<eof>";
                    _pos.line = 1;
                    _pos.column = 1;
                    _pos.offset = 0;
                } else {
                    _pos.file = _source->_source.at(_index).path;
                    _pos.line = _source->_source.at(_index).line;
                    _pos.column = _source->_source.at(_index).column;
                    _pos.offset = 0;
                }
            } else {
                if (c == '\n') {
                    _pos.column = 1;
                    ++_pos.line;
                } else {
                    ++_pos.column;
                }
            }
            return *this;
        }

        iterator operator++(int) {
            iterator old = *this;
            if (_index >= _source->_source.size())
                return old;
            int c = **this;
            if (++_pos.offset >= _source->_source.at(_index).source.size()) {
                ++_index;
                if (_index >= _source->_source.size()) {
                    _pos.file = "<eof>";
                    _pos.line = 1;
                    _pos.column = 1;
                    _pos.offset = 0;
                } else {
                    _pos.file = _source->_source.at(_index).path;
                    _pos.line = _source->_source.at(_index).line;
                    _pos.column = _source->_source.at(_index).column;
                    _pos.offset = 0;
                }
            } else {
                if (c == '\n') {
                    _pos.column = 1;
                    ++_pos.line;
                } else {
                    ++_pos.column;
                }
            }
            return old;
        }

        bool operator!=(const iterator& rhs) const {
            return !((&_source == &rhs._source) && (_index == rhs._index) && (_pos.offset == rhs._pos.offset));
        }
        bool operator==(const iterator& rhs) const {
            return (&_source == &rhs._source) && (_index == rhs._index) && (_pos.offset == rhs._pos.offset);
        }

        friend class source;
    };

    iterator begin() const {
        return iterator(*this, 0);
    }

    iterator end() const {
        return iterator(*this, _source.size());
    }

    iterator cbegin() const {
        return iterator(*this, 0);
    }

    iterator cend() const {
        return iterator(*this, _source.size());
    }

    friend class iterator;
};

class lexer {
private:
    std::vector<token> _tokens;

    static std::vector<token> tokenize(const source& source) {
        source::iterator it = source.begin();
        std::vector<token> tokens;

        static const std::unordered_map<std::string, enum token::type> operators = {
            {",", token::comma},
            {":", token::colon},
            {";", token::semicolon},
            {".", token::dot},
            {"->", token::rarrow},
            {"?", token::question},
            {"+", token::plus},
            {"-", token::minus},
            {"*", token::times},
            {"/", token::slash},
            {"%", token::modulo},
            {"++", token::increment},
            {"--", token::decrement},
            {"&", token::_and},
            {"|", token::_or},
            {"^", token::_xor},
            {"~", token::_not},
            {"<<", token::lshift},
            {">>", token::rshift},
            {"<", token::less_than},
            {"<=", token::less_equal},
            {">", token::greater_than},
            {">=", token::greater_equal},
            {"==", token::equal},
            {"!=", token::not_equal},
            {"&&", token::land},
            {"||", token::lor},
            {"(", token::lparen},
            {")", token::rparen},
            {"[", token::lbracket},
            {"]", token::rbracket},
            {"{", token::lcurly},
            {"}", token::rcurly},
            {"=", token::assign},
            {"+=", token::plus_assign},
            {"-=", token::minus_assign},
            {"*=", token::times_assign},
            {"/=", token::slash_assign},
            {"%=", token::modulo_assign},
            {"&=", token::and_assign},
            {"|=", token::or_assign},
            {"^=", token::xor_assign},
            {"<<=", token::lshift_assign},
            {">>=", token::rshift_assign}
        };

        static auto check_operator = [](source::iterator& it) -> token {
            // static constexpr std::string_view operator_symbols = "+-=<>*/()[]{}%!~^&|.,?";
            position pos = it.pos();
            std::string lexeme = "";
            for (int i = 0; i < 3; i++) {
                int c = *it;
                if (c == source::eof)
                    break;
                if (lexeme == "." && isdigit(c)) {
                    // operators have highest precedence, but allow for floating point number satisfy max munch
                    return lexer::none;
                }
                std::string tmp = lexeme;
                tmp += static_cast<char>(c);
                if (!operators.contains(tmp))
                    break;
                lexeme = tmp;
                ++it;
            }
            if (lexeme.size() == 0 || !operators.contains(lexeme))
                return lexer::none;
            return token(operators.at(lexeme), lexeme, pos);
        };

        static auto check_identifier = [](source::iterator& it) -> token {
            position pos = it.pos();
            std::string lexeme = "";
            if (*it == source::eof || !(isalpha(*it) || *it == '_'))
                return lexer::none;
            while (*it != source::eof && (isalnum(*it) || *it == '_'))
                lexeme += static_cast<char>(*it++);
            if (lexeme.size() == 0)
                return lexer::none;
            return token(token::identifier, lexeme, pos);
        };

        static auto check_numerical_literal = [](source::iterator& it) -> token {
            source::iterator old = it;
            position pos = it.pos();
            std::string lexeme = "";
            bool floating = false;
            if (*it != source::eof && *it == '.') {
                lexeme += static_cast<char>(*it++);
                floating = true;
            }
            if (*it == source::eof || !isdigit(*it)) {
                it = old;
                return lexer::none;
            }
            while (*it != source::eof && isalnum(*it))
                lexeme += static_cast<char>(*it++);
            if (*it != source::eof && *it == '.') {
                if (floating) {
                    it = old;
                    return lexer::none;
                }
                lexeme += static_cast<char>(*it++);
                floating = true;
                bool exponent = false;
                while (*it != source::eof && isalnum(*it)) {
                    int c = *it++;
                    lexeme += static_cast<char>(c);
                    if (c == 'p' || c == 'P' || c == 'e' || c == 'E') {
                        exponent = true;
                        break;
                    }
                }
                if (exponent) {
                    int c = *it;
                    if (c == '-' || c == '+')
                        lexeme += static_cast<char>(*it++);
                    while (*it != source::eof && isalnum(*it))
                        lexeme += static_cast<char>(*it++);
                }
            }
            if (lexeme.size() == 0)
                return lexer::none;
            return token(floating ? token::float_literal : token::integer_literal, lexeme, pos);
        };
        
        // Added support for string literals ("...")
        static auto check_string_literal = [](source::iterator& it) -> token {
            if (*it != '"') return lexer::none;
            position pos = it.pos();
            std::string lexeme = "";
            lexeme += static_cast<char>(*it++); // include opening quote or omit depending on style
            while (*it != source::eof && *it != '"') {
                if (*it == '\\') {
                    lexeme += static_cast<char>(*it++);
                }
                if (*it != source::eof)
                    lexeme += static_cast<char>(*it++);
            }
            if (*it == '"') {
                lexeme += static_cast<char>(*it++);
            }
            return token(token::string_literal, lexeme, pos);
        };
        
        // Added support for string literals ("...")
        static auto check_char_literal = [](source::iterator& it) -> token {
            if (*it != '\'') return lexer::none;
            position pos = it.pos();
            std::string lexeme = "";
            lexeme += static_cast<char>(*it++); // include opening quote or omit depending on style
            while (*it != source::eof && *it != '\'') {
                if (*it == '\\') {
                    lexeme += static_cast<char>(*it++);
                }
                if (*it != source::eof)
                    lexeme += static_cast<char>(*it++);
            }
            if (*it == '\'') {
                lexeme += static_cast<char>(*it++);
            }
            return token(token::char_literal, lexeme, pos);
        };

        // Main driver loop
        while (it != source.end()) {
            int c = *it;
            if (c == source::eof) break;

            if (isspace(c)) {
                while (*it != source::eof && isspace(*it))
                    ++it;
                continue;
            }

            if (c == '/') {
                source::iterator peek = it;
                ++peek;
                if (*peek == '/') {
                    while (*it != source::eof && *it != '\n')
                        ++it;
                    continue;
                } else if (*peek == '*') {
                    position pos = it.pos();
                    ++it; // skip /
                    ++it; // skip *
                    while (true) {
                        if (*it++ == '*') {
                            if (*it != source::eof && *it == '/') {
                                ++it;
                                break;
                            }
                        } else if (*it == source::eof) {
                            throw vhxcc::compile_error("unterminated comment", token(token::none, "", pos));
                        }
                    }
                }
                continue;
            }

            if (token tok = check_numerical_literal(it)) {
                tokens.push_back(tok);
                continue;
            }

            if (token tok = check_identifier(it)) {
                tokens.push_back(tok);
                continue;
            }

            if (token tok = check_operator(it)) {
                tokens.push_back(tok);
                continue;
            }

            if (token tok = check_string_literal(it)) {
                tokens.push_back(tok);
                continue;
            }

            if (token tok = check_char_literal(it)) {
                tokens.push_back(tok);
                continue;
            }

            position pos = it.pos();
            std::string bad_lexeme(1, static_cast<char>(c));
            tokens.push_back(token(token::unknown, bad_lexeme, pos));
            ++it;
        }

        position pos = it.pos();
        tokens.push_back(token(token::eof, "<eof>", pos));
        return tokens;
    }

public:
    static inline const token none = token(token::none);

    lexer() {}

    lexer(const source& source) {
        _tokens = tokenize(source);
    }

    ~lexer() {}

    lexer& append(const source& source) {
        std::vector<token> tokens = tokenize(source);
        if (!_tokens.empty())
            _tokens.pop_back();
        _tokens.insert(_tokens.end(), tokens.begin(), tokens.end());
        return *this;
    }

    lexer& append(const lexer& lexer) {
        if (!_tokens.empty())
            _tokens.pop_back();
        _tokens.insert(_tokens.end(), lexer._tokens.begin(), lexer._tokens.end());
        return *this;
    }

    lexer& append(const std::string& string) {
        source source;
        source.append(string);
        return append(source);
    }
    
    template <typename Arg>
    lexer& operator<<(const Arg& arg) {
        return append(arg);
    }

    class iterator {
        const lexer* _lexer;
        std::size_t _index;

        iterator(const lexer& lexer, std::size_t index = 0) : _lexer(&lexer), _index(index) {}

    public:
        const token& operator*() const {
            if (_index >= _lexer->_tokens.size())
                return _lexer->_tokens.back();
            return _lexer->_tokens.at(_index);
        }

        const token* operator->() const {
            if (_index >= _lexer->_tokens.size())
                return &_lexer->_tokens.back();
            return &_lexer->_tokens.at(_index);
        }

        iterator& operator++() {
            if (_index >= _lexer->_tokens.size())
                return *this;
            ++_index;
            return *this;
        }

        iterator operator++(int) {
            iterator old = *this;
            if (_index >= _lexer->_tokens.size())
                return old;
            ++_index;
            return old;
        }

        iterator& operator--() {
            if (_index == 0)
                return *this;
            --_index;
            return *this;
        }

        iterator operator--(int) {
            iterator old = *this;
            if (_index == 0)
                return old;
            --_index;
            return old;
        }

        bool operator!=(const iterator& rhs) const {
            return _index != rhs._index;
        }
        bool operator==(const iterator& rhs) const {
            return _index == rhs._index;
        }

        friend class lexer;
    };

    static const token& expect(lexer::iterator& it, std::initializer_list<enum token::type> expected) {
        const token& tok = *it;
        for (auto type : expected) {
            if (tok == type) {
                ++it;
                return tok;
            }
        }
        throw vhxcc::compile_error(std::format("unexpected token '{}'", it->lexeme()), *it); // for now until I make a token::type_str(token::type)
    }

    static const token& expect(lexer::iterator& it, std::string_view msg, std::initializer_list<enum token::type> expected) {
        const token& tok = *it;
        for (auto type : expected) {
            if (tok == type) {
                ++it;
                return tok;
            }
        }
        throw vhxcc::compile_error(msg, *it);
    }

    template <typename... Args>
    requires (std::same_as<enum token::type, Args> && ...)
    static const token& expect(lexer::iterator& it, Args... args) {
        return expect(it, {args...});
    }

    template <typename Fn>
    requires (std::invocable<Fn, const token&>)
    static const token& expect(lexer::iterator& it, Fn&& fn) {
        if (fn(*it))
            return *it++;
        throw vhxcc::compile_error(std::format("unexpected token '{}'", it->lexeme()), *it);
    }

    template <typename Fn>
    requires (std::invocable<Fn, const token&>)
    static const token& expect(lexer::iterator& it, std::string_view msg, Fn&& fn) {
        if (fn(*it)) {
            return *it++;
        }
        throw vhxcc::compile_error(msg, *it);
    }

    template <typename... Args,
              typename = std::enable_if<(std::is_same_v<enum token::type, Args> && ...)>>
    static const token& expect(lexer::iterator& it, std::string_view msg, Args... args) {
        return expect(it, msg, {args...});
    }

    static const token& match(lexer::iterator& it, std::initializer_list<enum token::type> expected) noexcept {
        const token& tok = *it;
        for (auto type : expected) {
            if (tok == type) {
                ++it;
                return tok;
            }
        }
        return lexer::none;
    }

    template <typename... Args>
    requires (std::same_as<enum token::type, Args> && ...)
    static const token& match(lexer::iterator& it, Args... args) noexcept {
        return match(it, {args...});
    }

    template <typename Fn>
    requires (std::invocable<Fn, const token&>)
    static const token& match(lexer::iterator& it, Fn&& fn) {
        if (fn(*it))
            return *it++;
        return lexer::none;
    }

    iterator begin() const {
        return iterator(*this, 0);
    }

    iterator end() const {
        return iterator(*this, _tokens.size());
    }

    iterator cbegin() const {
        return iterator(*this, 0);
    }

    iterator cend() const {
        return iterator(*this, _tokens.size());
    }
};

}