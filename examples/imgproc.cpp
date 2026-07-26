// A complete aiopt program that does real work: declare the options once, then
// accept the command line as ordinary prose.
//
//   ./imgproc "convert images from ./photos to ./out as jpg at quality 80"
//   ./imgproc "recursively shrink ./assets into ./web, max width 1024"
//
// The model is chosen when the program is built, not when it is run, so a user
// never has to know one is involved. See AIOPT_EXAMPLE_MODEL in the CMake file.

#include <aiopt/aiopt.hpp>

#include "third_party/stb_image.h"
#include "third_party/stb_image_resize2.h"
#include "third_party/stb_image_write.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <random>
#include <span>
#include <cctype>
#include <cmath>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

enum class Format { png, jpg, bmp };

// A resize is not a number, a string, or a choice, so it is declared with its
// own syntax, its own grammar rule, and its own reader. aiopt carries all three
// through without knowing what any of it means.
struct Resize {
    enum class Mode : std::uint8_t { unchanged, scale, exact };

    Mode mode = Mode::unchanged;
    double factor = 1.0;
    int width = 0;
    int height = 0;
};

constexpr std::string_view resize_syntax = R"(a string like "50%", "0.5", or "1920x1080")";

// Written as a GBNF value rule, quotes included, because the value travels as a
// JSON string: 50% and 1920x1080 are not JSON numbers.
constexpr std::string_view resize_grammar =
    R"(  "\"" ( [0-9]+ "%" | [0-9]* "." [0-9]+ | [0-9]+ "x" [0-9]+ ) "\"")";

[[nodiscard]] bool read_number(std::string_view text, long& out) noexcept {
    if (text.empty()) {
        return false;
    }
    out = 0;
    for (const char digit : text) {
        if (digit < '0' || digit > '9') {
            return false;
        }
        out = out * 10 + (digit - '0');
    }
    return true;
}

[[nodiscard]] bool read_resize(std::string_view text, Resize& out) {
    while (!text.empty() && (text.front() == ' ' || text.back() == ' ')) {
        text.front() == ' ' ? text.remove_prefix(1) : text.remove_suffix(1);
    }
    if (text.empty()) {
        return false;
    }

    if (const std::size_t cross = text.find_first_of("xX"); cross != std::string_view::npos) {
        long width = 0;
        long height = 0;
        if (!read_number(text.substr(0, cross), width) || !read_number(text.substr(cross + 1), height)) {
            return false;
        }
        if (width <= 0 || height <= 0 || width > 16384 || height > 16384) {
            return false;
        }
        out = Resize{Resize::Mode::exact, 1.0, static_cast<int>(width), static_cast<int>(height)};
        return true;
    }

    double factor = 0.0;
    if (text.back() == '%') {
        long percent = 0;
        if (!read_number(text.substr(0, text.size() - 1), percent)) {
            return false;
        }
        factor = static_cast<double>(percent) / 100.0;
    } else {
        const std::size_t dot = text.find('.');
        if (dot == std::string_view::npos) {
            return false;
        }
        long whole = 0;
        long fraction = 0;
        const std::string_view digits = text.substr(dot + 1);
        if (!(dot == 0 || read_number(text.substr(0, dot), whole)) || !read_number(digits, fraction)) {
            return false;
        }
        double scale = 1.0;
        for (std::size_t i = 0; i < digits.size(); ++i) {
            scale *= 10.0;
        }
        factor = static_cast<double>(whole) + static_cast<double>(fraction) / scale;
    }

    if (factor <= 0.0 || factor > 64.0) {
        return false;
    }
    out = Resize{Resize::Mode::scale, factor, 0, 0};
    return true;
}

struct Options {
    bool help = false;
    bool recursive = false;
    bool dry_run = false;
    bool overwrite = true;
    bool verbose = true;
    std::string input;
    std::string output;
    Format format = Format::png;
    Resize resize;
    int quality = 85;
    int max_width = 0;
    int jobs = 1;
    int examples = 3;
};

