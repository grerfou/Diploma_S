#==============================================================================
#   proj2 — Makefile
#==============================================================================

CC     = gcc
CFLAGS = -Wall -Wextra -std=c99 -I./src -I../lib \
         -Wno-unused-function \
         -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer
LIBS   = -lraylib -lm -lpthread -ldl -lavcodec -lavformat -lavutil -lswscale

DEBUG_FLAGS   = -g -O0 -DDEBUG
RELEASE_FLAGS = -O1 -DNDEBUG

SRC = src/main.c \
      src/app.c  \
      src/render.c \
      src/hud.c \
      src/input.c \
      ../lib/raymap_impl.c

TARGET    = bin/proj2
BUILD_DIR = bin

#------------------------------------------------------------------------------
# Targets
#------------------------------------------------------------------------------

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(TARGET): $(SRC)
	$(CC) $(SRC) $(CFLAGS) $(RELEASE_FLAGS) $(LIBS) -o $(TARGET)
	@echo "✓ Build OK (ASan actif) : $(TARGET)"

debug: $(BUILD_DIR)
	$(CC) $(SRC) $(CFLAGS) $(DEBUG_FLAGS) $(LIBS) -o $(TARGET)
	@echo "✓ Debug build OK"

run: all
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=0 ./$(TARGET)

clean:
	rm -rf $(BUILD_DIR)
	@echo "✓ Clean OK"

.PHONY: all debug run clean
