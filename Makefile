\
# Simple make-based build for nls (Linux + Windows via MSYS2/MinGW)
CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
LDFLAGS ?=
LDLIBS ?=

# Enable libgit2 status with: make USE_LIBGIT2=1
USE_LIBGIT2 ?= 0

SRC := src/main.cpp src/console.cpp src/util.cpp src/icons.cpp src/git_status.cpp
OBJ := $(SRC:.cpp=.o)
BIN := nls

# Detect Windows (cmd sets OS=Windows_NT). For MSYS2 / Git Bash uname contains MINGW.
ifeq ($(OS),Windows_NT)
  WIN := 1
endif

# libgit2 (optional)
ifeq ($(USE_LIBGIT2),1)
  CXXFLAGS += -DUSE_LIBGIT2
  LDLIBS += -lgit2
  # On Windows, you might also need (depending on your libgit2 build):
  # LDLIBS += -lws2_32 -lcrypt32 -lwinhttp -lbcrypt -lssh2 -lz
endif

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ) $(BIN)

.PHONY: all clean
