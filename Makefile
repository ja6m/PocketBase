# Makefile for PocketBase (Linux)
# Built with g++, C++17.
# Layout: sources in src/, build artifacts in build/, binary in the repo root.

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
TARGET   := pocketbase

SRC_DIR  := src
BUILD_DIR:= build

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean rebuild run debug

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@

# -MMD -MP generate per-file .d dependency files so editing kvstore.h
# triggers a rebuild of everything that includes it, not just kvstore.cpp.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

-include $(DEPS)

# Build with assertions/debug symbols and no optimization, for use with gdb.
debug: CXXFLAGS := -std=c++17 -Wall -Wextra -g -O0
debug: clean $(TARGET)

run: $(TARGET)
	./$(TARGET)

rebuild: clean all

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
