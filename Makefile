# Compiler and flags
CC       := gcc
CXX      := g++

SDIR     := src
IDIR     := include
BDIR     := build

CFLAGS   := -Wall -Wextra -O2 -I$(IDIR) -MMD -MP
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
all: $(TARGET_CC) $(TARGET_AS)

$(TARGET_AS): $(AS_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

$(TARGET_CC): $(CC_OBJS) | $(TARGET_AS)
	$(CXX) $(LDFLAGS) -o $@ $^

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
