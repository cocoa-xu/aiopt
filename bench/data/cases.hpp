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

enum class Language { english, chinese, japanese };

[[nodiscard]] constexpr std::string_view name(Language language) noexcept {
    switch (language) {
    case Language::english:
        return "english";
    case Language::chinese:
        return "chinese";
    case Language::japanese:
        return "japanese";
    }
    return "unknown";
}

// Language trails the other members so the English cases, which were written
// first, need no edit to carry the default.
struct Case {
    std::string_view utterance;
    Category category;
    Options expected;
    Language language = Language::english;
};

// Expected values are written as the full resolved struct: anything not named
// keeps its default, which is exactly what the parser should leave alone.
inline const std::array<Case, 62> cases{{
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

    // Chinese. Paths and enumeration labels stay in ASCII, as they would on a
    // real command line; only the surrounding request changes language.
    {"把质量设成 55", Category::direct, {.quality = 55}, Language::chinese},
    {"用 12 个并行任务", Category::direct, {.jobs = 12}, Language::chinese},
    {"格式用 webp", Category::direct, {.format = Format::webp}, Language::chinese},
    {"输出到 ./out", Category::direct, {.output = "./out"}, Language::chinese},
    {"递归处理子目录", Category::direct, {.recursive = true}, Language::chinese},
    {"覆盖已经存在的文件", Category::direct, {.overwrite = true}, Language::chinese},
    {"压缩狠一点，质量 30", Category::paraphrase, {.quality = 30}, Language::chinese},
    {"把结果写到 ./build/images 里面", Category::paraphrase, {.output = "./build/images"}, Language::chinese},
    {"去掉 exif 元数据", Category::paraphrase, {.strip_metadata = true}, Language::chinese},
    {"只是试运行，先别真的写文件", Category::paraphrase, {.dry_run = true}, Language::chinese},
    {"把 ./photos 里的图片转成 webp，质量 80", Category::multiple,
     {.output = "./photos", .format = Format::webp, .quality = 80}, Language::chinese},
    {"不要递归到子目录", Category::negation, {}, Language::chinese},
    {"这里有 40 张 2019 年拍的照片，质量用 60", Category::distractor, {.quality = 60}, Language::chinese},

    // Japanese.
    {"品質を55にして", Category::direct, {.quality = 55}, Language::japanese},
    {"ジョブを12個使って", Category::direct, {.jobs = 12}, Language::japanese},
    {"フォーマットはwebpで", Category::direct, {.format = Format::webp}, Language::japanese},
    {"./out に出力して", Category::direct, {.output = "./out"}, Language::japanese},
    {"サブディレクトリも再帰的に処理して", Category::direct, {.recursive = true}, Language::japanese},
    {"既存のファイルを上書きして", Category::direct, {.overwrite = true}, Language::japanese},
    {"もっと圧縮して、品質は30", Category::paraphrase, {.quality = 30}, Language::japanese},
    {"結果は ./build/images に書き出して", Category::paraphrase,
     {.output = "./build/images"}, Language::japanese},
    {"EXIFメタデータを削除して", Category::paraphrase, {.strip_metadata = true}, Language::japanese},
    {"実際には書き込まないで、動作だけ見せて", Category::paraphrase, {.dry_run = true}, Language::japanese},
    {"./photos の画像を webp に、品質80で変換して", Category::multiple,
     {.output = "./photos", .format = Format::webp, .quality = 80}, Language::japanese},
    {"サブディレクトリには入らないで", Category::negation, {}, Language::japanese},
    {"2019年に撮った写真が40枚あります。品質は60で", Category::distractor,
     {.quality = 60}, Language::japanese},
}};

} // namespace bench

#endif
