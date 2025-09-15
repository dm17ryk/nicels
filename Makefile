# Makefile for nls (NextLS)
.DEFAULT_GOAL := all

CXX ?= g++
BUILD ?= release   # 'release' or 'debug'

# Common flags
CXXFLAGS_COMMON := -std=c++17 -Wall -Wextra -pedantic
LDFLAGS :=
LDLIBS :=

# Build-type flags
ifeq ($(BUILD),debug)
  CXXFLAGS := $(CXXFLAGS_COMMON) -O0 -g
else ifeq ($(BUILD),release)
  CXXFLAGS := $(CXXFLAGS_COMMON) -O2
else
  $(error Unknown BUILD type '$(BUILD)'; use BUILD=debug or BUILD=release)
endif

USE_LIBGIT2 ?= 0

# Directories
SRC_DIR := src
OBJ_DIR := obj/$(BUILD)
BIN_DIR := bin

# Sources/objects/binary
SRC := $(wildcard $(SRC_DIR)/*.cpp)
OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC))
BIN := $(BIN_DIR)/nls$(if $(filter Windows_NT,$(OS)),.exe,)

# Optional: pkg-config for libgit2 (works on MSYS2, Linux, etc.)
PKGCONF ?= pkg-config
ifeq ($(USE_LIBGIT2),1)
  LIBGIT2_CFLAGS := $(shell $(PKGCONF) --cflags libgit2 2>/dev/null)
  LIBGIT2_LIBS   := $(shell $(PKGCONF) --libs   libgit2 2>/dev/null)
  CXXFLAGS += -DUSE_LIBGIT2 $(LIBGIT2_CFLAGS)
  LDLIBS   += $(LIBGIT2_LIBS)
  # Fallback if pkg-config not found or empty:
  ifeq ($(strip $(LIBGIT2_LIBS)),)
    LDLIBS += -lgit2
  endif
endif

# --- Targets ---------------------------------------------------------------

all: $(BIN)

$(BIN): $(OBJ) | $(BIN_DIR)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

# Compile objects into OBJ_DIR
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Auto-create build dirs
$(OBJ_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@

clean:
	$(RM) -r obj bin

.PHONY: all clean
