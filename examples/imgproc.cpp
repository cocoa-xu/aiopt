// A complete aiopt program: declare the options once, then accept the command
// line as ordinary prose.
//
//   ./imgproc model.gguf "shrink everything in ./photos to webp at quality 80"

#include <aiopt/aiopt.hpp>

#include <iostream>
#include <string>

namespace {

enum class Format { png, webp, avif };

struct Options {
    bool recursive = false;
    bool dry_run = false;
    bool strip_metadata = false;
    std::string output;
    Format format = Format::png;
    int quality = 80;
    int jobs = 1;
    int max_width = 0;
};

constexpr auto options = aiopt::spec<Options>(
    aiopt::flag(&Options::recursive, "recursive", "descend into subdirectories when collecting inputs"),
    aiopt::flag(&Options::dry_run, "dry-run", "report what would happen without writing any file"),
    aiopt::flag(&Options::strip_metadata, "strip-metadata", "remove EXIF and other metadata from output"),
    aiopt::path(&Options::output, "output", "directory to write processed images into"),
    aiopt::choice(&Options::format, "format", "output encoding", "png", "webp", "avif"),
    aiopt::number(&Options::quality, "quality", "compression quality, higher means larger files", 1, 100),
    aiopt::number(&Options::jobs, "jobs", "number of parallel worker threads", 1, 256),
    aiopt::number(&Options::max_width, "max-width", "downscale images wider than this many pixels", 0, 16384));

constexpr std::string_view name(Format format) noexcept {
    switch (format) {
    case Format::png:
        return "png";
    case Format::webp:
        return "webp";
    case Format::avif:
        return "avif";
    }
    return "png";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0] << " <model.gguf> <what you want done>\n";
        return 2;
    }

    aiopt::EngineOptions engine;
    engine.threads = 10;

    auto parser = aiopt::make_parser(options, argv[1], engine);
    if (!parser) {
        std::cerr << "aiopt: " << parser.error().detail() << '\n';
        return 1;
    }

    auto resolver = std::move(parser).value();
    auto outcome = resolver.parse(argv[2]);
    if (!outcome) {
        std::cerr << "aiopt: " << outcome.error().detail() << '\n';
        return 1;
    }

    const Options& resolved = outcome->options;
    std::cout << "recursive       " << std::boolalpha << resolved.recursive << '\n'
              << "dry-run         " << resolved.dry_run << '\n'
              << "strip-metadata  " << resolved.strip_metadata << '\n'
              << "output          " << (resolved.output.empty() ? "(unset)" : resolved.output) << '\n'
              << "format          " << name(resolved.format) << '\n'
              << "quality         " << resolved.quality << '\n'
              << "jobs            " << resolved.jobs << '\n'
              << "max-width       " << resolved.max_width << '\n';

    if (outcome->rejected > 0) {
        std::cerr << "note: " << outcome->rejected << " assignment(s) rejected by the specification\n";
    }
    return 0;
}