constexpr auto specification = aiopt::spec<Options>(
    aiopt::flag(&Options::help, "help",
                "the request asks what this program can do, how to use it, or for examples of using "
                "it, rather than asking for work to be done"),
    aiopt::flag(&Options::recursive, "recursive", "descend into subdirectories when collecting inputs"),
    aiopt::flag(&Options::dry_run, "dry-run", "report what would happen without writing any file"),
    aiopt::flag(&Options::overwrite, "overwrite",
                "replace files that already exist; false leaves them and skips that image"),
    aiopt::flag(&Options::verbose, "verbose",
                "true reports each image as usual; false is quiet, silent, prints nothing at all, "
                "and produces no output"),
    // Two paths in one request are easy to confuse, so each description says
    // which side of the operation it is. Descriptions are the prompt here.
    aiopt::path(&Options::input, "input", "source image, or a directory of them, to read"),
    aiopt::path(&Options::output, "output", "destination file or directory to write the results to"),
    aiopt::choice(&Options::format, "format", "output encoding", "png", "jpg", "bmp"),
    aiopt::number(&Options::quality, "quality", "JPEG compression quality, higher means larger files", 1, 100),
    aiopt::custom(&Options::resize, "resize", "scale every image by a factor or to exact pixels",
                  resize_syntax, resize_grammar, read_resize),
    aiopt::number(&Options::max_width, "max-width",
                  "shrink only images wider than this, keeping their shape; ignored when resize is set", 0,
                  16384),
    aiopt::number(&Options::jobs, "jobs", "number of images to process at once", 1, 64),
    // How many examples to show is part of asking for help, so it is declared
    // like anything else rather than fixed in the code.
    aiopt::number(&Options::examples, "examples",
                  "how many usage examples to show; a request asking for examples is asking for "
                  "help, so set help to true whenever this is set",
                  1, 10));

[[nodiscard]] std::string describe(const Resize& resize) {
    switch (resize.mode) {
    case Resize::Mode::scale:
        return std::to_string(std::lround(resize.factor * 100.0)) + "%";
    case Resize::Mode::exact:
        return std::to_string(resize.width) + "x" + std::to_string(resize.height);
    case Resize::Mode::unchanged:
        break;
    }
    return "unchanged";
}

constexpr std::string_view extension(Format format) noexcept {
    switch (format) {
    case Format::png:
        return ".png";
    case Format::jpg:
        return ".jpg";
    case Format::bmp:
        return ".bmp";
    }
    return ".png";
}

