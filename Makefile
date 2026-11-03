# Compiler and flags
CC       := gcc
CXX      := g++

SDIR     := src
IDIR     := include
BDIR     := build

ifeq ($(OS),Windows_NT)
	HOST := Windows_NT
else
	HOST := $(shell uname -s)
endif

# Fetches the latest git tag (e.g., v1.0.4) and calculates the next micro version
VERSION := $(shell git describe --tags --abbrev=0 2>/dev/null || echo "v0.0.0")
NEXT_VERSION := $(shell echo $(VERSION) | awk -F. '{OFS="."; $$NF+=1; print $$0}')

CFLAGS   := -Wall -Wextra -O2 -I$(IDIR) -MMD -MP -DHOSTNAME=\"$(HOST)\" -DVERSION=\"$(VERSION)\"
CCFLAGS  := $(CFLAGS) -std=c26
CXXFLAGS := $(CFLAGS) -std=c++26

SRCS_C   := $(wildcard $(SDIR)/*.c)
SRCS_CXX := $(wildcard $(SDIR)/*.cpp)

OBJS_C   := $(patsubst $(SDIR)/%.c,$(BDIR)/%.o,$(SRCS_C))
OBJS_CXX := $(patsubst $(SDIR)/%.cpp,$(BDIR)/%.o,$(SRCS_CXX))
OBJS     := $(OBJS_C) $(OBJS_CXX)
DEPS     := $(OBJS:.o=.d)

TARGET_CC   := vhx-cc
TARGET_AS	:= vhx-as

AS_OBJS	 := $(filter-out $(BDIR)/vhx-cc.o, $(OBJS))
CC_OBJS	 := $(filter-out $(BDIR)/vhx-as.o, $(OBJS))

.PHONY: all
all: release

$(TARGET_AS): $(AS_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

$(TARGET_CC): $(CC_OBJS) | $(TARGET_AS)
	$(CXX) $(LDFLAGS) -o $@ $^

# Target to tag a new version in your repository
release: $(TARGET_CC) $(TARGET_AS)
	git tag $(NEXT_VERSION)
	git push origin $(NEXT_VERSION)
	@echo "Release build complete"

debug: $(TARGET_CC) $(TARGET_AS)
	@echo "Debug build complete"

$(BDIR)/%.o: $(SDIR)/%.cpp | $(BDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BDIR)/%.o: $(SDIR)/%.c | $(BDIR)
	$(CC) $(CCFLAGS) -c $< -o $@

# Order-only prerequisite to ensure the build directory exists before compiling
$(BDIR):
	mkdir -p $(BDIR)

-include $(DEPS)

# Clean up build artifacts
.PHONY: clean
clean:
	rm -rf $(BDIR) $(TARGET_AS) $(TARGET_CC)
