#ifndef AIOPT_BENCH_CASES_HPP
#define AIOPT_BENCH_CASES_HPP

#include "imgproc_spec.hpp"

#include <array>
#include <string_view>

namespace bench {

enum class Category { direct, paraphrase, multiple, negation, distractor, implicit };

[[nodiscard]] constexpr std::string_view name(Category category) noexcept {
    switch (category) {
    case Category::direct:
        return "direct";
    case Category::paraphrase:
        return "paraphrase";
    case Category::multiple:
        return "multiple";
    case Category::negation:
        return "negation";
    case Category::distractor:
        return "distractor";
    case Category::implicit:
        return "implicit";
    }
    return "unknown";
}

struct Case {
    std::string_view utterance;
    Category category;
    Options expected;
};

// Expected values are written as the full resolved struct: anything not named
// keeps its default, which is exactly what the parser should leave alone.
inline const std::array<Case, 36> cases{{
    // Direct — the option name or its value appears almost literally.
    {"set quality to 55", Category::direct, {.quality = 55}},
    {"use 12 jobs", Category::direct, {.jobs = 12}},
    {"format webp", Category::direct, {.format = Format::webp}},
    {"format avif", Category::direct, {.format = Format::avif}},
    {"output ./out", Category::direct, {.output = "./out"}},
    {"enable recursive", Category::direct, {.recursive = true}},
    {"turn on dry-run", Category::direct, {.dry_run = true}},
    {"strip metadata", Category::direct, {.strip_metadata = true}},
    {"overwrite existing files", Category::direct, {.overwrite = true}},
    {"max width 1920", Category::direct, {.max_width = 1920}},

    // Paraphrase — same intent, different vocabulary.
    {"compress harder, quality 30", Category::paraphrase, {.quality = 30}},
    {"write the results into ./build/images", Category::paraphrase, {.output = "./build/images"}},
    {"use sixteen parallel workers", Category::paraphrase, {.jobs = 16}},
    {"encode them as avif please", Category::paraphrase, {.format = Format::avif}},
    {"walk into every subfolder too", Category::paraphrase, {.recursive = true}},
    {"just show me what would happen", Category::paraphrase, {.dry_run = true}},
    {"remove the exif data", Category::paraphrase, {.strip_metadata = true}},
    {"shrink anything wider than 800 pixels", Category::paraphrase, {.max_width = 800}},

    // Multiple — several options in one utterance.
    {"convert ./photos to webp at quality 80", Category::multiple,
     {.output = "./photos", .format = Format::webp, .quality = 80}},
    {"recursively write to ./out as avif", Category::multiple,
     {.recursive = true, .output = "./out", .format = Format::avif}},
    {"dry run with 4 jobs and quality 90", Category::multiple, {.dry_run = true, .quality = 90, .jobs = 4}},
    {"strip metadata, overwrite, output ./dist", Category::multiple,
     {.strip_metadata = true, .overwrite = true, .output = "./dist"}},
    {"recurse into ./assets, webp, max width 1200, 8 workers", Category::multiple,
     {.recursive = true, .output = "./assets", .format = Format::webp, .jobs = 8, .max_width = 1200}},
    {"quality 100 avif into ./hq overwriting whatever is there", Category::multiple,
     {.overwrite = true, .output = "./hq", .format = Format::avif, .quality = 100}},

    // Negation — the utterance rules an option out.
    {"do not recurse into subdirectories", Category::negation, {}},
    {"keep the metadata", Category::negation, {}},
    {"actually write the files, this is not a rehearsal", Category::negation, {}},
    {"never overwrite anything", Category::negation, {}},
    {"convert to webp but do not recurse", Category::negation, {.format = Format::webp}},

    // Distractor — numbers and paths that must not become option values.
    {"there are 40 photos from 2019 in there, use quality 60", Category::distractor, {.quality = 60}},
    {"my 3 year old laptop is slow, use 2 jobs", Category::distractor, {.jobs = 2}},
    {"the readme in ./docs explains it, write output to ./out", Category::distractor, {.output = "./out"}},
    {"i tried 5 other tools already, just use avif", Category::distractor, {.format = Format::avif}},

    // Implicit — the mapping requires a small inference step.
    {"prepare these for the web", Category::implicit, {.format = Format::webp}},
    {"make them as small as possible", Category::implicit, {.quality = 1}},
    {"use every core i have, say 16", Category::implicit, {.jobs = 16}},
}};

} // namespace bench

#endif
