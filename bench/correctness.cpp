// Measures how often a local model turns prose into the option assignments a
// specification actually asked for.
//
//   ./correctness model.gguf [--threads N] [--show-failures]

#include "data/cases.hpp"
#include "data/imgproc_spec.hpp"

#include <aiopt/aiopt.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Tally {
    int true_positive = 0;
    int predicted = 0;
    int gold = 0;

    [[nodiscard]] double precision() const noexcept {
        return predicted == 0 ? 1.0 : static_cast<double>(true_positive) / predicted;
    }
    [[nodiscard]] double recall() const noexcept {
        return gold == 0 ? 1.0 : static_cast<double>(true_positive) / gold;
    }
    [[nodiscard]] double f1() const noexcept {
        const double p = precision();
        const double r = recall();
        return (p + r) == 0.0 ? 0.0 : 2.0 * p * r / (p + r);
    }
};

struct CategoryScore {
    int exact = 0;
    int total = 0;
};

[[nodiscard]] double percentile(std::vector<double> samples, double fraction) {
    if (samples.empty()) {
        return 0.0;
    }
    std::sort(samples.begin(), samples.end());
    const auto index = static_cast<std::size_t>(fraction * static_cast<double>(samples.size() - 1));
    return samples[index];
}

[[nodiscard]] std::string escape(std::string_view text) {
    std::string out;
    for (const char character : text) {
        if (character == '\n') {
            out += "\\n";
        } else if (character == '\r') {
            out += "\\r";
        } else {
            out += character;
        }
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <model.gguf> [--threads N] [--show-failures]\n";
        return 2;
    }

    std::int32_t threads = 10;
    bool show_failures = false;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            threads = static_cast<std::int32_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--show-failures") == 0) {
            show_failures = true;
        }
    }

    aiopt::EngineOptions engine;
    engine.threads = threads;

    auto created = aiopt::make_parser(bench::specification, argv[1], engine);
    if (!created) {
        std::cerr << "aiopt: " << created.error().detail() << '\n';
        return 1;
    }
    auto parser = std::move(created).value();

    const bench::Options defaults{};
    Tally overall;
    std::map<std::string_view, Tally> per_field;
    std::map<std::string_view, CategoryScore> per_category;
    std::vector<double> latencies;
    int exact_matches = 0;
    int rejected_assignments = 0;
    int empty_responses = 0;

    for (const bench::Case& testcase : bench::cases) {
        const auto started = std::chrono::steady_clock::now();
        auto outcome = parser.parse(testcase.utterance);
        const auto finished = std::chrono::steady_clock::now();
        latencies.push_back(std::chrono::duration<double, std::milli>(finished - started).count());

        if (!outcome) {
            std::cerr << "aiopt: " << outcome.error().detail() << '\n';
            return 1;
        }

        const bench::Options& actual = outcome->options;
        rejected_assignments += outcome->rejected;
        if (outcome->accepted == 0 && outcome->rejected == 0) {
            ++empty_responses;
        }

        bool exact = true;
        for (const bench::Field& field : bench::fields) {
            const bool gold = field.differs(testcase.expected, defaults);
            const bool predicted = field.differs(actual, defaults);
            const bool correct = !field.differs(actual, testcase.expected);
            if (!correct) {
                exact = false;
            }

            Tally& tally = per_field[field.name];
            tally.gold += gold ? 1 : 0;
            tally.predicted += predicted ? 1 : 0;
            tally.true_positive += (gold && predicted && correct) ? 1 : 0;
            overall.gold += gold ? 1 : 0;
            overall.predicted += predicted ? 1 : 0;
            overall.true_positive += (gold && predicted && correct) ? 1 : 0;
        }

        CategoryScore& score = per_category[bench::name(testcase.category)];
        ++score.total;
        score.exact += exact ? 1 : 0;
        exact_matches += exact ? 1 : 0;

        if (show_failures && !exact) {
            std::cerr << "FAIL  " << testcase.utterance << "\n      model said: \""
                      << escape(outcome->response) << "\"\n";
        }
    }

    const auto total = static_cast<int>(bench::cases.size());
    const auto percent = [](int part, int whole) {
        return whole == 0 ? 0.0 : 100.0 * static_cast<double>(part) / whole;
    };

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "model            " << argv[1] << "\nthreads          " << threads << "\ncases            "
              << total << "\n\n";

    std::cout << "exact match      " << exact_matches << "/" << total << "  (" << percent(exact_matches, total)
              << "%)\n";
    std::cout << "slot precision   " << 100.0 * overall.precision() << "%\n";
    std::cout << "slot recall      " << 100.0 * overall.recall() << "%\n";
    std::cout << "slot F1          " << 100.0 * overall.f1() << "%\n";
    std::cout << "spec rejections  " << rejected_assignments << "  (assignments the specification refused)\n";
    std::cout << "empty responses  " << empty_responses << "/" << total << "  ("
              << percent(empty_responses, total) << "%)\n\n";

    double sum = 0.0;
    for (const double sample : latencies) {
        sum += sample;
    }
    const double mean = latencies.empty() ? 0.0 : sum / static_cast<double>(latencies.size());
    std::cout << "latency ms       mean " << mean << "   p50 " << percentile(latencies, 0.50) << "   p95 "
              << percentile(latencies, 0.95) << "\n\n";

    std::cout << "by category\n";
    for (const auto& [category, score] : per_category) {
        std::cout << "  " << std::setw(12) << std::left << category << std::right << score.exact << "/"
                  << score.total << "  (" << percent(score.exact, score.total) << "%)\n";
    }

    std::cout << "\nby option        gold  pred    TP     F1\n";
    for (const bench::Field& field : bench::fields) {
        const Tally& tally = per_field[field.name];
        std::cout << "  " << std::setw(15) << std::left << field.name << std::right << std::setw(4) << tally.gold
                  << std::setw(6) << tally.predicted << std::setw(6) << tally.true_positive << std::setw(7)
                  << 100.0 * tally.f1() << "\n";
    }

    return exact_matches == total ? 0 : 1;
}
