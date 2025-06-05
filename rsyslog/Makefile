# ==============================================================================
# Makefile for building rsyslog official Docker containers
#
# Naming convention: rsyslog/official-<function>:<version>
# Functions: minimal, collector, docker (extensible)
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

# Variable to control cache busting. Set to 'yes' to force a rebuild from scratch for all targets.
# Example: make all REBUILD=yes
REBUILD ?= no

# Conditionally add --no-cache flag if REBUILD is 'yes'
DOCKER_BUILD_FLAGS =
ifeq ($(REBUILD),yes)
    DOCKER_BUILD_FLAGS = --no-cache
endif

# --- Image Tags (DO NOT MODIFY MANUALLY, derived from above) ---
# Use $(strip) to ensure no unexpected whitespace in image names or tags.
MINIMAL_IMAGE_NAME = $(strip $(ORG_NAME)/$(REPO_PREFIX)-minimal)
COLLECTOR_IMAGE_NAME = $(strip $(ORG_NAME)/$(REPO_PREFIX)-collector)
DOCKER_IMAGE_NAME = $(strip $(ORG_NAME)/$(REPO_PREFIX)-docker)

MINIMAL_IMAGE_TAG = $(strip $(MINIMAL_IMAGE_NAME):$(VERSION))
COLLECTOR_IMAGE_TAG = $(strip $(COLLECTOR_IMAGE_NAME):$(VERSION))
DOCKER_IMAGE_TAG = $(strip $(DOCKER_IMAGE_NAME):$(VERSION))

# --- Phony Targets (commands, not actual files) ---
# All targets that don't correspond to actual files must be declared .PHONY.
.PHONY: all build clean push all_push tag_latest push_latest help \
        minimal collector docker \
        build_minimal_image build_collector_image build_docker_image \
        rebuild_all # New target for convenience

# Default target: Builds all functions.
# This will call 'build_collector_image' and 'build_docker_image'.
# Both depend on 'build_minimal_image', which will be built only once.
all: build_collector_image build_docker_image

# --- User-Friendly Build Targets ---
# These targets provide a simpler way to invoke the core build logic.

# Call 'make minimal' to build just the minimal image.
minimal: build_minimal_image
	@echo "Convenience target: Minimal image build triggered."

# Call 'make collector' to build just the collector image.
# It will automatically build 'minimal' first if it's not up-to-date.
collector: build_collector_image
	@echo "Convenience target: Collector image build triggered."

# New: Call 'make docker' to build just the docker image.
# It will automatically build 'minimal' first if it's not up-to-date.
docker: build_docker_image
	@echo "Convenience target: Docker image build triggered."


# --- Core Build Logic (These are explicitly Phony Targets) ---
# These targets explicitly define how the Docker images are built.

# Build the minimal rsyslog container.
# Depends on its Dockerfile and rsyslog.conf.
build_minimal_image: ./minimal/Dockerfile ./minimal/rsyslog.conf
	@echo "--- Building minimal image: $(MINIMAL_IMAGE_TAG) ---"
	docker build $(DOCKER_BUILD_FLAGS) \
	             -t $(MINIMAL_IMAGE_TAG) \
	             --build-arg RSYSLOG_IMG_VERSION=$(VERSION) \
	             -f ./minimal/Dockerfile ./minimal

# Build the collector rsyslog container.
# This target depends on 'build_minimal_image', ensuring it's built first.
# Also depends on its Dockerfile and config snippets.
build_collector_image: build_minimal_image \
                       ./collector/Dockerfile \
                       ./collector/10-collector.conf \
                       ./collector/80-file-output.conf
	@echo "--- Building collector image: $(COLLECTOR_IMAGE_TAG) ---"
	docker build $(DOCKER_BUILD_FLAGS) \
	             -t $(COLLECTOR_IMAGE_TAG) \
	             --build-arg BASE_IMAGE_TAG=$(MINIMAL_IMAGE_TAG) \
	             --build-arg RSYSLOG_IMG_VERSION=$(VERSION) \
	             -f ./collector/Dockerfile ./collector

