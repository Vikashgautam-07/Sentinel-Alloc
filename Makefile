# Custom Memory Allocator - Makefile
# Optimized for Flat Directory Structure

CC := gcc
CFLAGS := -Wall -Wextra -pedantic -std=c11 -pthread -fPIC
DEBUG_FLAGS := -g -O0 -DDEBUG
RELEASE_FLAGS := -O3 -DNDEBUG
LDFLAGS := -lpthread -lm

# Directories
SRC_DIR := src
INCLUDE_DIR := include
TEST_DIR := tests
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin

# Strategy-specific source selection
STRATEGY ?= BEST_FIT

ifeq ($(STRATEGY),FIRST_FIT)
ALLOCATOR_SRC := first_fit.c
STRATEGY_FLAG := -DUSE_FIRST_FIT
else
ALLOCATOR_SRC := best_fit.c
STRATEGY_FLAG := -DUSE_BEST_FIT
endif

# Source files
SERVER_SRC := main.c $(ALLOCATOR_SRC) utils.c
TEST_SRC := test_malloc.c $(ALLOCATOR_SRC) utils.c

# Object files
SERVER_OBJ := $(addprefix $(OBJ_DIR)/, $(SERVER_SRC:.c=.o))
TEST_OBJ := $(addprefix $(OBJ_DIR)/, $(TEST_SRC:.c=.o))

# Targets
RELEASE_BIN := $(BIN_DIR)/allocator_server
DEBUG_BIN := $(BIN_DIR)/allocator_server_debug
TEST_BIN := $(BIN_DIR)/test_malloc

.PHONY: all clean release debug test help install info run memcheck

# Default target
all: release

# Help
help:
	@echo "Custom Memory Allocator - Build System"
	@echo "========================================"
	@echo "Available targets:"
	@echo "  make release       - Build release binary (optimized)"
	@echo "  make debug         - Build debug binary (with symbols)"
	@echo "  make test          - Build and run test suite"
	@echo "  make clean         - Remove build artifacts"
	@echo "  make memcheck      - Run Valgrind memory leak check"
	@echo ""
	@echo "Strategy selection (default: BEST_FIT):"
	@echo "  make STRATEGY=FIRST_FIT release"

# Release build
release: CFLAGS += $(RELEASE_FLAGS) $(STRATEGY_FLAG)
release: directories $(RELEASE_BIN)
	@echo "✓ Release build complete: $(RELEASE_BIN)"

# Debug build
debug: CFLAGS += $(DEBUG_FLAGS) $(STRATEGY_FLAG)
debug: directories $(DEBUG_BIN)
	@echo "✓ Debug build complete: $(DEBUG_BIN)"

# Test build and run
test: CFLAGS += $(DEBUG_FLAGS) $(STRATEGY_FLAG)
test: directories $(TEST_BIN)
	@echo "✓ Running test suite..."
	@$(TEST_BIN)

# Main server executable
$(RELEASE_BIN): $(SERVER_OBJ)
	@mkdir -p $(BIN_DIR)
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Linked: $@"

# Debug executable
$(DEBUG_BIN): $(SERVER_OBJ)
	@mkdir -p $(BIN_DIR)
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Linked: $@"

# Test executable
$(TEST_BIN): $(TEST_OBJ)
	@mkdir -p $(BIN_DIR)
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Linked: $@"

# Object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	@$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@
	@echo "✓ Compiled: $<"

$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	@$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@
	@echo "✓ Compiled: $<"

# Directory setup
directories:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Clean build artifacts
clean:
	@rm -rf $(BUILD_DIR)
	@echo "✓ Build artifacts cleaned"

# Run server
run: release
	@$(RELEASE_BIN)

# Memory leak detection
memcheck: debug
	@echo "Running memory check..."
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes $(TEST_BIN)

info:
	@echo "Build Configuration"
	@echo "==================="
	@echo "Strategy: $(STRATEGY)"
	@echo "Compiler: $(CC)"