CC ?= cc

PROJECT := nta_netra

SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
BIN_DIR := bin

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

WARNINGS := -Wall -Wextra -Wpedantic
CPPFLAGS := -I$(INC_DIR) -MMD -MP
CFLAGS ?= -std=c11 -O2
LDFLAGS ?=
LDLIBS ?=

TARGET := $(BIN_DIR)/$(PROJECT)

.PHONY: all debug clean run dirs

all: $(TARGET)

debug: CFLAGS := -std=c11 -O0 -g3
debug: $(TARGET)

run: $(TARGET)
	./$(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -c $< -o $@

$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

clean:
	$(RM) -r $(BUILD_DIR) $(BIN_DIR)

-include $(DEPS)
