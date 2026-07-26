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

[[nodiscard]] inline std::string quoted_literal(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 8);
    out += "\"\\\"\"";
    out += " \"";
    for (const char character : text) {
        if (character == '"' || character == '\\') {
            out += '\\';
        }
        out += character;
    }
    out += "\" \"\\\"\"";
    return out;
}

[[nodiscard]] inline std::string rule_name(std::string_view option) {
    std::string out = "k-";
    for (const char character : option) {
        out += (character == '_' || character == ' ') ? '-' : character;
    }
    return out;
}

} // namespace detail

// A JSON object keyed by option name. The names are the instruction channel:
// "quality" tells a model what the field means in a way an ordinal never can,
// and JSON is the shape models are most heavily trained to emit. Each key
// carries the value rule implied by its declared type, so a boolean admits
// only true or false and an enumeration only its own labels.
[[nodiscard]] inline std::string render_grammar(std::span<const Descriptor> options) {
    std::string out;
    out.reserve(options.size() * 128 + 256);

    out += "root ::= \"{\" ws (pair (\",\" ws pair)*)? ws \"}\"\n";
    out += "ws ::= [ \\t\\n]*\n";
    out += "pair ::=";
    for (std::size_t i = 0; i < options.size(); ++i) {
        out += i == 0 ? " " : " | ";
        out += detail::rule_name(options[i].name);
    }
    out += '\n';

    for (const Descriptor& option : options) {
        out += detail::rule_name(option.name);
        out += " ::= ";
        out += detail::quoted_literal(option.name);
        out += " ws \":\" ws ";

        switch (option.kind) {
        case Kind::boolean:
            out += "(\"true\" | \"false\")";
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
                out += detail::quoted_literal(option.choices[c]);
            }
            out += ')';
            break;
        case Kind::text:
        case Kind::path:
            out += "\"\\\"\" [^\"\\n]+ \"\\\"\"";
            break;
        case Kind::custom:
            out += option.grammar;
            break;
        }
        out += " ws\n";
    }
    return out;
}

// Example requests are prose rather than an options object, so they need a
// grammar of their own: exactly `count` lines, each opening with a dash. The
// shape is fixed while the wording stays free.
[[nodiscard]] inline std::string render_suggestions_grammar(std::size_t count) {
    std::string out = "root ::=";
    for (std::size_t i = 0; i < count; ++i) {
        out += " item";
    }
    // Bounded rather than open-ended: an unbounded line lets a sampled run
    // ramble on inside one example, padding it out with repeated fragments
    // instead of stopping.
    out += "\nitem ::= \"- \" [^\\n]{4,110} \"\\n\"\n";
    return out;
}

} // namespace aiopt

#endif
