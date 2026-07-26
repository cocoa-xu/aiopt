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
        std::string grammar = render_grammar(descriptors);
        Engine owned = std::move(engine).value();

        if (const Status status = owned.use_grammar(grammar); status != Status::ok) {
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
        return Parser{std::move(specification), std::move(owned), std::move(prefix), std::move(instructions),
                      std::move(grammar), chat};
    }

    [[nodiscard]] Result<Outcome<target_type>> parse(std::string_view command_line, int max_tokens = 64) {
        Result<std::string> response =
            engine_.complete(chat_ ? request_for(command_line) : render_request(command_line), max_tokens);
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

    // Example requests for help text, written by the model against the same
    // specification it parses with. Uses a grammar of its own, since the answer
    // here is prose rather than an options object, and puts the parsing grammar
    // back before returning.
    //
    // Sampling here is deliberately not greedy: only a person reads these, so
    // varying the seed gives a different set each time. Parsing is restored to
    // its deterministic sampler on the way out.
    [[nodiscard]] Result<std::vector<std::string>> suggest(std::size_t count = 3,
                                                           const Sampling& sampling = {0.9f, 0.95f,
                                                                                       LLAMA_DEFAULT_SEED},
                                                           int max_tokens = 192) {
        if (const Status status = engine_.use_grammar(render_suggestions_grammar(count), sampling);
            status != Status::ok) {
            return Error{status};
        }

        Result<std::string> response = engine_.complete(request_for(render_suggestion_request(count)),
                                                        max_tokens);

        if (const Status status = engine_.use_grammar(grammar_); status != Status::ok) {
            return Error{status};
        }
        if (!response) {
            return response.error();
        }

        std::vector<std::string> examples;
        const std::string text = std::move(response).value();
        for (std::size_t at = 0; at < text.size();) {
            const std::size_t end = std::min(text.find('\n', at), text.size());
            std::string_view line{text.data() + at, end - at};
            at = end + 1;
            if (line.starts_with("- ")) {
                line.remove_prefix(2);
            }
            // A grammar admitting free text also admits the characters a turn
            // marker is spelled with, and a sampled run will occasionally write
            // "<|im_end|>" out rather than emitting the token. No usage example
            // contains "<|", so cutting there costs nothing.
            if (const std::size_t marker = line.find("<|"); marker != std::string_view::npos) {
                line = line.substr(0, marker);
            }
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
                line.remove_suffix(1);
            }
            if (!line.empty()) {
                examples.emplace_back(line);
            }
        }
        return examples;
    }

    [[nodiscard]] const std::string& prefix() const noexcept { return prefix_; }

private:
    Parser(SpecType specification, Engine engine, std::string prefix, std::string instructions,
           std::string grammar, bool chat) noexcept
        : specification_{std::move(specification)}, engine_{std::move(engine)}, prefix_{std::move(prefix)},
          instructions_{std::move(instructions)}, grammar_{std::move(grammar)}, chat_{chat} {}

    [[nodiscard]] std::string request_for(std::string_view body) const {
        if (!chat_) {
            return std::string{body};
        }
        const std::string full = apply_chat_template(engine_.model(), instructions_, body, true);
        return full.substr(std::min(prefix_.size(), full.size()));
    }

    SpecType specification_;
    Engine engine_;
    std::string prefix_;
    std::string instructions_;
    std::string grammar_;
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
