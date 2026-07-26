// Prints the prompt prefix synthesised from a specification. Needs no model,
// so it is the quickest way to see how a description reaches the parser.

#include <aiopt/prompt.hpp>
#include <aiopt/spec.hpp>

#include <iostream>
#include <string>

namespace {

enum class Level { quiet, normal, verbose };

struct Options {
    bool follow_symlinks = false;
    std::string root;
    std::string exclude;
    Level level = Level::normal;
    int depth = 8;
};

constexpr auto options = aiopt::spec<Options>(
    aiopt::flag(&Options::follow_symlinks, "follow-symlinks", "traverse symbolic links while walking the tree"),
    aiopt::path(&Options::root, "root", "directory the search starts from"),
    aiopt::text(&Options::exclude, "exclude", "glob pattern of entries to skip"),
    aiopt::choice(&Options::level, "level", "how much progress detail to print", "quiet", "normal", "verbose"),
    aiopt::number(&Options::depth, "depth", "how many directory levels to descend", 1, 64));

} // namespace

int main() {
    const auto descriptors = options.descriptors();
    std::cout << aiopt::render_prefix(descriptors) << aiopt::render_request("search ./src quietly, at most 3 deep");
    return 0;
}
