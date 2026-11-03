#pragma once
#include <token.hpp>
#include <stdexcept>
#include <string>


namespace vhxcc {
    class compile_error : public std::runtime_error {
        vhxcc::token tok;
    public:
        explicit compile_error(std::string_view message, const vhxcc::token& tok = vhxcc::token()) : std::runtime_error(std::string(message)), tok(tok) {}
        const vhxcc::token& token() const { return tok; }
    };
}