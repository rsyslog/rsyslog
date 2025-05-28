# ==============================================================================
# Makefile for building rsyslog official Docker containers
#
# Naming convention: rsyslog/official-<function>:<version>
# Functions: minimal, collector (extensible)
# Version: rsyslog version (e.g., 2025-04)
# ==============================================================================

# --- Configuration Variables ---
# Docker Hub organization/user
ORG_NAME = rsyslog
# Repository name prefix
REPO_PREFIX = official

# Default version for all images.
# This can be overridden via command line: 'make VERSION=my-custom-tag <target>'
VERSION ?= 2025-04

# --- Image Tags (DO NOT MODIFY MANUALLY, derived from above) ---
MINIMAL_IMAGE_NAME = $(ORG_NAME)/$(REPO_PREFIX)-minimal
COLLECTOR_IMAGE_NAME = $(ORG_NAME)/$(REPO_PREFIX)-collector

MINIMAL_IMAGE_TAG = $(MINIMAL_IMAGE_NAME):$(VERSION)
COLLECTOR_IMAGE_TAG = $(COLLECTOR_IMAGE_NAME):$(VERSION)

# --- Phony Targets (commands, not actual files) ---
# All targets that don't correspond to actual files must be declared .PHONY.
# This includes our image tags, as they are just names for build commands.
.PHONY: all build clean push all_push tag_latest push_latest help \
        minimal collector \
        build_minimal_image build_collector_image # These are the actual build targets now

# Default target: Builds all functions.
# This will call 'build_collector_image', which has 'build_minimal_image' as a prerequisite.
all: build_collector_image

# --- User-Friendly Build Targets ---
# These targets provide a simpler way to invoke the core build logic.

# Call 'make minimal' to build just the minimal image.
minimal: build_minimal_image
	@echo "Convenience target: Minimal image build triggered."

# Call 'make collector' to build just the collector image.
# It will automatically build 'minimal' first if it's not up-to-date.
collector: build_collector_image
	@echo "Convenience target: Collector image build triggered."


# --- Core Build Logic (These are explicitly Phony Targets) ---
# These targets contain the 'docker build' commands.

# Build the minimal rsyslog container.
# This target's name is now a consistent internal name.
build_minimal_image:
	@echo "--- Building minimal image: $(MINIMAL_IMAGE_TAG) ---"
	docker build -t $(MINIMAL_IMAGE_TAG) \
	             --build-arg RSYSLOG_IMG_VERSION=$(VERSION) \
	             -f ./minimal/Dockerfile ./minimal

# Build the collector rsyslog container.
# This target depends on 'build_minimal_image', ensuring it's built first.
build_collector_image: build_minimal_image
	@echo "--- Building collector image: $(COLLECTOR_IMAGE_TAG) ---"
	docker build -t $(COLLECTOR_IMAGE_TAG) \
	             --build-arg BASE_IMAGE_TAG=$(MINIMAL_IMAGE_TAG) \
	             --build-arg RSYSLOG_IMG_VERSION=$(VERSION) \
	             -f ./collector/Dockerfile ./collector

# Generic build target for convenience, alias for 'all'.
build: all

# --- Push Targets ---

# Push the minimal image with its specific version tag.
push_minimal: build_minimal_image
	@echo "--- Pushing minimal image: $(MINIMAL_IMAGE_TAG) ---"
	docker push $(MINIMAL_IMAGE_TAG)

# Push the collector image with its specific version tag.
push_collector: build_collector_image
	@echo "--- Pushing collector image: $(COLLECTOR_IMAGE_TAG) ---"
	docker push $(COLLECTOR_IMAGE_TAG)

# Push all built images with their specific version tags.
all_push: push_minimal push_collector
	@echo "All images pushed successfully with version: $(VERSION)"

# --- Tagging Targets ---

# Tag all current versioned images as 'latest'.
# Ensures all images are built before attempting to tag them.
tag_latest: build_collector_image
	@echo "--- Tagging images with 'latest' ---"
	docker tag $(MINIMAL_IMAGE_TAG) $(MINIMAL_IMAGE_NAME):latest
	docker tag $(COLLECTOR_IMAGE_TAG) $(COLLECTOR_IMAGE_NAME):latest
	@echo "All images tagged with 'latest'."

# Push all 'latest' tagged images to the registry.
# This target depends on 'tag_latest' to ensure the tags exist.
push_latest: tag_latest
	@echo "--- Pushing 'latest' images ---"
	docker push $(MINIMAL_IMAGE_NAME):latest
	docker push $(COLLECTOR_IMAGE_NAME):latest
	@echo "All 'latest' images pushed successfully."

# --- Clean Target ---

# Remove all built images locally (both versioned and 'latest' tags).
clean:
	@echo "--- Cleaning up images ---"
	docker rmi $(COLLECTOR_IMAGE_TAG) $(MINIMAL_IMAGE_TAG) || true
	docker rmi $(COLLECTOR_IMAGE_NAME):latest $(MINIMAL_IMAGE_NAME):latest || true
	@echo "Clean-up complete."

# --- Help Target ---

# Display a comprehensive help message for using the Makefile.
help:
	@echo "Usage: make [target] [VERSION=<tag>]"
	@echo ""
	@echo "Primary Build Targets:"
	@echo "  all                  - Builds all versioned images (minimal, collector)."
	@echo "  minimal              - Builds only the minimal image (versioned)."
	@echo "  collector            - Builds only the collector image (versioned, will build minimal first if needed)."
	@echo ""
	@echo "Push & Tagging Targets:"
	@echo "  push_minimal         - Builds and pushes the minimal image (versioned)."
	@echo "  push_collector       - Builds and pushes the collector image (versioned)."
	@echo "  all_push             - Builds and pushes all versioned images."
	@echo "  tag_latest           - Tags all built images with ':latest'."
	@echo "  push_latest          - Pushes all ':latest' tagged images."
	@echo ""
	@echo "Utility Targets:"
	@echo "  clean                - Removes all local built and ':latest' images."
	@echo "  help                 - Display this help message."
	@echo ""
	@echo "Variables:"
	@echo "  VERSION              - Override the default version (e.g., make VERSION=custom all)."
	@echo "                         Current default: $(VERSION)"
	@echo ""
	@echo "Example Workflow:"
	@echo "  1. Build a specific image: make minimal"
	@echo "  2. Build all images: make all"
	@echo "  3. Push all versioned images: make all_push"
	@echo "  4. Tag and push latest: make push_latest"
	@echo "  5. Get help: make help"
