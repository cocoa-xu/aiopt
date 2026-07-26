#ifndef AIOPT_PARSER_HPP
#define AIOPT_PARSER_HPP

#include "aiopt/engine.hpp"
#include "aiopt/error.hpp"
#include "aiopt/grammar.hpp"
#include "aiopt/prompt.hpp"
#include "aiopt/spec.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aiopt {

struct Assignment {
    std::string_view name;
    std::string_view value;
};

namespace detail {

constexpr void skip_space(std::string_view text, std::size_t& at) noexcept {
    while (at < text.size() && (text[at] == ' ' || text[at] == '\t' || text[at] == '\n' || text[at] == '\r')) {
        ++at;
    }
}

// Reads a JSON string body, leaving `at` past the closing quote. Escapes are
// not unescaped: the grammar forbids them inside values, and a path that
// contained one should reach the specification exactly as written.
[[nodiscard]] constexpr std::string_view read_string(std::string_view text, std::size_t& at) noexcept {
    if (at >= text.size() || text[at] != '"') {
        return {};
    }
    const std::size_t begin = ++at;
    while (at < text.size() && text[at] != '"') {
        ++at;
    }
    const std::string_view body = text.substr(begin, at - begin);
    if (at < text.size()) {
        ++at;
    }
    return body;
}

} // namespace detail

// Reads a flat JSON object. The grammar already guarantees well-formedness, so
// this only has to be careful, not defensive: anything it cannot read is
// dropped rather than failing the whole parse.
[[nodiscard]] inline std::vector<Assignment> read_assignments(std::string_view response) {
    std::vector<Assignment> assignments;

    std::size_t at = response.find('{');
    if (at == std::string_view::npos) {
        return assignments;
    }
    ++at;

    while (at < response.size()) {
        detail::skip_space(response, at);
        if (at >= response.size() || response[at] == '}') {
            break;
        }
        if (response[at] == ',') {
            ++at;
            continue;
        }

        const std::string_view key = detail::read_string(response, at);
        if (key.empty()) {
            break;
        }

        detail::skip_space(response, at);
        if (at >= response.size() || response[at] != ':') {
            break;
        }
        ++at;
        detail::skip_space(response, at);
        if (at >= response.size()) {
            break;
        }

        std::string_view value;
        if (response[at] == '"') {
            value = detail::read_string(response, at);
        } else {
            const std::size_t begin = at;
            while (at < response.size() && response[at] != ',' && response[at] != '}' &&
                   response[at] != ' ' && response[at] != '\n') {
                ++at;
            }
            value = response.substr(begin, at - begin);
        }

        if (!value.empty()) {
            assignments.push_back(Assignment{key, value});
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

        const auto descriptors = specification.descriptors();
        std::string instructions = render_prefix(descriptors);
        Engine owned = std::move(engine).value();

        if (const Status status = owned.use_grammar(render_grammar(descriptors)); status != Status::ok) {
            return Error{status};
        }

        // A chat template is only usable for prefix reuse if the system turn is
        // a literal prefix of the full exchange. Some templates fold the system
        // message into the first user turn, which would break that; those fall
        // back to plain completion.
        std::string prefix = apply_chat_template(owned.model(), instructions, {}, false);
        const std::string probe = apply_chat_template(owned.model(), instructions, "probe", true);
        const bool chat = !prefix.empty() && !probe.empty() && probe.compare(0, prefix.size(), prefix) == 0;
        if (!chat) {
            prefix = instructions;
        }

        if (const Status status = owned.prime(prefix); status != Status::ok) {
            return Error{status};
        }
        return Parser{std::move(specification), std::move(owned), std::move(prefix), std::move(instructions), chat};
    }

    [[nodiscard]] Result<Outcome<target_type>> parse(std::string_view command_line, int max_tokens = 64) {
        std::string request;
        if (chat_) {
            const std::string full = apply_chat_template(engine_.model(), instructions_, command_line, true);
            request = full.substr(std::min(prefix_.size(), full.size()));
        } else {
            request = render_request(command_line);
        }

        Result<std::string> response = engine_.complete(request, max_tokens);
        if (!response) {
            return response.error();
        }

        Outcome<target_type> outcome;
        outcome.response = std::move(response).value();
        for (const Assignment& assignment : read_assignments(outcome.response)) {
            if (specification_.assign(outcome.options, assignment.name, assignment.value)) {
                ++outcome.accepted;
            } else {
                ++outcome.rejected;
            }
        }
        return outcome;
    }

    [[nodiscard]] const std::string& prefix() const noexcept { return prefix_; }

private:
    Parser(SpecType specification, Engine engine, std::string prefix, std::string instructions,
           bool chat) noexcept
        : specification_{std::move(specification)}, engine_{std::move(engine)}, prefix_{std::move(prefix)},
          instructions_{std::move(instructions)}, chat_{chat} {}

    SpecType specification_;
    Engine engine_;
    std::string prefix_;
    std::string instructions_;
    bool chat_ = false;
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