# Build the docker rsyslog container.
# This target depends on 'build_minimal_image', ensuring it's built first.
# Also depends on its Dockerfile and config snippets.
build_docker_image: build_minimal_image \
                    ./docker/Dockerfile \
                    ./docker/30-docker.conf
	@echo "--- Building docker image: $(DOCKER_IMAGE_TAG) ---"
	docker build $(DOCKER_BUILD_FLAGS) \
	             -t $(DOCKER_IMAGE_TAG) \
	             --build-arg BASE_IMAGE_TAG=$(MINIMAL_IMAGE_TAG) \
	             --build-arg RSYSLOG_IMG_VERSION=$(VERSION) \
	             -f ./docker/Dockerfile ./docker

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

# Push the docker image with its specific version tag.
push_docker: build_docker_image
	@echo "--- Pushing docker image: $(DOCKER_IMAGE_TAG) ---"
	docker push $(DOCKER_IMAGE_TAG)

# Push all built images with their specific version tags.
all_push: push_minimal push_collector push_docker
	@echo "All images pushed successfully with version: $(VERSION)"

# --- Tagging Targets ---

# Tag all current versioned images as 'latest'.
# Ensures all images are built before attempting to tag them.
tag_latest: build_collector_image build_docker_image
	@echo "--- Tagging images with 'latest' ---"
	docker tag $(MINIMAL_IMAGE_TAG) $(MINIMAL_IMAGE_NAME):latest
	docker tag $(COLLECTOR_IMAGE_TAG) $(COLLECTOR_IMAGE_NAME):latest
	docker tag $(DOCKER_IMAGE_TAG) $(DOCKER_IMAGE_NAME):latest
	@echo "All images tagged with 'latest'."

# Push all 'latest' tagged images to the registry.
# This target depends on 'tag_latest' to ensure the tags exist.
push_latest: tag_latest
	@echo "--- Pushing 'latest' images ---"
	docker push $(MINIMAL_IMAGE_NAME):latest
	docker push $(COLLECTOR_IMAGE_NAME):latest
	docker push $(DOCKER_IMAGE_NAME):latest
	@echo "All 'latest' images pushed successfully."

# --- Clean Target ---

# Remove all built images locally (both versioned and 'latest' tags).
clean:
	@echo "--- Cleaning up images ---"
	docker rmi $(COLLECTOR_IMAGE_TAG) $(MINIMAL_IMAGE_TAG) $(DOCKER_IMAGE_TAG) || true
	docker rmi $(COLLECTOR_IMAGE_NAME):latest $(MINIMAL_IMAGE_NAME):latest $(DOCKER_IMAGE_NAME):latest || true
	@echo "Clean-up complete."

# --- Help Target ---

# Display a comprehensive help message for using the Makefile.
help:
	@echo "Usage: make [target] [VERSION=<tag>] [REBUILD=yes]"
	@echo ""
	@echo "Primary Build Targets:"
	@echo "  all                  - Builds all versioned images (minimal, collector, docker)."
	@echo "  minimal              - Builds only the minimal image (versioned)."
	@echo "  collector            - Builds only the collector image (versioned, will build minimal first if needed)."
	@echo "  docker               - Builds only the docker image (versioned, will build minimal first if needed)."
	@echo "  rebuild_all          - Forces a rebuild of all images, bypassing Docker cache."
	@echo ""
	@echo "Push & Tagging Targets:"
	@echo "  push_minimal         - Builds and pushes the minimal image (versioned)."
	@echo "  push_collector       - Builds and pushes the collector image (versioned)."
	@echo "  push_docker          - Builds and pushes the docker image (versioned)."
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
	@echo "  REBUILD              - Set to 'yes' to force a full rebuild, bypassing Docker build cache."
	@echo "                         Example: make all REBUILD=yes"
	@echo ""
	@echo "Example Workflow:"
	@echo "  1. Build a specific image: make docker"
	@echo "  2. Build all images: make all"
	@echo "  3. Force a full rebuild of all images: make rebuild_all"
	@echo "  4. Push all versioned images: make all_push"
	@echo "  5. Tag and push latest: make push_latest"
	@echo "  6. Get help: make help"
