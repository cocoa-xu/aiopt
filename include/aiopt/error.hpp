#ifndef AIOPT_ERROR_HPP
#define AIOPT_ERROR_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace aiopt {

enum class Status : std::uint8_t {
    ok,
    model_unavailable,
    context_creation_failed,
    tokenization_failed,
    inference_failed,
    grammar_rejected,
    malformed_response,
};

[[nodiscard]] constexpr std::string_view describe(Status status) noexcept {
    switch (status) {
    case Status::ok:
        return "ok";
    case Status::model_unavailable:
        return "the language model could not be loaded";
    case Status::context_creation_failed:
        return "the inference context could not be created";
    case Status::tokenization_failed:
        return "the prompt could not be tokenised";
    case Status::inference_failed:
        return "inference failed";
    case Status::grammar_rejected:
        return "the decoding grammar was rejected by the sampler";
    case Status::malformed_response:
        return "the model produced an assignment the specification rejects";
    }
    return "unknown error";
}

class Error {
public:
    Error(Status status, std::string detail) noexcept : status_{status}, detail_{std::move(detail)} {}
    explicit Error(Status status) : status_{status}, detail_{describe(status)} {}

    [[nodiscard]] Status status() const noexcept { return status_; }
    [[nodiscard]] const std::string& detail() const noexcept { return detail_; }

private:
    Status status_;
    std::string detail_;
};

// A deliberately small stand-in for std::expected, which is C++23. The variant
// keeps this usable with move-only types that have no default constructor,
// which both Engine and Parser are.
template <class T>
class Result {
    static_assert(!std::is_same_v<std::remove_cvref_t<T>, Error>,
                  "Result<Error> would make the constructors ambiguous");

public:
    Result(T value) // NOLINT(google-explicit-constructor)
        : storage_{std::in_place_index<0>, std::move(value)} {}
    Result(Error error) // NOLINT(google-explicit-constructor)
        : storage_{std::in_place_index<1>, std::move(error)} {}

    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }
    [[nodiscard]] bool has_value() const noexcept { return storage_.index() == 0; }

    [[nodiscard]] const T& value() const& { return std::get<0>(storage_); }
    [[nodiscard]] T&& value() && { return std::get<0>(std::move(storage_)); }
    [[nodiscard]] const Error& error() const& { return std::get<1>(storage_); }

    [[nodiscard]] const T* operator->() const { return &std::get<0>(storage_); }
    [[nodiscard]] const T& operator*() const& { return std::get<0>(storage_); }

private:
    std::variant<T, Error> storage_;
};

} // namespace aiopt

#endif
