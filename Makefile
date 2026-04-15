BUILD_DIR = build
BINARY = $(BUILD_DIR)/nemu

CMAKE_FLAGS = -G Ninja

ifdef RELEASE
CMAKE_FLAGS += -DCMAKE_BUILD_TYPE=Release
else
CMAKE_FLAGS += -DCMAKE_BUILD_TYPE=Debug
endif

.PHONY: run clean

build:
	@cmake -B $(BUILD_DIR) $(CMAKE_FLAGS)
	@cmake --build $(BUILD_DIR) -j$$(nproc 2>/dev/null || sysctl -n hw.ncpu)

run:
	$(BINARY) $(ARGS) --image $(IMG)

clean:
	rm -rf $(BUILD_DIR)
