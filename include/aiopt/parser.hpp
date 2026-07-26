#ifndef AIOPT_PARSER_HPP
#define AIOPT_PARSER_HPP

#include "aiopt/engine.hpp"
#include "aiopt/error.hpp"
#include "aiopt/prompt.hpp"
#include "aiopt/spec.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aiopt {

struct Assignment {
    std::size_t slot = 0;
    std::string_view value;
};

// Recognises the INDEX=VALUE lines the prompt asks for and ignores anything
// else, so stray prose costs an assignment rather than the whole parse.
[[nodiscard]] inline std::vector<Assignment> read_assignments(std::string_view response) {
    std::vector<Assignment> assignments;
    std::size_t position = 0;

    while (position < response.size()) {
        const std::size_t line_end = std::min(response.find('\n', position), response.size());
        const std::string_view line = response.substr(position, line_end - position);
        position = line_end + 1;

        const std::size_t separator = line.find('=');
        if (separator == std::string_view::npos || separator == 0) {
            continue;
        }

        std::size_t slot = 0;
        bool numeric = true;
        for (const char digit : line.substr(0, separator)) {
            if (digit < '0' || digit > '9') {
                numeric = false;
                break;
            }
            slot = slot * 10 + static_cast<std::size_t>(digit - '0');
        }
        if (!numeric) {
            continue;
        }

        std::string_view value = line.substr(separator + 1);
        while (!value.empty() && (value.back() == '\r' || value.back() == ' ')) {
            value.remove_suffix(1);
        }
        if (!value.empty()) {
            assignments.push_back(Assignment{slot, value});
        }
    }
    return assignments;
}

template <class Target>
struct Outcome {
    Target options{};
    std::string response;
    int accepted = 0;
    int rejected = 0;
};

template <class SpecType>
class Parser {
public:
    using target_type = typename SpecType::target_type;

    [[nodiscard]] static Result<Parser> create(SpecType specification, const std::string& model_path,
                                               const EngineOptions& options = {}) {
        Result<Engine> engine = Engine::open(model_path, options);
        if (!engine) {
            return engine.error();
        }

        std::string prefix = render_prefix(specification.descriptors());
        Engine owned = std::move(engine).value();
        if (const Status status = owned.prime(prefix); status != Status::ok) {
            return Error{status};
        }
        return Parser{std::move(specification), std::move(owned), std::move(prefix)};
    }

    [[nodiscard]] Result<Outcome<target_type>> parse(std::string_view command_line, int max_tokens = 64) {
        Result<std::string> response = engine_.complete(render_request(command_line), max_tokens);
        if (!response) {
            return response.error();
        }

        Outcome<target_type> outcome;
        outcome.response = std::move(response).value();
        for (const Assignment& assignment : read_assignments(outcome.response)) {
            if (specification_.assign(outcome.options, assignment.slot, assignment.value)) {
                ++outcome.accepted;
            } else {
                ++outcome.rejected;
            }
        }
        return outcome;
    }

    [[nodiscard]] const std::string& prefix() const noexcept { return prefix_; }

private:
    Parser(SpecType specification, Engine engine, std::string prefix) noexcept
        : specification_{std::move(specification)}, engine_{std::move(engine)}, prefix_{std::move(prefix)} {}

    SpecType specification_;
    Engine engine_;
    std::string prefix_;
};

// Deduces the specification type, so callers never have to spell out
// Parser<decltype(spec)> — which would carry the const that constexpr implies.
template <class SpecType>
[[nodiscard]] Result<Parser<SpecType>> make_parser(SpecType specification, const std::string& model_path,
                                                   const EngineOptions& options = {}) {
    return Parser<SpecType>::create(std::move(specification), model_path, options);
}

} // namespace aiopt

#endif
