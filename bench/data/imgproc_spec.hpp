#ifndef AIOPT_BENCH_IMGPROC_SPEC_HPP
#define AIOPT_BENCH_IMGPROC_SPEC_HPP

#include <aiopt/spec.hpp>

#include <array>
#include <string>
#include <string_view>

namespace bench {

enum class Format { png, webp, avif };

struct Options {
    bool recursive = false;
    bool dry_run = false;
    bool strip_metadata = false;
    bool overwrite = false;
    std::string output;
    Format format = Format::png;
    int quality = 80;
    int jobs = 1;
    int max_width = 0;
};

inline constexpr auto specification = aiopt::spec<Options>(
    aiopt::flag(&Options::recursive, "recursive", "descend into subdirectories when collecting inputs"),
    aiopt::flag(&Options::dry_run, "dry-run", "report what would happen without writing any file"),
    aiopt::flag(&Options::strip_metadata, "strip-metadata", "remove EXIF and other metadata from output"),
    aiopt::flag(&Options::overwrite, "overwrite", "replace files that already exist in the output directory"),
    aiopt::path(&Options::output, "output", "directory to write processed images into"),
    aiopt::choice(&Options::format, "format", "output encoding", "png", "webp", "avif"),
    aiopt::number(&Options::quality, "quality", "compression quality, higher means larger files", 1, 100),
    aiopt::number(&Options::jobs, "jobs", "number of parallel worker threads", 1, 256),
    aiopt::number(&Options::max_width, "max-width", "downscale images wider than this many pixels", 0, 16384));

// Scoring treats "the option was set" as "it differs from its default", so a
// case that expects a default and gets one is neither credited nor penalised.
struct Field {
    std::string_view name;
    bool (*differs)(const Options&, const Options&);
};

inline constexpr std::array<Field, 9> fields{{
    {"recursive", [](const Options& a, const Options& b) { return a.recursive != b.recursive; }},
    {"dry-run", [](const Options& a, const Options& b) { return a.dry_run != b.dry_run; }},
    {"strip-metadata", [](const Options& a, const Options& b) { return a.strip_metadata != b.strip_metadata; }},
    {"overwrite", [](const Options& a, const Options& b) { return a.overwrite != b.overwrite; }},
    {"output", [](const Options& a, const Options& b) { return a.output != b.output; }},
    {"format", [](const Options& a, const Options& b) { return a.format != b.format; }},
    {"quality", [](const Options& a, const Options& b) { return a.quality != b.quality; }},
    {"jobs", [](const Options& a, const Options& b) { return a.jobs != b.jobs; }},
    {"max-width", [](const Options& a, const Options& b) { return a.max_width != b.max_width; }},
}};

} // namespace bench

#endif
