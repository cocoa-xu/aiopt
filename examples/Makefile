# Convenience wrapper around CMake so that trying an example out is one command.
# The build proper lives in the top level CMakeLists.txt; nothing here is
# required to use the library.
#
#   make                          build every example with the default model
#   make imgproc                  build just one
#   make MODEL=gemma3-270m        build against a smaller model
#   make MODEL_PATH=/my/own.gguf  build against a file you already have
#   make run                      build, then ask the example for help
#   make clean                    remove the build directory

# GNU make ships a rule that turns imgproc.cpp into imgproc with a bare c++
# call. It bypasses this file entirely and fails on the include path, so every
# name worth typing is declared below and the built-in rules are turned off.
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:

# Ninja when it is available, because a first build compiles llama.cpp.
GENERATOR := $(shell command -v ninja >/dev/null 2>&1 && echo -G Ninja)

BUILD ?= ../build
MODEL ?= qwen3-4b
JOBS ?= $(shell (sysctl -n hw.ncpu || nproc) 2>/dev/null || echo 4)

EXAMPLES := imgproc show_prompt

CMAKE_ARGS := -DCMAKE_BUILD_TYPE=Release -DAIOPT_EXAMPLE_MODEL=$(MODEL)
ifneq ($(MODEL_PATH),)
CMAKE_ARGS += -DAIOPT_EXAMPLE_MODEL_PATH=$(MODEL_PATH)
endif

.PHONY: all $(EXAMPLES) configure run clean help

all: $(EXAMPLES)
	@echo
	@echo "built $(BUILD)/examples/"
	@echo
	@echo "try it:"
	@echo "  cd $(BUILD)/examples"
	@echo "  ./imgproc how do I use this"
	@echo "  ./imgproc convert example.png to jpg at quality 70"
	@echo "  ./imgproc shrink example.png by half into small.png"

configure:
	@echo "aiopt: the first build fetches a model, which is a large download."
	@echo "aiopt: pass MODEL_PATH=... to use a file you already have."
	@echo
	cmake -S .. -B $(BUILD) $(GENERATOR) $(CMAKE_ARGS)

$(EXAMPLES): configure
	cmake --build $(BUILD) --target aiopt_$@ -j $(JOBS)

run: imgproc
	@echo
	cd $(BUILD)/examples && ./imgproc how do I use this

clean:
	rm -rf $(BUILD)

help:
	@echo "targets: all $(EXAMPLES) run clean"
	@echo "options: BUILD=$(BUILD) MODEL=$(MODEL) MODEL_PATH= JOBS=$(JOBS)"
