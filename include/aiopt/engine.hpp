#ifndef AIOPT_ENGINE_HPP
#define AIOPT_ENGINE_HPP

#include "aiopt/error.hpp"

#include <llama.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace aiopt {

struct EngineOptions {
    std::uint32_t context_length = 4096;
    std::int32_t threads = 4;
    std::int32_t gpu_layers = 0;
    bool quiet = true;
};

namespace detail {

struct ModelDeleter {
    void operator()(llama_model* model) const noexcept { llama_model_free(model); }
};

struct ContextDeleter {
    void operator()(llama_context* context) const noexcept { llama_free(context); }
};

// Initialised on first use rather than by a global constructor, so merely
// linking against this library never runs code before main().
inline void ensure_backend(bool quiet) {
    static const bool initialised = [quiet] {
        if (quiet) {
            llama_log_set([](ggml_log_level, const char*, void*) {}, nullptr);
        }
        llama_backend_init();
        return true;
    }();
    (void)initialised;
}

} // namespace detail

// Owns a model and one inference context. The prompt prefix is decoded once in
// prime(); every later completion reuses that key/value cache and only pays for
// the request and the tokens it generates.
class Engine {
public:
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) noexcept = default;
    Engine& operator=(Engine&&) noexcept = default;
    ~Engine() = default;

    [[nodiscard]] static Result<Engine> open(const std::string& model_path, const EngineOptions& options) {
        detail::ensure_backend(options.quiet);

        llama_model_params model_params = llama_model_default_params();
        model_params.n_gpu_layers = options.gpu_layers;

        std::unique_ptr<llama_model, detail::ModelDeleter> model{
            llama_model_load_from_file(model_path.c_str(), model_params)};
        if (!model) {
            return Error{Status::model_unavailable, "cannot load model from " + model_path};
        }

        llama_context_params context_params = llama_context_default_params();
        context_params.n_ctx = options.context_length;
        context_params.n_batch = options.context_length;
        context_params.n_threads = options.threads;
        context_params.n_threads_batch = options.threads;

        std::unique_ptr<llama_context, detail::ContextDeleter> context{
            llama_init_from_model(model.get(), context_params)};
        if (!context) {
            return Error{Status::context_creation_failed};
        }

        return Engine{std::move(model), std::move(context)};
    }

    [[nodiscard]] Status prime(std::string_view prefix) {
        const std::vector<llama_token> tokens = tokenize(prefix, true);
        if (tokens.empty()) {
            return Status::tokenization_failed;
        }
        clear_from(0);
        if (!decode(tokens)) {
            return Status::inference_failed;
        }
        prefix_length_ = static_cast<llama_pos>(tokens.size());
        return Status::ok;
    }

    [[nodiscard]] Result<std::string> complete(std::string_view request, int max_tokens) {
        clear_from(prefix_length_);

        const std::vector<llama_token> tokens = tokenize(request, prefix_length_ == 0);
        if (tokens.empty()) {
            return Error{Status::tokenization_failed};
        }
        if (!decode(tokens)) {
            return Error{Status::inference_failed};
        }

        const llama_vocab* vocab = llama_model_get_vocab(model_.get());
        const int vocab_size = llama_vocab_n_tokens(vocab);

        std::string out;
        for (int produced = 0; produced < max_tokens; ++produced) {
            const float* logits = llama_get_logits_ith(context_.get(), -1);
            if (logits == nullptr) {
                return Error{Status::inference_failed};
            }

            llama_token best = 0;
            for (int token = 1; token < vocab_size; ++token) {
                if (logits[token] > logits[best]) {
                    best = token;
                }
            }
            if (llama_vocab_is_eog(vocab, best)) {
                break;
            }

            out += piece(vocab, best);
            if (out.size() >= 2 && out.compare(out.size() - 2, 2, "\n\n") == 0) {
                break;
            }
            if (!decode(std::vector<llama_token>{best})) {
                return Error{Status::inference_failed};
            }
        }
        return out;
    }

    [[nodiscard]] llama_pos prefix_length() const noexcept { return prefix_length_; }

private:
    Engine(std::unique_ptr<llama_model, detail::ModelDeleter> model,
           std::unique_ptr<llama_context, detail::ContextDeleter> context) noexcept
        : model_{std::move(model)}, context_{std::move(context)} {}

    [[nodiscard]] std::vector<llama_token> tokenize(std::string_view text, bool add_special) const {
        const llama_vocab* vocab = llama_model_get_vocab(model_.get());
        const auto length = static_cast<std::int32_t>(text.size());

        std::vector<llama_token> tokens(text.size() + 8);
        const std::int32_t count =
            llama_tokenize(vocab, text.data(), length, tokens.data(),
                           static_cast<std::int32_t>(tokens.size()), add_special, false);
        if (count < 0) {
            return {};
        }
        tokens.resize(static_cast<std::size_t>(count));
        return tokens;
    }

    [[nodiscard]] bool decode(const std::vector<llama_token>& tokens) {
        auto* mutable_tokens = const_cast<llama_token*>(tokens.data());
        const auto count = static_cast<std::int32_t>(tokens.size());
        return llama_decode(context_.get(), llama_batch_get_one(mutable_tokens, count)) == 0;
    }

    void clear_from(llama_pos position) {
        llama_memory_seq_rm(llama_get_memory(context_.get()), 0, position, -1);
    }

    [[nodiscard]] static std::string piece(const llama_vocab* vocab, llama_token token) {
        std::string buffer(16, '\0');
        std::int32_t written = llama_token_to_piece(vocab, token, buffer.data(),
                                                    static_cast<std::int32_t>(buffer.size()), 0, false);
        if (written < 0) {
            buffer.resize(static_cast<std::size_t>(-written));
            written = llama_token_to_piece(vocab, token, buffer.data(),
                                           static_cast<std::int32_t>(buffer.size()), 0, false);
        }
        if (written < 0) {
            return {};
        }
        buffer.resize(static_cast<std::size_t>(written));
        return buffer;
    }

    std::unique_ptr<llama_model, detail::ModelDeleter> model_;
    std::unique_ptr<llama_context, detail::ContextDeleter> context_;
    llama_pos prefix_length_ = 0;
};

} // namespace aiopt

#endif
