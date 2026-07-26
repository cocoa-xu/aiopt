#ifndef AIOPT_PROMPT_HPP
#define AIOPT_PROMPT_HPP

#include "aiopt/spec.hpp"

#include <span>
#include <string>

// Define AIOPT_SYSTEM_PROMPT before including this header to replace the
// standing instructions wholesale; pass an argument to render_prefix to
// replace them for one call. The option list, worked examples, and answer
// format are appended either way, since the grammar depends on them.
#ifndef AIOPT_SYSTEM_PROMPT
#define AIOPT_SYSTEM_PROMPT                                                              \
    "You turn a request written in prose into a JSON object of command line options.\n"  \
    "Reply with one JSON object and nothing else.\n"                                     \
    "Include a key only when the request asks for it. Omit every other key.\n"           \
    "Most requests set one or two options. Copy paths and text exactly.\n"
#endif

namespace aiopt {

[[nodiscard]] constexpr std::string_view default_system_prompt() noexcept {
    return AIOPT_SYSTEM_PROMPT;
}

namespace detail {

inline void describe_type(std::string& out, const Descriptor& option) {
    switch (option.kind) {
    case Kind::boolean:
        out += "true or false";
        break;
    case Kind::integer:
        out += "a whole number";
        if (option.bounded()) {
            out += " from ";
            out += std::to_string(option.minimum);
            out += " to ";
            out += std::to_string(option.maximum);
        }
        break;
    case Kind::text:
        out += "a string";
        break;
    case Kind::path:
        out += "a path, as a string";
        break;
    case Kind::choice:
        out += "one of";
        for (std::size_t c = 0; c < option.choice_count; ++c) {
            out += c == 0 ? " \"" : ", \"";
            out += option.choices[c];
            out += '"';
        }
        break;
    }
}

} // namespace detail

// The prompt is split so the prefix stays byte-identical across every
// invocation of a given program. Only the prefix is worth precomputing a
// key/value cache for; the request changes with each command line.
[[nodiscard]] inline std::string render_prefix(std::span<const Descriptor> options,
                                               std::string_view instructions = default_system_prompt()) {
    std::string out;
    out.reserve(options.size() * 112 + instructions.size() + 512);

    out += instructions;
    out += "\nKeys:\n";
    for (const Descriptor& option : options) {
        out += "  \"";
        out += option.name;
        out += "\": ";
        detail::describe_type(out, option);
        out += "  — ";
        out += option.description;
        out += '\n';
    }

    // Worked examples are built from this specification's own key names, so the
    // shape of a correct answer is never described in the abstract.
    const Descriptor* boolean_key = nullptr;
    const Descriptor* integer_key = nullptr;
    for (const Descriptor& option : options) {
        if (boolean_key == nullptr && option.kind == Kind::boolean) {
            boolean_key = &option;
        }
        if (integer_key == nullptr && option.kind == Kind::integer) {
            integer_key = &option;
        }
    }

    out += "\nExamples:\n\n";
    if (boolean_key != nullptr && integer_key != nullptr) {
        const std::int64_t sample = integer_key->bounded() ? integer_key->maximum : 7;
        out += "request: turn on ";
        out += boolean_key->name;
        out += " and set ";
        out += integer_key->name;
        out += " to ";
        out += std::to_string(sample);
        out += "\nanswer: {\"";
        out += boolean_key->name;
        out += "\": true, \"";
        out += integer_key->name;
        out += "\": ";
        out += std::to_string(sample);
        out += "}\n\n";
    }
    out += "request: leave everything as it is\nanswer: {}\n\n";
    return out;
}

[[nodiscard]] inline std::string render_request(std::string_view command_line) {
    std::string out;
    out.reserve(command_line.size() + 24);
    out += "request: ";
    out += command_line;
    out += "\nanswer: ";
    return out;
}

} // namespace aiopt

#endif
