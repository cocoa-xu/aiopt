# aiopt

Have you ever tired to remember the flags for a command line program or explain to your user how to use it?

`aiopt` comes to the rescue. It is a C++ library that lets you declare the options your program accepts, and then users can describe what they want in ordinary prose. A small language model runs locally to turn one into the other.

## Motivation

Fun and because I can!

## Example

See the [imgproc](./examples/imgproc.cpp) example for a program that converts and resizes images. It can be invoked like this:

```
$ ./imgproc "heyo convert example.png to jpg, this thing is huge so make it small"
understood:
  input       example.png
  output      .
  format      jpg
  resize      50%
  quality     85
  max-width   unchanged
  recursive   false
  overwrite   true
  dry-run     false
  jobs        1

1 image(s) found, 1 to process

  wrote ./example.jpg  300x543

wrote 1, failed 0
```

If the user asks for help, the program can show usage examples written by the model itself, and they don't necessarily need to quote the arguments, and the best part is, it can even generate different usage examples

```
$ ./imgproc how on earth do I even use this, give me five examples
imgproc — convert and resize images, described in plain language

Say what you want in plain language:

  imgproc "..."

For example:

  imgproc "Show me how to resize and convert images to PNG"
  imgproc "Convert all photos to JPG with 90 quality"
  imgproc "Resize images to 1080x1920 and save as JPEG"
  imgproc "Process images in folder and output to new directory"
  imgproc "Convert images to BMP with max width 800 pixels"

What a request can set:

  help       yes or no                                   the request asks what this program can do, how to use it, or for examples of using it, rather than asking for work to be done
  recursive  yes or no                                   descend into subdirectories when collecting inputs
  dry-run    yes or no                                   report what would happen without writing any file
  overwrite  yes or no                                   replace files that already exist; false leaves them and skips that image
  verbose    yes or no                                   true reports each image as usual; false is quiet, silent, prints nothing at all, and produces no output
  input      a path                                      source image, or a directory of them, to read
  output     a path                                      destination file or directory to write the results to
  format     png | jpg | bmp                             output encoding
  quality    1 to 100                                    JPEG compression quality, higher means larger files
  resize     a string like "50%", "0.5", or "1920x1080"  scale every image by a factor or to exact pixels
  max-width  0 to 16384                                  shrink only images wider than this, keeping their shape; ignored when resize is set
  jobs       1 to 64                                     number of images to process at once
  examples   1 to 10                                     how many usage examples to show; a request asking for examples is asking for help, so set help to true whenever this is set

Anything a request does not mention keeps its default.
```

And since it's AI-powered, it can show the users different usage examples every time they ask for help

```
$ ./imgproc how on earth do I even use this, give me five examples
...
For example:

  imgproc "Show me how to resize and convert images to PNG"
  imgproc "Convert all photos to JPG with 90 quality"
  imgproc "Resize images to 50% and save as output"
  imgproc "Process images in folder and output to new dir"
  imgproc "Resize wide images to max 1920 pixels wide"
...
```

For vague requests, the model can infer what the user wants:

```
$ ./imgproc convert ./example.png to jpg, like half the size, and super tiny disk space usage
understood:
  input       ./example.png
  output      .
  format      jpg
  resize      50%
  quality     10
  max-width   unchanged
  recursive   false
  overwrite   true
  dry-run     false
  jobs        1

1 image(s) found, 1 to process

  wrote ./example.jpg  300x543

wrote 1, failed 0
```

## Features

Specify your command line arguments in a simple and intuitive way, and let `aiopt` handle the parsing for you. It supports:

- **Options as a plain struct.** Each option holds a pointer-to-member, so a resolved value is written straight into your own type with no lookup, no allocation, and no type casting.
- **Nothing leaves the machine.** Inference runs locally through `llama.cpp`. There is no network call and no API key.
- **Grammar-constrained decoding.** The declared type of every option is compiled into a GBNF grammar, so a boolean can only be answered `true` or `false`, an enumeration only with one of its own labels, and a key outside the specification cannot be written at all.
- **Types the library has never heard of.** `aiopt::custom` takes a syntax, a grammar rule, and a reader, so a program can accept a value like `"50%"` or `"1920x1080"` without aiopt knowing what either means.
- **Help generated from the same source.** The help text is derived from the specification the model reads, so the two can never describe different programs, and the usage examples in it can be written by the model itself.

## Usage

