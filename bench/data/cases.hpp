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

// The three language blocks are parallel translations of one another: case n
// carries the same meaning, the same category, and the same expected options in
// every language. A difference in score is then a difference in language rather
// than a difference in what was asked.
//
// Paths and enumeration labels stay in ASCII throughout, as they would on a
// real command line; only the surrounding request changes language.
//
// Expected values are written as the full resolved struct: anything not named
// keeps its default, which is exactly what the parser should leave alone.
inline const std::array<Case, 108> cases{{
    // ---------------------------------------------------------------- English
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

    // ---------------------------------------------------------------- Chinese
    {"把质量设成 55", Category::direct, {.quality = 55}, Language::chinese},
    {"用 12 个任务", Category::direct, {.jobs = 12}, Language::chinese},
    {"格式用 webp", Category::direct, {.format = Format::webp}, Language::chinese},
    {"格式用 avif", Category::direct, {.format = Format::avif}, Language::chinese},
    {"输出到 ./out", Category::direct, {.output = "./out"}, Language::chinese},
    {"开启递归", Category::direct, {.recursive = true}, Language::chinese},
    {"打开试运行", Category::direct, {.dry_run = true}, Language::chinese},
    {"去掉元数据", Category::direct, {.strip_metadata = true}, Language::chinese},
    {"覆盖已有的文件", Category::direct, {.overwrite = true}, Language::chinese},
    {"最大宽度 1920", Category::direct, {.max_width = 1920}, Language::chinese},

    {"压缩狠一点，质量 30", Category::paraphrase, {.quality = 30}, Language::chinese},
    {"把结果写到 ./build/images 里", Category::paraphrase, {.output = "./build/images"}, Language::chinese},
    {"用十六个并行工作线程", Category::paraphrase, {.jobs = 16}, Language::chinese},
    {"请把它们编码成 avif", Category::paraphrase, {.format = Format::avif}, Language::chinese},
    {"每个子文件夹也要进去", Category::paraphrase, {.recursive = true}, Language::chinese},
    {"先让我看看会发生什么就好", Category::paraphrase, {.dry_run = true}, Language::chinese},
    {"把 exif 数据删掉", Category::paraphrase, {.strip_metadata = true}, Language::chinese},
    {"宽度超过 800 像素的都缩小", Category::paraphrase, {.max_width = 800}, Language::chinese},

    {"把 ./photos 转成 webp，质量 80", Category::multiple,
     {.output = "./photos", .format = Format::webp, .quality = 80}, Language::chinese},
    {"递归地以 avif 写到 ./out", Category::multiple,
     {.recursive = true, .output = "./out", .format = Format::avif}, Language::chinese},
    {"试运行，4 个任务，质量 90", Category::multiple,
     {.dry_run = true, .quality = 90, .jobs = 4}, Language::chinese},
    {"去掉元数据，覆盖，输出到 ./dist", Category::multiple,
     {.strip_metadata = true, .overwrite = true, .output = "./dist"}, Language::chinese},
    {"递归进 ./assets，webp，最大宽度 1200，8 个工作线程", Category::multiple,
     {.recursive = true, .output = "./assets", .format = Format::webp, .jobs = 8, .max_width = 1200},
     Language::chinese},
    {"质量 100 的 avif 放到 ./hq，里面有什么都覆盖掉", Category::multiple,
     {.overwrite = true, .output = "./hq", .format = Format::avif, .quality = 100}, Language::chinese},

    {"不要递归到子目录", Category::negation, {}, Language::chinese},
    {"保留元数据", Category::negation, {}, Language::chinese},
    {"真的把文件写出来，这不是演习", Category::negation, {}, Language::chinese},
    {"绝对不要覆盖任何东西", Category::negation, {}, Language::chinese},
    {"转成 webp，但是不要递归", Category::negation, {.format = Format::webp}, Language::chinese},

    {"那里有 40 张 2019 年的照片，质量用 60", Category::distractor, {.quality = 60}, Language::chinese},
    {"我这台 3 年的笔记本很慢，用 2 个任务", Category::distractor, {.jobs = 2}, Language::chinese},
    {"./docs 里的说明文件有讲，输出写到 ./out", Category::distractor,
     {.output = "./out"}, Language::chinese},
    {"我已经试过 5 个别的工具了，就用 avif 吧", Category::distractor,
     {.format = Format::avif}, Language::chinese},

    {"把这些准备成适合网页用的", Category::implicit, {.format = Format::webp}, Language::chinese},
    {"让它们尽可能小", Category::implicit, {.quality = 1}, Language::chinese},
    {"把我所有的核心都用上，就 16 个吧", Category::implicit, {.jobs = 16}, Language::chinese},

    // --------------------------------------------------------------- Japanese
    {"品質を55にして", Category::direct, {.quality = 55}, Language::japanese},
    {"ジョブを12個使って", Category::direct, {.jobs = 12}, Language::japanese},
    {"フォーマットはwebp", Category::direct, {.format = Format::webp}, Language::japanese},
    {"フォーマットはavif", Category::direct, {.format = Format::avif}, Language::japanese},
    {"出力先は ./out", Category::direct, {.output = "./out"}, Language::japanese},
    {"再帰を有効にして", Category::direct, {.recursive = true}, Language::japanese},
    {"ドライランをオンにして", Category::direct, {.dry_run = true}, Language::japanese},
    {"メタデータを削除して", Category::direct, {.strip_metadata = true}, Language::japanese},
    {"既存のファイルを上書きして", Category::direct, {.overwrite = true}, Language::japanese},
    {"最大幅は1920", Category::direct, {.max_width = 1920}, Language::japanese},

    {"もっと圧縮して、品質は30", Category::paraphrase, {.quality = 30}, Language::japanese},
    {"結果は ./build/images に書き出して", Category::paraphrase,
     {.output = "./build/images"}, Language::japanese},
    {"並列ワーカーを十六個使って", Category::paraphrase, {.jobs = 16}, Language::japanese},
    {"avifでエンコードしてください", Category::paraphrase, {.format = Format::avif}, Language::japanese},
    {"サブフォルダにも全部入って", Category::paraphrase, {.recursive = true}, Language::japanese},
    {"何が起きるかだけ見せて", Category::paraphrase, {.dry_run = true}, Language::japanese},
    {"exifデータを消して", Category::paraphrase, {.strip_metadata = true}, Language::japanese},
    {"幅が800ピクセルを超えるものは縮小して", Category::paraphrase,
     {.max_width = 800}, Language::japanese},

    {"./photos を webp に品質80で変換して", Category::multiple,
     {.output = "./photos", .format = Format::webp, .quality = 80}, Language::japanese},
    {"再帰的に ./out へ avif で書き出して", Category::multiple,
     {.recursive = true, .output = "./out", .format = Format::avif}, Language::japanese},
    {"ドライランで、ジョブ4個、品質90", Category::multiple,
     {.dry_run = true, .quality = 90, .jobs = 4}, Language::japanese},
    {"メタデータを削除、上書き、出力は ./dist", Category::multiple,
     {.strip_metadata = true, .overwrite = true, .output = "./dist"}, Language::japanese},
    {"./assets に再帰的に入って、webp、最大幅1200、ワーカー8個", Category::multiple,
     {.recursive = true, .output = "./assets", .format = Format::webp, .jobs = 8, .max_width = 1200},
     Language::japanese},
    {"品質100のavifを ./hq に、中身が何であっても上書きして", Category::multiple,
     {.overwrite = true, .output = "./hq", .format = Format::avif, .quality = 100}, Language::japanese},

    {"サブディレクトリには再帰しないで", Category::negation, {}, Language::japanese},
    {"メタデータは残して", Category::negation, {}, Language::japanese},
    {"本当にファイルを書き出して、リハーサルではありません", Category::negation, {}, Language::japanese},
    {"絶対に何も上書きしないで", Category::negation, {}, Language::japanese},
    {"webpに変換して、でも再帰はしないで", Category::negation,
     {.format = Format::webp}, Language::japanese},

    {"そこには2019年の写真が40枚あります。品質は60で", Category::distractor,
     {.quality = 60}, Language::japanese},
    {"私の3年前のノートパソコンは遅いので、ジョブは2個で", Category::distractor,
     {.jobs = 2}, Language::japanese},
    {"./docs のREADMEに説明があります。出力は ./out に書いて", Category::distractor,
     {.output = "./out"}, Language::japanese},
    {"すでに他のツールを5つ試しました。avifでお願いします", Category::distractor,
     {.format = Format::avif}, Language::japanese},

    {"これらをウェブ用に準備して", Category::implicit, {.format = Format::webp}, Language::japanese},
    {"できるだけ小さくして", Category::implicit, {.quality = 1}, Language::japanese},
    {"使えるコアを全部使って、16個で", Category::implicit, {.jobs = 16}, Language::japanese},
}};

} // namespace bench

#endif
