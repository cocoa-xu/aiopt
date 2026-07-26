#ifndef AIOPT_HELP_HPP
#define AIOPT_HELP_HPP

#include "aiopt/spec.hpp"

#include <algorithm>
#include <span>
#include <string>
#include <string_view>

namespace aiopt {

namespace detail {

[[nodiscard]] inline std::string value_summary(const Descriptor& option) {
    switch (option.kind) {
    case Kind::boolean:
        return "yes or no";
    case Kind::integer:
        return option.bounded() ? std::to_string(option.minimum) + " to " + std::to_string(option.maximum)
                                : std::string{"a number"};
    case Kind::text:
        return "text";
    case Kind::path:
        return "a path";
    case Kind::choice: {
        std::string out;
        for (std::size_t c = 0; c < option.choice_count; ++c) {
            if (c > 0) {
                out += " | ";
            }
            out += option.choices[c];
        }
        return out;
    }
    }
    return {};
}

} // namespace detail

// Help for a program whose command line is prose. There are no flags to list,
// so what a reader needs is the vocabulary: which things can be asked for, and
// what values each will accept. Both come from the same specification the model
// is shown, so the two can never describe different programs.
[[nodiscard]] inline std::string render_help(std::span<const Descriptor> options, std::string_view program,
                                             std::string_view summary = {},
                                             std::span<const std::string_view> examples = {}) {
    std::string out;
    out.reserve(options.size() * 96 + 512);

    out += program;
    if (!summary.empty()) {
        out += " — ";
        out += summary;
    }
    out += "\n\nSay what you want in plain language:\n\n  ";
    out += program;
    out += " \"...\"\n";

    if (!examples.empty()) {
        out += "\nFor example:\n";
        for (const std::string_view example : examples) {
            out += "\n  ";
            out += program;
            out += " \"";
            out += example;
            out += '"';
        }
        out += '\n';
    }

    std::size_t name_width = 0;
    std::size_t value_width = 0;
    for (const Descriptor& option : options) {
        name_width = std::max(name_width, option.name.size());
        value_width = std::max(value_width, detail::value_summary(option).size());
    }

    out += "\nWhat a request can set:\n\n";
    for (const Descriptor& option : options) {
        const std::string values = detail::value_summary(option);
        out += "  ";
        out += option.name;
        out.append(name_width - option.name.size() + 2, ' ');
        out += values;
        out.append(value_width - values.size() + 2, ' ');
        out += option.description;
        out += '\n';
    }

    out += "\nAnything a request does not mention keeps its default.\n";
    return out;
}

} // namespace aiopt

#endif