```cpp
#include <aiopt/aiopt.hpp>
#include <iostream>
#include <string>

enum class Format { png, jpg, bmp };

struct Options {
    bool recursive = false;
    std::string input;
    std::string output;
    Format format = Format::png;
    int quality = 85;
};

constexpr auto specification = aiopt::spec<Options>(
    aiopt::flag(&Options::recursive, "recursive", "descend into subdirectories"),
    aiopt::path(&Options::input, "input", "source image, or a directory of them, to read"),
    aiopt::path(&Options::output, "output", "destination file or directory to write to"),
    aiopt::choice(&Options::format, "format", "output encoding", "png", "jpg", "bmp"),
    aiopt::number(&Options::quality, "quality", "JPEG quality, higher is larger", 1, 100));

int main(int argc, char** argv) {
    auto created = aiopt::make_parser(specification, "model.gguf");
    if (!created) {
        std::cerr << created.error().detail() << '\n';
        return 1;
    }

    auto parser = std::move(created).value();
    auto outcome = parser.parse(argv[1]);
    if (!outcome) {
        std::cerr << outcome.error().detail() << '\n';
        return 1;
    }

    const Options& options = outcome->options;
    std::cout << options.output << ' ' << options.quality << '\n';
}
```

See the [examples](./examples) directory for more usage examples, including one that decodes, rescales, and re-encodes real images.

There is a `Makefile` there that wraps CMake, so trying one is a single command:

```console
$ cd examples && make
$ cd ../build/examples

$ ./imgproc how do I use this
$ ./imgproc convert example.png to jpg at quality 70
$ ./imgproc shrink example.png by half into small.png
```

`example.png` is staged next to the binary by the build, so those commands work as written. The
first build fetches a model, which is a large download; pass `make MODEL=gemma3-270m` for a smaller
one, or `make MODEL_PATH=/my/own.gguf` to use a file you already have.

```
$ cd examples
$ make imgproc
```

## Design

An option's description is not documentation, it is the prompt. It is the text the model reads to decide what a request means, and a vague description produces a vague parser. Everything else follows from that.

1. The specification is `constexpr`, so the option names, types, bounds, and enumeration labels are all known at compile time.
2. That specification is rendered once into a prompt prefix, and into a GBNF grammar in which each key carries the value rule its declared type implies.
3. The prefix is byte-identical on every run of a given program, so it is decoded once into the key/value cache and reused; only the request itself is processed per parse.
4. The model answers with a flat JSON object keyed by option name. It is constrained to that shape, so it cannot answer with a key you did not declare or a value your type does not admit.
5. Each assignment is offered back to the specification, which refuses anything violating a declared bound. `Outcome` reports what was accepted and what was refused.

## Integration

<details>
  <summary>CMake (FetchContent)</summary>

  ```cmake
  include(FetchContent)
  FetchContent_Declare(
      aiopt
      GIT_REPOSITORY https://github.com/cocoa-xu/aiopt.git
      GIT_TAG main
  )
  FetchContent_MakeAvailable(aiopt)
  target_link_libraries(program PRIVATE aiopt::aiopt)
  ```
</details>

<details>
  <summary>CMake (vendored)</summary>

  ```cmake
  add_subdirectory(vendor/aiopt)
  target_link_libraries(program PRIVATE aiopt::aiopt)
  ```
</details>

<details>
  <summary>CMake (installed)</summary>

  ```cmake
  find_package(aiopt CONFIG REQUIRED)
  target_link_libraries(program PRIVATE aiopt::aiopt)
  ```

  Installation is only offered when `llama.cpp` comes from outside the build tree; an exported
  target may only depend on targets that are themselves exported.
</details>

`llama.cpp` is resolved in three ways: an existing `llama` target, a checkout named by
`AIOPT_LLAMA_SOURCE_DIR`, or a pinned `FetchContent` download.

### Choosing a model

The examples compile a model path in rather than taking one on the command line, since which model
to use is a build-time decision:

```console
$ cmake -B build -DAIOPT_EXAMPLE_MODEL=qwen3-4b            # default; fetched on first build
$ cmake -B build -DAIOPT_EXAMPLE_MODEL_PATH=/my/own.gguf   # use a file you already have
```

Fetched models are cached under `AIOPT_MODEL_CACHE_DIR`, shared across build directories.

### Overriding the prompt

The prompt is assembled from named pieces, every one of which can be replaced at compile time by defining its macro before including the header. Nothing about the wording is privileged; these are the defaults, not the contract.

