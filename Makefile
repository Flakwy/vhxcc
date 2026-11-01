# Compiler and flags
CC       := gcc
CXX      := g++

SDIR     := src
IDIR     := include
BDIR     := build

CFLAGS   := -Wall -Wextra -O2 -fPIC -I$(IDIR) -MMD -MP
CCFLAGS  := $(CFLAGS) -std=c20
CXXFLAGS := $(CFLAGS) -std=c++20

# 1. Grab the raw filenames from src/ using wildcard
SRCS_C   := $(wildcard $(SDIR)/*.c)
SRCS_CXX := $(wildcard $(SDIR)/*.cpp)

# 2. Transform them so the object files are mapped to the build/ directory
OBJS_C   := $(patsubst $(SDIR)/%.c,$(BDIR)/%.o,$(SRCS_C))
OBJS_CXX := $(patsubst $(SDIR)/%.cpp,$(BDIR)/%.o,$(SRCS_CXX))
OBJS     := $(OBJS_C) $(OBJS_CXX)
DEPS     := $(OBJS:.o=.d)

# Target library name
TARGET   := vhxcc

# Default target
.PHONY: all
all: $(TARGET)

# Link object files to create the shared library
$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

# Rule to compile C++ files into the build directory
$(BDIR)/%.o: $(SDIR)/%.cpp | $(BDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to compile C files into the build directory
$(BDIR)/%.o: $(SDIR)/%.c | $(BDIR)
	$(CC) $(CCFLAGS) -c $< -o $@

# Order-only prerequisite to ensure the build directory exists before compiling
$(BDIR):
	mkdir -p $(BDIR)

-include $(DEPS)

# Clean up build artifacts
.PHONY: clean
clean:
	rm -rf $(BDIR) $(TARGET)
