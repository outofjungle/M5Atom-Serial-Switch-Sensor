# M5Atom Serial Switch Sensor - Makefile
# Docker-based build environment (default) with host tools for flashing/monitoring.
# See docs/10-build-and-flash.md.
#
# Docker builds (default):
#   make build, make clean, make menuconfig, make rebuild
#
# Flash & Monitor (host tools):
#   make flash, make erase, make monitor

.PHONY: all build clean fullclean rebuild flash monitor erase \
        menuconfig shell image-build image-pull image-status help

all: build

TARGET := esp32s3

# Autodetect serial ports (macOS: cu.usbmodem*, Linux: ttyUSB*/ttyACM*).
# CDC0 (commands/samples) and the download-mode port are the lower-numbered
# match. CDC1 (logs) is the higher-numbered match once the app is running.
# See docs/09-host-integration.md.
PORT ?= $(shell ls /dev/cu.usbmodem* /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | sort | sed -n '1p')
LOG_PORT ?= $(shell ls /dev/cu.usbmodem* /dev/ttyACM* 2>/dev/null | sort | sed -n '2p')

LOGS_DIR := logs
LOG_FILE := $(LOGS_DIR)/monitor_$(shell date +%Y%m%d_%H%M%S).log

DOCKER_COMPOSE := docker-compose
DOCKER_RUN := $(DOCKER_COMPOSE) run --rm esp-idf

#------------------------------------------------------------------------------
# Docker Image Management
#------------------------------------------------------------------------------

image-build: ## Build the Docker image
	$(DOCKER_COMPOSE) build

image-pull: ## Pull the base ESP-IDF image
	docker pull espressif/idf:v5.4.1

image-status: ## Show locally built ESP-IDF related images
	@docker images | grep -E "REPOSITORY|esp|m5atom" || echo "No images found"

#------------------------------------------------------------------------------
# Docker Build
#------------------------------------------------------------------------------

build: ## Build firmware in Docker
	$(DOCKER_RUN) idf.py -C /project -D IDF_TARGET=$(TARGET) -D SDKCONFIG_DEFAULTS=/project/sdkconfig.defaults build

clean: ## Clean build output, in Docker
	$(DOCKER_RUN) idf.py -C /project fullclean

fullclean: ## Remove all build output, on the host
	rm -rf build managed_components sdkconfig sdkconfig.old dependencies.lock

rebuild: clean build ## Clean, then build

menuconfig: ## Open the ESP-IDF configuration menu, in Docker
	$(DOCKER_RUN) idf.py -C /project menuconfig

shell: ## Open an interactive shell in the Docker build container
	$(DOCKER_RUN) bash

#------------------------------------------------------------------------------
# Flash & Monitor (host tools)
#------------------------------------------------------------------------------

flash: ## Flash firmware to the device using host esptool
	@test -n "$(PORT)" || (echo "Error: No device found. Set PORT=<device>" && exit 1)
	@test -f build/flash_args || (echo "Error: Build first with 'make build'" && exit 1)
	cd build && esptool --port $(PORT) write_flash @flash_args

erase: ## Erase the device's flash memory
	@test -n "$(PORT)" || (echo "Error: No device found. Set PORT=<device>" && exit 1)
	esptool --port $(PORT) erase_flash

monitor: ## View the log port (CDC1), with logging to file
	@test -n "$(LOG_PORT)" || (echo "Error: No log port found. Set LOG_PORT=<device>" && exit 1)
	@mkdir -p $(LOGS_DIR)
	@if [ -n "$$(ls -A $(LOGS_DIR)/*.log 2>/dev/null)" ]; then \
		mkdir -p $(LOGS_DIR).bak; \
		echo "Backing up existing logs to $(LOGS_DIR).bak/"; \
		mv $(LOGS_DIR)/*.log $(LOGS_DIR).bak/ 2>/dev/null || true; \
	fi
	@echo "Logging to $(LOG_FILE)"
	@if command -v esp-idf-monitor >/dev/null 2>&1; then \
		echo "Using esp-idf-monitor (Ctrl+] to exit)"; \
		esp-idf-monitor --port $(LOG_PORT) build/M5Atom-Serial-Switch-Sensor.elf 2>&1 | tee $(LOG_FILE); \
	else \
		echo "Using screen for monitoring (Ctrl+A then K to exit)"; \
		stty -f $(LOG_PORT) 115200 raw -echo; \
		cat $(LOG_PORT) | while IFS= read -r line; do \
			echo "$$(date '+%H:%M:%S.%3N') $$line" | tee -a $(LOG_FILE); \
		done; \
	fi

#------------------------------------------------------------------------------
# Utilities
#------------------------------------------------------------------------------

help: ## Show this help
	@echo "DOCKER BUILD"
	@echo "  make build          Build firmware in Docker"
	@echo "  make clean          Clean build output, in Docker"
	@echo "  make rebuild        Clean, then build"
	@echo "  make menuconfig     Open the ESP-IDF configuration menu"
	@echo ""
	@echo "FLASH & MONITOR (host tools)"
	@echo "  make flash          Flash firmware using host esptool"
	@echo "  make monitor        View the log port (CDC1)"
	@echo "  make erase          Erase the device's flash memory"
	@echo ""
	@echo "DOCKER MANAGEMENT"
	@echo "  make image-build    Build the Docker image"
	@echo "  make image-pull     Pull the base ESP-IDF image"
	@echo "  make image-status   Show local images"
	@echo ""
	@echo "UTILITIES"
	@echo "  make shell          Open a shell in the Docker build container"
	@echo "  make fullclean      Remove all build output, on the host"
	@echo ""
	@echo "Current PORT: $(PORT)"
	@echo "Current LOG_PORT: $(LOG_PORT)"