| macro | governs |
|---|---|
| `AIOPT_SYSTEM_PROMPT` | the standing instructions that open the prompt |
| `AIOPT_GUIDANCE_BOOLEAN` | how a boolean key must be answered |
| `AIOPT_GUIDANCE_INTEGER` | how a numeric key must be answered, and that it must stay in range |
| `AIOPT_GUIDANCE_CHOICE` | that a choice key takes one of its own labels, spelled exactly |
| `AIOPT_GUIDANCE_PATH` | that a path must already appear in the request, copied character for character |
| `AIOPT_GUIDANCE_TEXT` | that free text must be taken from the request rather than invented |
| `AIOPT_GUIDANCE_CUSTOM` | that a custom key follows the syntax shown beside it |
| `AIOPT_LANGUAGE_PROMPT` | how a sample of the user's words decides which language help comes back in |
| `AIOPT_EXAMPLES_PROMPT` | what the usage examples in help should look like |

The six `AIOPT_GUIDANCE_*` blocks are emitted only when the specification actually uses that type, so a program with no paths never pays for the paragraph about paths, and a program with no custom types never mentions them.

Two functions take their instructions as an argument as well, to replace them for a single call rather than for the whole program: `render_prefix(descriptors, instructions)` and `render_suggestion_request(count, request, instructions)`.

Two things are worth knowing before rewriting any of this, because both cost measurable accuracy when they were got wrong:

**A description is the prompt, not documentation.** Every measurable jump in this project came from
changing what a description says. Widening `verbose` from "prints nothing at all" to also name
"quiet" and "silent" was the difference between the model understanding *quietly* and not. If a
request is being misread, the description is the first place to look, not the wording of these
macros.

**Do not name specific languages in `AIOPT_LANGUAGE_PROMPT`.** Saying "Chinese sample, Chinese
examples" makes a model latch onto the names in the instruction rather than read the sample, and
English requests start coming back in Chinese. The rule has to stay abstract. Order matters for the
same reason: whatever sits closest to the point of generation pulls hardest, so the sample goes
first and the instructions last.

## Help

Help is not a special case in the library. It is an option a program declares like any other, and
the model decides whether a request is asking for work or asking what the program does:

```cpp
aiopt::flag(&Options::help, "help",
            "the request asks what this program can do, how to use it, or for examples of "
            "using it, rather than asking for work to be done"),
```

`help`, `-h`, `what can this do?`, `怎么用啊` and `使い方を教えて` all reach it, while a genuine
request does not. Running a program with no argument at all is unambiguous, so it can answer without
loading a model.

`aiopt::render_help` builds the text from the same descriptors the model reads, so help and parser
can never describe different programs. The usage examples in it are written by the model rather than
by the author, through `Parser::suggest`:

```cpp
auto suggested = parser.suggest(aiopt::Suggestions{
    .count = 5,              // how many to write
    .request = argv[1],      // decides which language they come back in
    .sampling = {0.75f, 0.92f, seed},
});
```

Passing the request through is what makes the examples come back in the language they were asked
in — including languages the author never considered, and dialects, without a translation file
anywhere. Sampling is deliberately not greedy here, so a fresh seed gives a fresh set; parsing keeps
its deterministic sampler either way, because the same command line has to resolve the same way
every time.

Suggestion quality tracks how strong the chosen model is in a given language. It reliably answers
in the right language; whether every example reads as a request for work rather than a question
varies, and the weaker the model is in that language, the more it varies.

## Benchmarks

`bench/correctness` measures whether a model produces the assignments a specification asked for,
over 108 cases: 36 each in English, Chinese, and Japanese, written as parallel translations so the
same category mix is asked in every language. Cases span direct phrasing, paraphrase, several
options at once, negation, distractors, and requests needing inference. It measures parsing only;
the quality of generated help is not scored.

Measured on an Apple M4 Max, CPU only:

| model | exact match | slot F1 | mean latency |
|---|---:|---:|---:|
| Qwen3-4B-Instruct Q4_K_M | **87.0%** | **93.5%** | 397 ms |
| SmolLM2-360M Q8 | 0.0% | 17.3% | 522 ms |
| Gemma3-270M Q4 | 12.0% | 18.1% | 184 ms |

By language, for Qwen3-4B: English 91.7% exact / 94.4% F1, Japanese 88.9% / 95.5%, Chinese
80.6% / 90.7%.

Be aware of what those numbers say. **A model of roughly this size is required.** Models below 1B
parameters do not reach usable accuracy on this task, whatever the prompt, and they hallucinate
values that never appeared in the request. Constraining string values to spans of the input would
address that structurally, and is not implemented yet.

## License

`aiopt` is licensed under the MIT License. See [LICENSE](./LICENSE) for more information.
