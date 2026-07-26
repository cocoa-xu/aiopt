#ifndef AIOPT_PROMPT_HPP
#define AIOPT_PROMPT_HPP

#include "aiopt/spec.hpp"

#include <span>
#include <string>

namespace aiopt {

// The prompt is split so the prefix stays byte-identical across every
// invocation of a given program. Only the prefix is worth precomputing a
// key/value cache for; the request changes with each command line.
[[nodiscard]] inline std::string render_prefix(std::span<const Descriptor> options) {
    std::string out;
    out.reserve(options.size() * 96 + 512);

    out += "You translate a command line into option assignments.\n"
           "Write one line per option that must change, in the form INDEX=VALUE.\n"
           "Write nothing for options that keep their default.\n"
           "Booleans use 1 or 0. Copy paths and text exactly as they appear in the input.\n"
           "\nOptions:\n";

    for (std::size_t i = 0; i < options.size(); ++i) {
        const Descriptor& option = options[i];
        out += std::to_string(i);
        out += ' ';
        out += option.name;
        out += " (";
        switch (option.kind) {
        case Kind::boolean:
            out += "bool";
            break;
        case Kind::integer:
            out += "int";
            if (option.bounded()) {
                out += ' ';
                out += std::to_string(option.minimum);
                out += '-';
                out += std::to_string(option.maximum);
            }
            break;
        case Kind::text:
            out += "text";
            break;
        case Kind::path:
            out += "path";
            break;
        case Kind::choice:
            out += "one of:";
            for (std::size_t c = 0; c < option.choice_count; ++c) {
                out += ' ';
                out += option.choices[c];
            }
            break;
        }
        out += ") ";
        out += option.description;
        out += '\n';
    }

    // Worked examples use this specification's own slot numbers, so the shape
    // of a correct answer is never described in the abstract.
    // Drawn from the end of the list: an example built on slot 0 teaches a
    // small model to emit "0=1" for every request it sees.
    std::size_t boolean_slot = options.size();
    std::size_t integer_slot = options.size();
    for (std::size_t i = options.size(); i > 0; --i) {
        const std::size_t index = i - 1;
        if (boolean_slot == options.size() && options[index].kind == Kind::boolean) {
            boolean_slot = index;
        }
        if (integer_slot == options.size() && options[index].kind == Kind::integer) {
            integer_slot = index;
        }
    }

    out += "\nExamples:\n\n";
    if (boolean_slot != options.size() && integer_slot != options.size()) {
        out += "input: a request that turns on ";
        out += options[boolean_slot].name;
        out += " and sets ";
        out += options[integer_slot].name;
        out += " to ";
        const std::int64_t sample = options[integer_slot].bounded() ? options[integer_slot].maximum : 7;
        out += std::to_string(sample);
        out += "\noutput:\n";
        out += std::to_string(boolean_slot);
        out += "=1\n";
        out += std::to_string(integer_slot);
        out += '=';
        out += std::to_string(sample);
        out += "\n\n";
    }
    out += "input: a request that changes nothing\noutput:\n\n";
    return out;
}

[[nodiscard]] inline std::string render_request(std::string_view command_line) {
    std::string out;
    out.reserve(command_line.size() + 24);
    out += "input: ";
    out += command_line;
    out += "\noutput:\n";
    return out;
}

} // namespace aiopt

#endif
