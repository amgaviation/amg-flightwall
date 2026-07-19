CXX ?= c++
CPPFLAGS := -Ifirmware/amg-flightwall/include
CXXFLAGS := -std=c++17 -O2 -g -Wall -Wextra -Wpedantic -Werror

BUILD_DIR := build/host
CORE_SOURCES := $(sort $(wildcard firmware/amg-flightwall/src/*.cpp))
SIMULATOR_SOURCE := firmware/amg-flightwall/simulator/main.cpp
TEST_SOURCE := firmware/amg-flightwall/tests/test_main.cpp
SIMULATOR_BIN := $(BUILD_DIR)/amg-flightwall-simulator
TEST_BIN := $(BUILD_DIR)/amg-flightwall-tests
ARTIFACT_DIR := $(BUILD_DIR)/artifacts

.PHONY: all simulator test check simulator-smoke clean

all: check

$(SIMULATOR_BIN): $(CORE_SOURCES) $(SIMULATOR_SOURCE)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CORE_SOURCES) $(SIMULATOR_SOURCE) -o $@

$(TEST_BIN): $(CORE_SOURCES) $(TEST_SOURCE)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CORE_SOURCES) $(TEST_SOURCE) -o $@

simulator: $(SIMULATOR_BIN)

test: $(TEST_BIN)
	$(TEST_BIN)

simulator-smoke: $(SIMULATOR_BIN)
	@mkdir -p $(ARTIFACT_DIR)
	$(SIMULATOR_BIN) $(ARTIFACT_DIR)
	@test -s $(ARTIFACT_DIR)/classic.ppm
	@test -s $(ARTIFACT_DIR)/operations.ppm
	@test -s $(ARTIFACT_DIR)/hardware-smoke.ppm

check: test simulator-smoke

clean:
	rm -rf $(BUILD_DIR)
