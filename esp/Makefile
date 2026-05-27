# ─────────────────────────────────────────
# Hub ESP32-S3 — Makefile
# ─────────────────────────────────────────

PORT    = /dev/ttyACM0
TARGET  = esp32s3

.PHONY: init build run flash monitor clean

# ─────────────────────────────────────────
# Init — crée la structure du projet
# ─────────────────────────────────────────

init:
	get_idf && \
	mkdir -p main && \
	echo 'cmake_minimum_required(VERSION 3.16)\ninclude($$ENV{IDF_PATH}/tools/cmake/project.cmake)\nproject(hub)' > CMakeLists.txt && \
	echo 'idf_component_register(SRCS "main.c" INCLUDE_DIRS ".")' > main/CMakeLists.txt && \
	idf.py set-target $(TARGET)

# ─────────────────────────────────────────
# Build
# ─────────────────────────────────────────

all:
	idf.py build

# ─────────────────────────────────────────
# Flash + Monitor
# ─────────────────────────────────────────

run:
	idf.py -p $(PORT) flash monitor

# ─────────────────────────────────────────
# Flash seul
# ─────────────────────────────────────────

flash:
	idf.py -p $(PORT) flash

# ─────────────────────────────────────────
# Monitor seul
# ─────────────────────────────────────────

monitor:
	idf.py -p $(PORT) monitor

# ─────────────────────────────────────────
# Clean
# ─────────────────────────────────────────

clean:
	idf.py fullclean


# ─────────────────────────────────────────
# Clean
# ─────────────────────────────────────────

help:
	@echo "─────────────────────────────────────────"
	@echo " Hub ESP32-S3 — Commandes disponibles"
	@echo "─────────────────────────────────────────"
	@echo " make init     → initialise le projet"
	@echo " make          → compile"
	@echo " make run      → compile + flash + monitor"
	@echo " make flash    → flash seul"
	@echo " make monitor  → monitor seul"
	@echo " make clean    → nettoie le build"
	@echo " make help     → affiche cette aide"
	@echo "─────────────────────────────────────────"

