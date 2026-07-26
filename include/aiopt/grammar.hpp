#ifndef AIOPT_GRAMMAR_HPP
#define AIOPT_GRAMMAR_HPP

#include "aiopt/spec.hpp"

#include <span>
#include <string>

namespace aiopt {

namespace detail {

[[nodiscard]] inline std::string digit_run(std::int64_t maximum) {
    int digits = 1;
    for (std::int64_t bound = maximum; bound >= 10; bound /= 10) {
        ++digits;
    }
    std::string rule = "[0-9]";
    for (int i = 1; i < digits; ++i) {
        rule += " [0-9]?";
    }
    return rule;
}

[[nodiscard]] inline std::string quote(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 2);
    out += '"';
    for (const char character : text) {
        if (character == '"' || character == '\\') {
            out += '\\';
        }
        out += character;
    }
    out += '"';
    return out;
}

} // namespace detail

// Produces a GBNF grammar in which every slot carries the value rule implied
// by its declared type. A boolean slot can only be followed by 0 or 1, an
// enumeration only by one of its labels, and a slot number outside the
// specification cannot be written at all.
[[nodiscard]] inline std::string render_grammar(std::span<const Descriptor> options) {
    std::string out;
    out.reserve(options.size() * 96 + 256);

    out += "root ::= assignment* \"\\n\"\n";
    out += "assignment ::=";
    for (std::size_t i = 0; i < options.size(); ++i) {
        out += i == 0 ? " a" : " | a";
        out += std::to_string(i);
    }
    out += '\n';

    for (std::size_t i = 0; i < options.size(); ++i) {
        const Descriptor& option = options[i];
        const std::string index = std::to_string(i);
        out += 'a';
        out += index;
        out += " ::= \"";
        out += index;
        out += "=\" ";

        switch (option.kind) {
        case Kind::boolean:
            out += "(\"1\" | \"0\")";
            break;
        case Kind::integer:
            out += option.bounded() ? detail::digit_run(option.maximum) : std::string{"[0-9] [0-9]? [0-9]?"};
            break;
        case Kind::choice:
            out += '(';
            for (std::size_t c = 0; c < option.choice_count; ++c) {
                if (c > 0) {
                    out += " | ";
                }
                out += detail::quote(option.choices[c]);
            }
            out += ')';
            break;
        case Kind::text:
        case Kind::path:
            out += "[^\\n]+";
            break;
        }
        out += " \"\\n\"\n";
    }
    return out;
}

} // namespace aiopt

#endif
