# ==============================================================================
# Makefile for building rsyslog Docker containers
#
# New Naming Scheme:
# Standard Image: rsyslog/rsyslog:<VERSION>
# Functional Variants: rsyslog/rsyslog-<function>:<VERSION>
# Functions: minimal, standard, collector, dockerlogs, etl (extensible)
# Version: rsyslog version (e.g., 2025-04, matches rsyslog release)
# ==============================================================================

# --- Configuration Variables ---
# Docker Hub organization/user
ORG_NAME = rsyslog

# Default version for all images.
# Keep the default obviously non-release so local `make all` does not produce
# release-like tags by accident.
DEFAULT_VERSION = dev-local
# Override on the command line for release rehearsals, for example:
#   make all VERSION=2026-03
VERSION ?= $(DEFAULT_VERSION)

# Default OCI metadata values for local builds.
BUILD_DATE ?= $(shell date -u +%Y-%m-%dT%H:%M:%SZ)
VCS_REF ?= $(shell git rev-parse --short=12 HEAD 2>/dev/null || echo unknown)
OCI_BUILD_ARGS = --build-arg BUILD_DATE="$(BUILD_DATE)" \
                 --build-arg VCS_REF="$(VCS_REF)"

# Variable to control cache busting. Set to 'yes' to force a rebuild from scratch for all targets.
# Example: make all REBUILD=yes
REBUILD ?= no

# Conditionally add --no-cache flag if REBUILD is 'yes'
DOCKER_BUILD_FLAGS =
ifeq ($(REBUILD),yes)
    DOCKER_BUILD_FLAGS = --no-cache
endif

# --- Image Names and Tags (Derived from above) ---
# Use $(strip) to ensure no unexpected whitespace.

# Main/Standard Image
MAIN_RSYSLOG_IMAGE_NAME = $(strip $(ORG_NAME)/rsyslog)
STANDARD_IMAGE_NAME = $(strip $(MAIN_RSYSLOG_IMAGE_NAME))
STANDARD_IMAGE_TAG = $(strip $(STANDARD_IMAGE_NAME):$(VERSION))

# Functional Variants
RSYSLOG_FUNCTION_PREFIX = $(strip $(ORG_NAME)/rsyslog-)

MINIMAL_IMAGE_NAME = $(strip $(RSYSLOG_FUNCTION_PREFIX)minimal)
COLLECTOR_IMAGE_NAME = $(strip $(RSYSLOG_FUNCTION_PREFIX)collector)
DOCKERLOGS_IMAGE_NAME = $(strip $(RSYSLOG_FUNCTION_PREFIX)dockerlogs)
ETL_IMAGE_NAME = $(strip $(RSYSLOG_FUNCTION_PREFIX)etl)

MINIMAL_IMAGE_TAG = $(strip $(MINIMAL_IMAGE_NAME):$(VERSION))
COLLECTOR_IMAGE_TAG = $(strip $(COLLECTOR_IMAGE_NAME):$(VERSION))
DOCKERLOGS_IMAGE_TAG = $(strip $(DOCKERLOGS_IMAGE_NAME):$(VERSION))
ETL_IMAGE_TAG = $(strip $(ETL_IMAGE_NAME):$(VERSION))


# --- Phony Targets (commands, not actual files) ---
.PHONY: all build clean push all_push tag_latest push_latest help \
        minimal standard collector dockerlogs etl \
        build_minimal_image build_standard_image build_collector_image build_dockerlogs_image build_etl_image \
        push_minimal push_standard push_collector push_dockerlogs push_etl \
        rebuild_all check_publish_version

# Default target: Builds all functional images.
# Assumed layering: minimal -> standard -> (collector, dockerlogs, etl)
all: build_collector_image build_dockerlogs_image build_etl_image
# build_standard_image is a dependency for collector, dockerlogs, and etl
# build_minimal_image is a dependency for standard

# --- User-Friendly Build Targets ---
minimal: build_minimal_image
	@echo "Convenience target: Minimal image build triggered."

standard: build_standard_image
	@echo "Convenience target: Standard image build triggered."

collector: build_collector_image
	@echo "Convenience target: Collector image build triggered."

dockerlogs: build_dockerlogs_image
	@echo "Convenience target: Dockerlogs image build triggered."

etl: build_etl_image
	@echo "Convenience target: ETL image build triggered."


# --- Core Build Logic ---

# Build the minimal rsyslog container.
build_minimal_image: ./minimal/Dockerfile ./minimal/rsyslog.conf
	@echo "--- Building minimal image: $(MINIMAL_IMAGE_TAG) ---"
	docker build $(DOCKER_BUILD_FLAGS) \
		              -t $(MINIMAL_IMAGE_TAG) \
		              $(OCI_BUILD_ARGS) \
		              --build-arg RSYSLOG_IMG_VERSION=$(VERSION) \
		              -f ./minimal/Dockerfile ./minimal