[[nodiscard]] bool loadable(const std::filesystem::path& file) {
    static const std::vector<std::string> known{".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif", ".psd"};
    std::string suffix = file.extension().string();
    std::transform(suffix.begin(), suffix.end(), suffix.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return std::find(known.begin(), known.end(), suffix) != known.end();
}

// A request may name one image or a directory of them. Both are ordinary things
// to ask for, so both are accepted.
[[nodiscard]] std::vector<std::filesystem::path> collect(const std::filesystem::path& root, bool recursive) {
    namespace fs = std::filesystem;
    std::vector<fs::path> files;
    std::error_code failure;

    if (fs::is_regular_file(root, failure)) {
        if (loadable(root)) {
            files.push_back(root);
        }
        return files;
    }

    const auto consider = [&files](const fs::directory_entry& entry) {
        if (entry.is_regular_file() && loadable(entry.path())) {
            files.push_back(entry.path());
        }
    };

    if (recursive) {
        for (fs::recursive_directory_iterator it{root, failure}, end; it != end; it.increment(failure)) {
            consider(*it);
        }
    } else {
        for (fs::directory_iterator it{root, failure}, end; it != end; it.increment(failure)) {
            consider(*it);
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

struct Report {
    std::atomic<int> written{0};
    std::atomic<int> skipped{0};
    std::atomic<int> failed{0};
};

// Owns the pixel buffer stb hands back, which must be released with stbi_image_free.
class Pixels {
public:
    Pixels(unsigned char* data, int width, int height) noexcept : data_{data}, width_{width}, height_{height} {}
    Pixels(const Pixels&) = delete;
    Pixels& operator=(const Pixels&) = delete;
    Pixels(Pixels&& other) noexcept
        : data_{std::exchange(other.data_, nullptr)}, width_{other.width_}, height_{other.height_} {}
    Pixels& operator=(Pixels&& other) noexcept {
        if (this != &other) {
            stbi_image_free(data_);
            data_ = std::exchange(other.data_, nullptr);
            width_ = other.width_;
            height_ = other.height_;
        }
        return *this;
    }
    ~Pixels() { stbi_image_free(data_); }

    [[nodiscard]] unsigned char* data() const noexcept { return data_; }
    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] explicit operator bool() const noexcept { return data_ != nullptr; }

private:
    unsigned char* data_;
    int width_;
    int height_;
};

constexpr int channels = 4;

[[nodiscard]] bool encode(const std::filesystem::path& destination, Format format, const Pixels& pixels,
                          int quality) {
    const std::string name = destination.string();
    switch (format) {
    case Format::png:
        return stbi_write_png(name.c_str(), pixels.width(), pixels.height(), channels, pixels.data(),
                              pixels.width() * channels) != 0;
    case Format::jpg:
        return stbi_write_jpg(name.c_str(), pixels.width(), pixels.height(), channels, pixels.data(),
                              quality) != 0;
    case Format::bmp:
        return stbi_write_bmp(name.c_str(), pixels.width(), pixels.height(), channels, pixels.data()) != 0;
    }
    return false;
}

void process(const std::filesystem::path& source, const std::filesystem::path& destination,
             const Options& options, Report& report, std::mutex& console) {
    int width = 0;
    int height = 0;
    int original = 0;
    Pixels pixels{stbi_load(source.string().c_str(), &width, &height, &original, channels), width, height};
    if (!pixels) {
        ++report.failed;
        return;
    }

    // An explicit resize wins over max-width, which is only a ceiling.
    int scaled_width = width;
    int scaled_height = height;
    switch (options.resize.mode) {
    case Resize::Mode::exact:
        scaled_width = options.resize.width;
        scaled_height = options.resize.height;
        break;
    case Resize::Mode::scale:
        scaled_width = std::max(1, static_cast<int>(std::lround(width * options.resize.factor)));
        scaled_height = std::max(1, static_cast<int>(std::lround(height * options.resize.factor)));
        break;
    case Resize::Mode::unchanged:
        if (options.max_width > 0 && width > options.max_width) {
            scaled_width = options.max_width;
            scaled_height = std::max(1, height * scaled_width / width);
        }
        break;
    }

    if (scaled_width != width || scaled_height != height) {
        Pixels scaled{stbir_resize_uint8_srgb(pixels.data(), width, height, 0, nullptr, scaled_width,
                                              scaled_height, 0, STBIR_RGBA),
                      scaled_width, scaled_height};
        if (!scaled) {
            ++report.failed;
            return;
        }
        pixels = std::move(scaled);
    }

    if (!encode(destination, options.format, pixels, options.quality)) {
        ++report.failed;
        return;
    }

    ++report.written;
    if (!options.verbose) {
        return;
    }
    const std::lock_guard<std::mutex> held{console};
    std::cout << "  wrote " << destination.string() << "  " << pixels.width() << "x" << pixels.height() << '\n';
}

} // namespace

#ifndef AIOPT_MODEL_PATH
#error "AIOPT_MODEL_PATH must be defined by the build; see examples/CMakeLists.txt"
#endif

constexpr std::string_view summary = "convert and resize images, described in plain language";

// Used when no model has been loaded, which is the case for a bare invocation.
const std::array<std::string_view, 3> fallback_examples{
    "convert images from ./photos to ./out as jpg at quality 80",
    "recursively shrink ./assets to ./web, nothing wider than 1024",
    "show me what would happen, do not write anything yet",
};

void print_help(std::span<const std::string_view> examples) {
    std::cout << aiopt::render_help(specification.descriptors(), "imgproc", summary, examples);
}

int main(int argc, char** argv) {
    // Asking for nothing is unambiguous, so answer it without loading a model.
    if (argc < 2) {
        print_help(fallback_examples);
        return 0;
    }

    aiopt::EngineOptions engine;
    engine.threads = 10;

    auto created = aiopt::make_parser(specification, AIOPT_MODEL_PATH, engine);
    if (!created) {
        std::cerr << "imgproc: " << created.error().detail() << '\n';
        return 1;
    }
    auto parser = std::move(created).value();

    auto outcome = parser.parse(argv[1]);
    if (!outcome) {
        std::cerr << "imgproc: " << outcome.error().detail() << '\n';
        return 1;
    }

    const Options& options = outcome->options;
    if (options.help) {
        // The model is already loaded and already knows the options, so let it
        // write the examples rather than shipping a list that goes stale. A
        // fresh seed each run makes it visible that these are written rather
        // than recited; parsing keeps its deterministic sampler.
        // The request goes back in so the examples come out in whatever language
        // it was written in.
        std::random_device entropy;
        auto suggested = parser.suggest(aiopt::Suggestions{static_cast<std::size_t>(options.examples),
                                                           argv[1],
                                                           {0.75f, 0.92f, entropy()},
                                                           256});
        if (suggested && !suggested->empty()) {
            std::vector<std::string_view> examples{suggested->begin(), suggested->end()};
            print_help(examples);
        } else {
            print_help(fallback_examples);
        }
        return 0;
    }

    if (options.verbose) {
    std::cout << "understood:\n"
              << "  input       " << (options.input.empty() ? "." : options.input) << '\n'
              << "  output      " << (options.output.empty() ? "(unset)" : options.output) << '\n'
              << "  format      " << extension(options.format).substr(1) << '\n'
              << "  resize      " << describe(options.resize) << '\n'
              << "  quality     " << options.quality << '\n'
              << "  max-width   " << (options.max_width == 0 ? "unchanged" : std::to_string(options.max_width))
              << '\n'
              << "  recursive   " << std::boolalpha << options.recursive << '\n'
              << "  overwrite   " << options.overwrite << '\n'
              << "  dry-run     " << options.dry_run << '\n'
              << "  jobs        " << options.jobs << "\n\n";
    }

    if (options.output.empty()) {
        std::cerr << "imgproc: no output location was named, so there is nothing to write\n";
        return 1;
    }

    namespace fs = std::filesystem;
    const fs::path source_root = options.input.empty() ? fs::path{"."} : fs::path{options.input};
    const fs::path destination_root{options.output};

    std::error_code failure;
    const bool one_image = fs::is_regular_file(source_root, failure);
    if (!one_image && !fs::is_directory(source_root, failure)) {
        std::cerr << "imgproc: there is no file or directory at " << source_root.string() << '\n';
        return 1;
    }

    const std::vector<fs::path> files = collect(source_root, options.recursive);
    if (files.empty()) {
        if (options.verbose) {
            std::cout << (one_image ? "not an image this program can read: " : "no images found under ")
                      << source_root.string() << '\n';
        }
        return one_image ? 1 : 0;
    }

    // An output carrying an extension names a file rather than a directory,
    // which only means anything when a single image is being converted.
    const bool destination_is_file = one_image && !destination_root.extension().empty();

    std::vector<std::pair<fs::path, fs::path>> work;
    int skipped = 0;
    for (const fs::path& file : files) {
        fs::path destination;
        if (destination_is_file) {
            destination = destination_root;
        } else {
            destination = destination_root / (one_image ? file.filename() : fs::relative(file, source_root));
            destination.replace_extension(extension(options.format));
        }
        if (!options.overwrite && fs::exists(destination)) {
            ++skipped;
            continue;
        }
        work.emplace_back(file, destination);
    }

    if (options.verbose) {
        std::cout << files.size() << " image(s) found, " << work.size() << " to process";
        if (skipped > 0) {
            std::cout << ", " << skipped << " left alone because they already exist";
        }
        std::cout << "\n\n";
    }

    if (options.dry_run) {
        if (options.verbose) {
            for (const auto& [source, destination] : work) {
                std::cout << "  would write " << destination.string() << '\n';
            }
            std::cout << "\ndry run, nothing was written\n";
        }
        return 0;
    }

    for (const auto& [source, destination] : work) {
        fs::create_directories(destination.parent_path(), failure);
    }

    Report report;
    std::mutex console;
    std::atomic<std::size_t> next{0};
    const auto worker = [&] {
        for (std::size_t i = next++; i < work.size(); i = next++) {
            process(work[i].first, work[i].second, options, report, console);
        }
    };

    const auto count = static_cast<std::size_t>(std::max(1, options.jobs));
    std::vector<std::thread> pool;
    pool.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        pool.emplace_back(worker);
    }
    for (std::thread& thread : pool) {
        thread.join();
    }

    if (options.verbose) {
        std::cout << "\nwrote " << report.written << ", failed " << report.failed << '\n';
    }
    return report.failed > 0 ? 1 : 0;
}
