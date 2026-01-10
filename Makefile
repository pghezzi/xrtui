# ===== Project =====
TARGET := xrtui

# ===== Toolchain =====
CC := gcc

# ===== Flags =====
CFLAGS  := -Wall -Wextra -O2 -std=c11
CFLAGS  += -Iinclude
LDFLAGS :=
LIBS    := -lncurses -lpanel -lmenu -lX11 -lXrandr

# ===== Directories =====
SRC_DIR := src
OBJ_DIR := build

# ===== Sources =====
SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))

# ===== Default =====
all: $(TARGET)

# ===== Link =====
$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# ===== Compile =====
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ===== Create obj dir =====
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# ===== Clean =====
clean:
	rm -rf $(OBJ_DIR) $(TARGET)

# ===== Debug =====
debug: CFLAGS += -g -O0
debug: clean all

.PHONY: all clean debug