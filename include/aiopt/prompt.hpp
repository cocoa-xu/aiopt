#ifndef AIOPT_PROMPT_HPP
#define AIOPT_PROMPT_HPP

#include "aiopt/spec.hpp"

#include <algorithm>
#include <array>
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
    "Most requests set one or two options.\n"
#endif

// A declared type is information the caller already gave us, and stating it
// plainly costs a few tokens while removing a whole class of guesswork. Each
// block is emitted only when the specification actually uses that type, so a
// program with no paths never pays for the paragraph about paths.
#ifndef AIOPT_GUIDANCE_BOOLEAN
#define AIOPT_GUIDANCE_BOOLEAN                                                           \
    "Boolean keys take true or false, never a string and never a number.\n"              \
    "If the request does not ask about one, leave the key out entirely.\n"
#endif

#ifndef AIOPT_GUIDANCE_INTEGER
#define AIOPT_GUIDANCE_INTEGER                                                           \
    "Number keys take bare digits, not a quoted string, and must fall inside the\n"      \
    "range shown. Take the number from the request; never invent or round one.\n"
#endif

#ifndef AIOPT_GUIDANCE_CHOICE
#define AIOPT_GUIDANCE_CHOICE                                                            \
    "Choice keys take exactly one of the listed values, spelled exactly as listed.\n"     \
    "If the request does not clearly point at one of them, leave the key out.\n"
#endif

#ifndef AIOPT_GUIDANCE_PATH
#define AIOPT_GUIDANCE_PATH                                                              \
    "Path keys take a path that already appears in the request. Copy it character\n"     \
    "for character, keeping any leading ./ or ../ or ~/ exactly as written. Never\n"     \
    "invent a path, never expand one into an absolute path such as /home/... or\n"       \
    "/tmp/..., and never complete a partial one. If the request names no path,\n"        \
    "leave the key out.\n"
#endif

#ifndef AIOPT_GUIDANCE_TEXT
#define AIOPT_GUIDANCE_TEXT                                                              \
    "Text keys take words that already appear in the request, copied exactly.\n"         \
    "Never paraphrase and never invent a value.\n"
#endif

// Governs the example requests shown in help. Override it to change their
// flavour, for instance to ask for examples in a particular language.
#ifndef AIOPT_EXAMPLES_PROMPT
#define AIOPT_EXAMPLES_PROMPT                                                            \
    "Now write example requests a person could type to run this program.\n"              \
    "Put each on its own line beginning with \"- \".\n"                                  \
    "Use only the keys listed above, and have each example use a different mix\n"        \
    "of them. Write natural sentences, not JSON. Keep each under twelve words.\n"
#endif

namespace aiopt {

[[nodiscard]] constexpr std::string_view default_system_prompt() noexcept {
    return AIOPT_SYSTEM_PROMPT;
}

[[nodiscard]] constexpr std::string_view default_examples_prompt() noexcept {
    return AIOPT_EXAMPLES_PROMPT;
}

[[nodiscard]] inline std::string render_suggestion_request(std::size_t count,
                                                           std::string_view instructions =
                                                               default_examples_prompt()) {
    std::string out;
    out.reserve(instructions.size() + 64);
    out += instructions;
    out += "Write exactly ";
    out += std::to_string(count);
    out += " of them.\n";
    return out;
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

[[nodiscard]] constexpr std::string_view guidance_for(Kind kind) noexcept {
    switch (kind) {
    case Kind::boolean:
        return AIOPT_GUIDANCE_BOOLEAN;
    case Kind::integer:
        return AIOPT_GUIDANCE_INTEGER;
    case Kind::choice:
        return AIOPT_GUIDANCE_CHOICE;
    case Kind::path:
        return AIOPT_GUIDANCE_PATH;
    case Kind::text:
        return AIOPT_GUIDANCE_TEXT;
    }
    return {};
}

inline void append_guidance(std::string& out, std::span<const Descriptor> options) {
    constexpr std::array order{Kind::boolean, Kind::integer, Kind::choice, Kind::path, Kind::text};
    bool heading = false;

    for (const Kind kind : order) {
        const bool used = std::any_of(options.begin(), options.end(),
                                      [kind](const Descriptor& option) { return option.kind == kind; });
        if (!used) {
            continue;
        }
        if (!heading) {
            out += "\nRules:\n";
            heading = true;
        }
        out += guidance_for(kind);
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

    detail::append_guidance(out, options);

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