# Build the standard rsyslog container.
# Depends on 'build_minimal_image'. Assumes ./standard/Dockerfile exists.
build_standard_image: build_minimal_image ./standard/Dockerfile # Add ./standard/rsyslog.conf if it exists
	@echo "--- Building standard image: $(STANDARD_IMAGE_TAG) ---"
	docker build $(DOCKER_BUILD_FLAGS) \
		              -t $(STANDARD_IMAGE_TAG) \
		              --build-arg BASE_IMAGE_TAG=$(MINIMAL_IMAGE_TAG) \
		              $(OCI_BUILD_ARGS) \
		              --build-arg RSYSLOG_IMG_VERSION=$(VERSION) \
		              -f ./standard/Dockerfile ./standard

# Build the collector rsyslog container.
# Depends on 'build_standard_image'.
build_collector_image: build_standard_image \
                       ./collector/Dockerfile \
                       ./collector/10-collector.conf \
                       ./collector/80-file-output.conf
	@echo "--- Building collector image: $(COLLECTOR_IMAGE_TAG) ---"
	docker build $(DOCKER_BUILD_FLAGS) \
		              -t $(COLLECTOR_IMAGE_TAG) \
		              --build-arg BASE_IMAGE_TAG=$(STANDARD_IMAGE_TAG) \
		              $(OCI_BUILD_ARGS) \
		              --build-arg RSYSLOG_IMG_VERSION=$(VERSION) \
		              -f ./collector/Dockerfile ./collector

# Build the dockerlogs rsyslog container.
# Depends on 'build_standard_image'. Dockerfile path assumed ./dockerlogs/
build_dockerlogs_image: build_standard_image \
                        ./dockerlogs/Dockerfile \
                        ./dockerlogs/30-docker.conf
	@echo "--- Building dockerlogs image: $(DOCKERLOGS_IMAGE_TAG) ---"
	docker build $(DOCKER_BUILD_FLAGS) \
		              -t $(DOCKERLOGS_IMAGE_TAG) \
		              --build-arg BASE_IMAGE_TAG=$(STANDARD_IMAGE_TAG) \
		              $(OCI_BUILD_ARGS) \
		              --build-arg RSYSLOG_IMG_VERSION=$(VERSION) \
		              -f ./dockerlogs/Dockerfile ./dockerlogs

# Build the etl rsyslog container.
# Depends on 'build_standard_image'.
build_etl_image: build_standard_image \
                  ./etl/Dockerfile \
                  ./etl/10-collector.conf \
                  ./etl/70-vespa_ai.conf
	@echo "--- Building ETL image: $(ETL_IMAGE_TAG) ---"
	docker build $(DOCKER_BUILD_FLAGS) \
		              -t $(ETL_IMAGE_TAG) \
		              --build-arg BASE_IMAGE_TAG=$(STANDARD_IMAGE_TAG) \
		              $(OCI_BUILD_ARGS) \
		              --build-arg RSYSLOG_IMG_VERSION=$(VERSION) \
		              -f ./etl/Dockerfile ./etl

# Generic build target, alias for 'all'.
build: all

rebuild_all:
	$(MAKE) all REBUILD=yes

# Publishing must always use an explicit non-development version. This keeps
# local smoke builds and CI validation tags from being pushed by mistake.
check_publish_version:
	@case "$(VERSION)" in \
		""|$(DEFAULT_VERSION)|dev-*|ci-*) \
			echo "ERROR: publish/tag targets require an explicit stable VERSION."; \
			echo "Set VERSION to a release tag, for example: make VERSION=2026-03 all_push"; \
			exit 1 ;; \
		*) \
			echo "Using publishable VERSION=$(VERSION)" ;; \
	esac

# --- Push Targets ---
push_minimal: check_publish_version build_minimal_image
	@echo "--- Pushing minimal image: $(MINIMAL_IMAGE_TAG) ---"
	docker push $(MINIMAL_IMAGE_TAG)

push_standard: check_publish_version build_standard_image
	@echo "--- Pushing standard image: $(STANDARD_IMAGE_TAG) ---"
	docker push $(STANDARD_IMAGE_TAG)

push_collector: check_publish_version build_collector_image
	@echo "--- Pushing collector image: $(COLLECTOR_IMAGE_TAG) ---"
	docker push $(COLLECTOR_IMAGE_TAG)

push_dockerlogs: check_publish_version build_dockerlogs_image
	@echo "--- Pushing dockerlogs image: $(DOCKERLOGS_IMAGE_TAG) ---"
	docker push $(DOCKERLOGS_IMAGE_TAG)

push_etl: check_publish_version build_etl_image
	@echo "--- Pushing ETL image: $(ETL_IMAGE_TAG) ---"
	docker push $(ETL_IMAGE_TAG)

