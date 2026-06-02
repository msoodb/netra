CC ?= cc

PROJECT := netra
BUILD_MODE ?= release

SRC_DIR := src
INC_DIR := include
BUILD_ROOT := build
BUILD_DIR := $(BUILD_ROOT)/$(BUILD_MODE)
BIN_DIR := bin
COMPLETION_DIR := completions

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

WARNINGS := -Wall -Wextra -Wpedantic
CPPFLAGS := -I$(INC_DIR) -MMD -MP
CFLAGS ?= -std=c11 -O2
LDFLAGS ?=
LDLIBS ?=

ifeq ($(BUILD_MODE),debug)
CFLAGS := -std=c11 -O0 -g3
endif

TARGET := $(BIN_DIR)/$(PROJECT)
BASH_COMPLETION := $(COMPLETION_DIR)/$(PROJECT).bash

ifeq ($(BUILD_MODE),debug)
TARGET := $(BIN_DIR)/$(PROJECT)-debug
endif

.PHONY: all debug clean completion install-completion run dirs

all: $(TARGET)

debug:
	$(MAKE) BUILD_MODE=debug all

run: $(TARGET)
	./$(TARGET)

completion:
	@printf '%s\n' 'Source $(BASH_COMPLETION) to enable Bash completion for netra.'
	@printf '%s\n' 'Example: source $(BASH_COMPLETION)'

install-completion: $(BASH_COMPLETION)
	install -Dm644 $(BASH_COMPLETION) $(DESTDIR)/usr/share/bash-completion/completions/$(PROJECT)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -c $< -o $@

$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

clean:
	$(RM) -r $(BUILD_ROOT) $(BIN_DIR)

-include $(DEPS)
