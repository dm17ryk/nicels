# Makefile for nls (NextLS)
.DEFAULT_GOAL := all

CXX ?= g++
# 'release' or 'debug'
BUILD ?= release

# Common flags
CXXFLAGS_COMMON := -std=c++17 -Wall -Wextra -pedantic \
    -Ithird-party/cli11/include
LDFLAGS :=
LDLIBS :=

ifeq ($(OS),Windows_NT)
  LDLIBS += -ladvapi32
endif

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
  LIBGIT2_AVAILABLE := $(shell $(PKGCONF) --exists libgit2 >/dev/null 2>&1 && echo 1 || echo 0)
  ifeq ($(LIBGIT2_AVAILABLE),1)
    LIBGIT2_CFLAGS := $(shell $(PKGCONF) --cflags libgit2 2>/dev/null)
    LIBGIT2_LIBS   := $(shell $(PKGCONF) --libs   libgit2 2>/dev/null)
    CXXFLAGS += -DUSE_LIBGIT2 $(LIBGIT2_CFLAGS)
    LDLIBS   += $(LIBGIT2_LIBS)
    ifeq ($(strip $(LIBGIT2_LIBS)),)
      LDLIBS += -lgit2
    endif
  else
    $(warning libgit2 development files not found; building without Git status support)
  endif
endif

# --- Targets ---------------------------------------------------------------

all: $(BIN)

$(BIN): $(OBJ) | $(BIN_DIR)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)
	cp -f $(BIN) .

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
