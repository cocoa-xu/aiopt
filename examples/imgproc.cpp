// A complete aiopt program that does real work: declare the options once, then
// accept the command line as ordinary prose.
//
//   ./imgproc model.gguf "convert ./photos into ./out as jpg at quality 80"
//   ./imgproc model.gguf "recursively shrink ./assets into ./web, max width 1024"

#include <aiopt/aiopt.hpp>

#include "third_party/stb_image.h"
#include "third_party/stb_image_resize2.h"
#include "third_party/stb_image_write.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <cctype>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

enum class Format { png, jpg, bmp };

struct Options {
    bool recursive = false;
    bool dry_run = false;
    bool overwrite = false;
    std::string input;
    std::string output;
    Format format = Format::png;
    int quality = 85;
    int max_width = 0;
    int jobs = 1;
};

constexpr auto specification = aiopt::spec<Options>(
    aiopt::flag(&Options::recursive, "recursive", "descend into subdirectories when collecting inputs"),
    aiopt::flag(&Options::dry_run, "dry-run", "report what would happen without writing any file"),
    aiopt::flag(&Options::overwrite, "overwrite", "replace files that already exist in the output directory"),
    // Two paths in one request are easy to confuse, so each description says
    // which side of the operation it is. Descriptions are the prompt here.
    aiopt::path(&Options::input, "input", "source directory the images are read from"),
    aiopt::path(&Options::output, "output", "destination directory the results are written to"),
    aiopt::choice(&Options::format, "format", "output encoding", "png", "jpg", "bmp"),
    aiopt::number(&Options::quality, "quality", "JPEG compression quality, higher means larger files", 1, 100),
    aiopt::number(&Options::max_width, "max-width", "downscale images wider than this many pixels", 0, 16384),
    aiopt::number(&Options::jobs, "jobs", "number of images to process at once", 1, 64));

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

[[nodiscard]] std::vector<std::filesystem::path> collect(const std::filesystem::path& root, bool recursive) {
    namespace fs = std::filesystem;
    std::vector<fs::path> files;
    std::error_code failure;

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

    if (options.max_width > 0 && width > options.max_width) {
        const int scaled_width = options.max_width;
        const int scaled_height = std::max(1, height * scaled_width / width);
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
    const std::lock_guard<std::mutex> held{console};
    std::cout << "  wrote " << destination.string() << "  " << pixels.width() << "x" << pixels.height() << '\n';
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0] << " <model.gguf> <what you want done>\n";
        return 2;
    }

    aiopt::EngineOptions engine;
    engine.threads = 10;

    auto created = aiopt::make_parser(specification, argv[1], engine);
    if (!created) {
        std::cerr << "imgproc: " << created.error().detail() << '\n';
        return 1;
    }
    auto parser = std::move(created).value();

    auto outcome = parser.parse(argv[2]);
    if (!outcome) {
        std::cerr << "imgproc: " << outcome.error().detail() << '\n';
        return 1;
    }

    const Options& options = outcome->options;
    std::cout << "understood:\n"
              << "  input       " << (options.input.empty() ? "." : options.input) << '\n'
              << "  output      " << (options.output.empty() ? "(unset)" : options.output) << '\n'
              << "  format      " << extension(options.format).substr(1) << '\n'
              << "  quality     " << options.quality << '\n'
              << "  max-width   " << (options.max_width == 0 ? "unchanged" : std::to_string(options.max_width))
              << '\n'
              << "  recursive   " << std::boolalpha << options.recursive << '\n'
              << "  overwrite   " << options.overwrite << '\n'
              << "  dry-run     " << options.dry_run << '\n'
              << "  jobs        " << options.jobs << "\n\n";

    if (options.output.empty()) {
        std::cerr << "imgproc: no output directory was named, so there is nothing to write\n";
        return 1;
    }

    namespace fs = std::filesystem;
    const fs::path source_root = options.input.empty() ? fs::path{"."} : fs::path{options.input};
    const fs::path destination_root{options.output};

    if (!fs::is_directory(source_root)) {
        std::cerr << "imgproc: " << source_root.string() << " is not a directory\n";
        return 1;
    }

    const std::vector<fs::path> files = collect(source_root, options.recursive);
    if (files.empty()) {
        std::cout << "no images found under " << source_root.string() << '\n';
        return 0;
    }

    std::vector<std::pair<fs::path, fs::path>> work;
    int skipped = 0;
    for (const fs::path& file : files) {
        const fs::path relative = fs::relative(file, source_root);
        fs::path destination = destination_root / relative;
        destination.replace_extension(extension(options.format));
        if (!options.overwrite && fs::exists(destination)) {
            ++skipped;
            continue;
        }
        work.emplace_back(file, destination);
    }

    std::cout << files.size() << " image(s) found, " << work.size() << " to process";
    if (skipped > 0) {
        std::cout << ", " << skipped << " already present (pass overwrite to replace)";
    }
    std::cout << "\n\n";

    if (options.dry_run) {
        for (const auto& [source, destination] : work) {
            std::cout << "  would write " << destination.string() << '\n';
        }
        std::cout << "\ndry run, nothing was written\n";
        return 0;
    }

    std::error_code failure;
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

    std::cout << "\nwrote " << report.written << ", failed " << report.failed << '\n';
    return report.failed > 0 ? 1 : 0;
}