all_push: push_minimal push_standard push_collector push_dockerlogs push_etl
	@echo "All images pushed successfully with version: $(VERSION)"

# --- Tagging Targets ---
# Ensures all images are built before attempting to tag them.
tag_latest: check_publish_version build_minimal_image build_standard_image build_collector_image build_dockerlogs_image build_etl_image
	@echo "--- Tagging images with 'latest' ---"
	docker tag $(STANDARD_IMAGE_TAG) $(STANDARD_IMAGE_NAME):latest
	docker tag $(MINIMAL_IMAGE_TAG) $(MINIMAL_IMAGE_NAME):latest
	docker tag $(COLLECTOR_IMAGE_TAG) $(COLLECTOR_IMAGE_NAME):latest
	docker tag $(DOCKERLOGS_IMAGE_TAG) $(DOCKERLOGS_IMAGE_NAME):latest
	docker tag $(ETL_IMAGE_TAG) $(ETL_IMAGE_NAME):latest
	@echo "All images tagged with 'latest' in their respective repositories."

push_latest: tag_latest
	@echo "--- Pushing 'latest' images ---"
	docker push $(STANDARD_IMAGE_NAME):latest
	docker push $(MINIMAL_IMAGE_NAME):latest
	docker push $(COLLECTOR_IMAGE_NAME):latest
	docker push $(DOCKERLOGS_IMAGE_NAME):latest
	docker push $(ETL_IMAGE_NAME):latest
	@echo "All 'latest' images pushed successfully."

# --- Clean Target ---
clean:
	@echo "--- Cleaning up images ---"
	docker rmi $(STANDARD_IMAGE_TAG) $(MINIMAL_IMAGE_TAG) $(COLLECTOR_IMAGE_TAG) $(DOCKERLOGS_IMAGE_TAG) $(ETL_IMAGE_TAG) || true
	docker rmi $(STANDARD_IMAGE_NAME):latest $(MINIMAL_IMAGE_NAME):latest $(COLLECTOR_IMAGE_NAME):latest $(DOCKERLOGS_IMAGE_NAME):latest $(ETL_IMAGE_NAME):latest || true
	@echo "Clean-up complete."

# --- Help Target ---
help:
	@echo "Usage: make [target] [VERSION=<tag>] [REBUILD=yes]"
	@echo ""
	@echo "Primary Build Targets:"
	@echo "  all                - Builds all versioned images (minimal, standard, collector, dockerlogs, etl)."
	@echo "  minimal            - Builds only the minimal image (versioned)."
	@echo "  standard           - Builds only the standard image (versioned, builds minimal first)."
	@echo "  collector          - Builds only the collector image (versioned, builds standard first)."
	@echo "  dockerlogs         - Builds only the dockerlogs image (versioned, builds standard first)."
	@echo "  etl                - Builds only the etl image (versioned, builds standard first)."
	@echo "  rebuild_all        - Forces a rebuild of all images, bypassing Docker cache."
	@echo ""
	@echo "Push & Tagging Targets:"
	@echo "  push_minimal       - Builds and pushes the rsyslog/rsyslog-minimal:<VERSION> image."
	@echo "  push_standard      - Builds and pushes the rsyslog/rsyslog:<VERSION> image."
	@echo "  push_collector     - Builds and pushes the rsyslog/rsyslog-collector:<VERSION> image."
	@echo "  push_dockerlogs    - Builds and pushes the rsyslog/rsyslog-dockerlogs:<VERSION> image."
	@echo "  push_etl           - Builds and pushes the rsyslog/rsyslog-etl:<VERSION> image."
	@echo "  all_push           - Builds and pushes all versioned images."
	@echo "  tag_latest         - Tags all built images with ':latest' in their respective repositories."
	@echo "  push_latest        - Pushes all ':latest' tagged images."
	@echo ""
	@echo "Utility Targets:"
	@echo "  clean              - Removes all local built and ':latest' images."
	@echo "  help               - Display this help message."
	@echo ""
	@echo "Variables:"
	@echo "  VERSION            - Override the default version (e.g., make VERSION=custom all)."
	@echo "                       Current default: $(VERSION)"
	@echo "                       The default is intentionally non-release for local builds."
	@echo "                       Publish/tag targets reject development-like values."
	@echo "  REBUILD            - Set to 'yes' to force a full rebuild, bypassing Docker build cache."
	@echo "                       Example: make all REBUILD=yes"
	@echo ""
	@echo "Example Workflow:"
	@echo "  1. Local smoke build: make all"
	@echo "  2. Local release rehearsal: make VERSION=2026-03 all"
	@echo "  3. Force a full rebuild of all images: make rebuild_all"
	@echo "  4. Push all release-tagged images: make VERSION=2026-03 all_push"
	@echo "  5. Tag and push latest for a release build: make VERSION=2026-03 push_latest"
