# Convenience wrapper around CMake so that trying an example out is one command.
# The build proper lives in the top level CMakeLists.txt; nothing here is
# required to use the library.
#
#   make                          build with the default model
#   make MODEL=gemma3-270m        build against a smaller one
#   make MODEL_PATH=/my/own.gguf  build against a file you already have
#   make run                      build, then ask the example for help
#   make clean                    remove the build directory

BUILD ?= ../build
MODEL ?= qwen3-4b
JOBS ?= $(shell (sysctl -n hw.ncpu || nproc) 2>/dev/null || echo 4)

CMAKE_ARGS := -DCMAKE_BUILD_TYPE=Release -DAIOPT_EXAMPLE_MODEL=$(MODEL)
ifneq ($(MODEL_PATH),)
CMAKE_ARGS += -DAIOPT_EXAMPLE_MODEL_PATH=$(MODEL_PATH)
endif

.PHONY: all run clean

all:
	@echo "aiopt: the first build fetches a model, which is a large download."
	@echo "aiopt: pass MODEL_PATH=... to use a file you already have."
	@echo
	cmake -S .. -B $(BUILD) $(CMAKE_ARGS)
	cmake --build $(BUILD) --target imgproc show_prompt -j $(JOBS)
	@echo
	@echo "built $(BUILD)/examples/imgproc"
	@echo
	@echo "try it:"
	@echo "  cd $(BUILD)/examples"
	@echo "  ./imgproc how do I use this"
	@echo "  ./imgproc convert example.png to jpg at quality 70"
	@echo "  ./imgproc shrink example.png by half into small.png"

run: all
	@echo
	cd $(BUILD)/examples && ./imgproc how do I use this

clean:
	rm -rf $(BUILD)
