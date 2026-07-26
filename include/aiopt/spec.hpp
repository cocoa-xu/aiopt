#ifndef AIOPT_SPEC_HPP
#define AIOPT_SPEC_HPP

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace aiopt {

enum class Kind : std::uint8_t { boolean, integer, text, path, choice, custom };

// Type-erased view of one option, used to synthesise the prompt and the
// decoding grammar. Non-owning: every string_view points at the spec, which
// outlives any use of a Descriptor.
struct Descriptor {
    std::string_view name;
    std::string_view description;
    Kind kind = Kind::text;
    std::int64_t minimum = 0;
    std::int64_t maximum = 0;
    const std::string_view* choices = nullptr;
    std::size_t choice_count = 0;

    // Set only for Kind::custom: how the value reads to a person, and the GBNF
    // rule that admits it. The library does not interpret either.
    std::string_view syntax;
    std::string_view grammar;

    [[nodiscard]] constexpr bool bounded() const noexcept {
        return kind == Kind::integer && minimum != maximum;
    }
};

namespace detail {

[[nodiscard]] constexpr bool equal_ignore_case(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const char lhs = (a[i] >= 'A' && a[i] <= 'Z') ? static_cast<char>(a[i] - 'A' + 'a') : a[i];
        const char rhs = (b[i] >= 'A' && b[i] <= 'Z') ? static_cast<char>(b[i] - 'A' + 'a') : b[i];
        if (lhs != rhs) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] constexpr bool parse_boolean(std::string_view text, bool& out) noexcept {
    for (const std::string_view yes : {"1", "true", "yes", "on"}) {
        if (equal_ignore_case(text, yes)) {
            out = true;
            return true;
        }
    }
    for (const std::string_view no : {"0", "false", "no", "off"}) {
        if (equal_ignore_case(text, no)) {
            out = false;
            return true;
        }
    }
    return false;
}

[[nodiscard]] inline bool parse_integer(std::string_view text, std::int64_t& out) noexcept {
    const char* const first = text.data();
    const char* const last = first + text.size();
    const std::from_chars_result result = std::from_chars(first, last, out);
    return result.ec == std::errc{} && result.ptr == last;
}

} // namespace detail

// Each option owns a pointer-to-member, so assignment writes straight into the
// caller's struct with no lookup, no allocation, and full type checking.
template <class Struct>
class Flag {
public:
    constexpr Flag(bool Struct::* member, std::string_view name, std::string_view description) noexcept
        : member_{member}, name_{name}, description_{description} {}

    [[nodiscard]] constexpr Descriptor describe() const noexcept {
        return Descriptor{name_, description_, Kind::boolean, 0, 0, nullptr, 0, {}, {}};
    }

    [[nodiscard]] bool assign(Struct& target, std::string_view value) const noexcept {
        return detail::parse_boolean(value, target.*member_);
    }

private:
    bool Struct::* member_;
    std::string_view name_;
    std::string_view description_;
};

template <class Struct, class Int>
class Number {
    static_assert(std::is_integral_v<Int> && !std::is_same_v<Int, bool>,
                  "Number requires an integral member that is not bool");

public:
    constexpr Number(Int Struct::* member, std::string_view name, std::string_view description,
                     std::int64_t minimum, std::int64_t maximum) noexcept
        : member_{member}, name_{name}, description_{description},
          minimum_{minimum}, maximum_{maximum} {}

    [[nodiscard]] constexpr Descriptor describe() const noexcept {
        return Descriptor{name_, description_, Kind::integer, minimum_, maximum_, nullptr, 0, {}, {}};
    }

    [[nodiscard]] bool assign(Struct& target, std::string_view value) const noexcept {
        std::int64_t parsed = 0;
        if (!detail::parse_integer(value, parsed)) {
            return false;
        }
        if (parsed < minimum_ || parsed > maximum_) {
            return false;
        }
        target.*member_ = static_cast<Int>(parsed);
        return true;
    }

private:
    Int Struct::* member_;
    std::string_view name_;
    std::string_view description_;
    std::int64_t minimum_;
    std::int64_t maximum_;
};

template <class Struct, class String, Kind TextKind>
class Text {
public:
    constexpr Text(String Struct::* member, std::string_view name, std::string_view description) noexcept
        : member_{member}, name_{name}, description_{description} {}

    [[nodiscard]] constexpr Descriptor describe() const noexcept {
        return Descriptor{name_, description_, TextKind, 0, 0, nullptr, 0, {}, {}};
    }

    [[nodiscard]] bool assign(Struct& target, std::string_view value) const {
        if (value.empty()) {
            return false;
        }
        target.*member_ = String{value};
        return true;
    }

private:
    String Struct::* member_;
    std::string_view name_;
    std::string_view description_;
};

// Labels are given in declaration order and map onto the enumerators by index,
// so the enum and the prompt can never drift apart.
template <class Struct, class Enum, std::size_t N>
class Choice {
    static_assert(std::is_enum_v<Enum>, "Choice requires an enumeration member");

public:
    constexpr Choice(Enum Struct::* member, std::string_view name, std::string_view description,
                     std::array<std::string_view, N> labels) noexcept
        : member_{member}, name_{name}, description_{description}, labels_{labels} {}

    [[nodiscard]] constexpr Descriptor describe() const noexcept {
        return Descriptor{name_, description_, Kind::choice, 0, 0, labels_.data(), N, {}, {}};
    }

    [[nodiscard]] bool assign(Struct& target, std::string_view value) const noexcept {
        for (std::size_t i = 0; i < N; ++i) {
            if (detail::equal_ignore_case(labels_[i], value)) {
                target.*member_ = static_cast<Enum>(i);
                return true;
            }
        }
        return false;
    }

private:
    Enum Struct::* member_;
    std::string_view name_;
    std::string_view description_;
    std::array<std::string_view, N> labels_;
};

// An escape hatch for values the library has no business knowing about. The
// program supplies how the value reads, the grammar that admits it, and the
// function that turns it into the member's type; aiopt carries all three
// through to the prompt, the decoder, and the help text without inspecting any
// of them.
template <class Struct, class Value>
class Custom {
public:
    using Reader = bool (*)(std::string_view, Value&);

    constexpr Custom(Value Struct::* member, std::string_view name, std::string_view description,
                     std::string_view syntax, std::string_view grammar, Reader reader) noexcept
        : member_{member}, name_{name}, description_{description}, syntax_{syntax}, grammar_{grammar},
          reader_{reader} {}

    [[nodiscard]] constexpr Descriptor describe() const noexcept {
        return Descriptor{name_, description_, Kind::custom, 0, 0, nullptr, 0, syntax_, grammar_};
    }

    [[nodiscard]] bool assign(Struct& target, std::string_view value) const {
        return reader_(value, target.*member_);
    }

private:
    Value Struct::* member_;
    std::string_view name_;
    std::string_view description_;
    std::string_view syntax_;
    std::string_view grammar_;
    Reader reader_;
};

template <class Struct, class... Options>
class Spec {
public:
    using target_type = Struct;
    static constexpr std::size_t size = sizeof...(Options);
    static_assert(size > 0, "a specification needs at least one option");

    constexpr explicit Spec(Options... options) noexcept : options_{options...} {}

    [[nodiscard]] constexpr std::array<Descriptor, size> descriptors() const noexcept {
        return std::apply([](const Options&... option) { return std::array<Descriptor, size>{option.describe()...}; },
                          options_);
    }

    // Returns false when the slot is out of range or the value does not satisfy
    // the option's declared type or bounds.
    [[nodiscard]] bool assign(Struct& target, std::size_t slot, std::string_view value) const {
        return assign_at(target, slot, value, std::index_sequence_for<Options...>{});
    }

    [[nodiscard]] bool assign(Struct& target, std::string_view name, std::string_view value) const {
        const std::array<Descriptor, size> all = descriptors();
        for (std::size_t i = 0; i < size; ++i) {
            if (all[i].name == name) {
                return assign(target, i, value);
            }
        }
        return false;
    }

private:
    template <std::size_t... I>
    [[nodiscard]] bool assign_at(Struct& target, std::size_t slot, std::string_view value,
                                 std::index_sequence<I...>) const {
        bool assigned = false;
        const auto try_slot = [&]<std::size_t Index>() {
            if (slot != Index) {
                return false;
            }
            assigned = std::get<Index>(options_).assign(target, value);
            return true;
        };
        (void)(try_slot.template operator()<I>() || ...);
        return assigned;
    }

    std::tuple<Options...> options_;
};

template <class Struct>
[[nodiscard]] constexpr auto flag(bool Struct::* member, std::string_view name,
                                  std::string_view description) noexcept {
    return Flag<Struct>{member, name, description};
}

template <class Struct, class Int>
[[nodiscard]] constexpr auto number(Int Struct::* member, std::string_view name, std::string_view description,
                                    std::int64_t minimum, std::int64_t maximum) noexcept {
    return Number<Struct, Int>{member, name, description, minimum, maximum};
}

template <class Struct, class String>
[[nodiscard]] constexpr auto text(String Struct::* member, std::string_view name,
                                  std::string_view description) noexcept {
    return Text<Struct, String, Kind::text>{member, name, description};
}

template <class Struct, class String>
[[nodiscard]] constexpr auto path(String Struct::* member, std::string_view name,
                                  std::string_view description) noexcept {
    return Text<Struct, String, Kind::path>{member, name, description};
}

template <class Struct, class Enum, class... Labels>
[[nodiscard]] constexpr auto choice(Enum Struct::* member, std::string_view name, std::string_view description,
                                    Labels... labels) noexcept {
    return Choice<Struct, Enum, sizeof...(Labels)>{
        member, name, description, std::array<std::string_view, sizeof...(Labels)>{std::string_view{labels}...}};
}

template <class Struct, class Value>
[[nodiscard]] constexpr auto custom(Value Struct::* member, std::string_view name,
                                    std::string_view description, std::string_view syntax,
                                    std::string_view grammar,
                                    bool (*reader)(std::string_view, Value&)) noexcept {
    return Custom<Struct, Value>{member, name, description, syntax, grammar, reader};
}

template <class Struct, class... Options>
[[nodiscard]] constexpr auto spec(Options... options) noexcept {
    return Spec<Struct, Options...>{options...};
}

} // namespace aiopt

#endif
