# ============================================================================
# Makefile for building rsyslog development environment containers
#
# Each directory containing a build.sh script is automatically exposed
# as a make target. The target name mirrors the directory path with
# slashes replaced by dashes. For example:
#   make ubuntu-base-22.04
#   make fedora-buildbot-31
#
# Additional arguments can be passed to the underlying build.sh via
# the DOCKER_ARGS variable, e.g.:
#   make ubuntu-base-22.04 DOCKER_ARGS="--no-cache"
# ============================================================================

SHELL := /bin/bash

# Discover build scripts
BUILD_SCRIPTS := $(shell find . -name build.sh | sort)
TARGET_PATHS  := $(dir $(BUILD_SCRIPTS))
TARGET_NAMES  := $(patsubst ./%,%,$(subst /,-,$(TARGET_PATHS)))

.PHONY: $(TARGET_NAMES) all list help

all: $(TARGET_NAMES)

$(TARGET_NAMES):
	@script="$(subst -,/,$@)/build.sh"; \
	dir="$$(dirname $$script)"; \
	if [ ! -f $$script ]; then \
	echo "Unknown target: $@"; exit 1; \
	fi; \
	echo "--- Building $$script ---"; \
	(cd $$dir && bash ./build.sh $(DOCKER_ARGS))

list:
	@for t in $(TARGET_NAMES); do echo $$t; done

help: list
	@echo
	@echo "Use 'make <target>' to build the corresponding dev container."
	@echo "Pass extra Docker build flags via DOCKER_ARGS."
