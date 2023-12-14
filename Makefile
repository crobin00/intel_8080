SRC_DIR := ./src
BUILD_DIR := ./build

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,$(SRCS:.c=.o))
DEPS := $(OBJS.o=.d)

CC := gcc
CFLAGS := -std=c99 -Wall -Wextra -Werror
DEPFLAGS := -MMD -MP

TARGET_EXEC := ./intel_8080

$(TARGET_EXEC): $(OBJS)
	@mkdir -p $(@D)
	$(CC) $^ -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

-include $(DEPS)

.PHONY: clean

clean:
	rm -rf $(BUILD_DIR) $(TARGET_EXEC)
