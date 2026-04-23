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

# Package source channel for all container builds.
# Local and CI builds default to the stable PPA; pass PPA_CHANNEL=daily-stable
# to switch the image family to the daily package feed.
PPA_CHANNEL ?= stable
ifeq ($(PPA_CHANNEL),stable)
RSYSLOG_APT_PPA = ppa:adiscon/v8-stable
else ifeq ($(PPA_CHANNEL),daily-stable)
RSYSLOG_APT_PPA = ppa:adiscon/daily-stable
else
RSYSLOG_APT_PPA =
endif

# Stable release targets derive the container tag from RSYSLOG_VERSION using
# 8.yymm.0 -> 20yy-mm. Example: 8.2602.0 -> 2026-02.
RELEASE_CHANNEL ?= $(PPA_CHANNEL)
RELEASE_VERSION = $(strip $(shell \
	if [ "$(RELEASE_CHANNEL)" = "daily-stable" ]; then \
		printf 'daily-stable\n'; \
	elif printf '%s\n' "$(RSYSLOG_VERSION)" | grep -Eq '^8\.[0-9]{4}\.0$$'; then \
		printf '%s\n' "$(RSYSLOG_VERSION)" | sed -E 's/^8\.([0-9]{2})([0-9]{2})\.0$$/20\1-\2/'; \
	fi))

# Default OCI metadata values for local builds.
BUILD_DATE ?= $(shell date -u +%Y-%m-%dT%H:%M:%SZ)
VCS_REF ?= $(shell git rev-parse --short=12 HEAD 2>/dev/null || echo unknown)
OCI_BUILD_ARGS = --build-arg BUILD_DATE="$(BUILD_DATE)" \
                 --build-arg VCS_REF="$(VCS_REF)"
PPA_BUILD_ARGS = --build-arg RSYSLOG_APT_PPA="$(RSYSLOG_APT_PPA)" \
                 --build-arg RSYSLOG_PPA_CHANNEL="$(PPA_CHANNEL)"

# Manual release publishing is downstream of PPA readiness. Operators must
# provide the expected rsyslog release version explicitly before stable
# release-tagged images are built or pushed. The readiness check keeps to
# the minimum: confirm that the selected release channel exposes the
# requested series.
RELEASE_UBUNTU_VERSION ?= 24.04

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
        rebuild_all check_build_channel check_publish_version check_release_inputs \
        check_ppa_release_ready release_build release_push release_publish

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

check_build_channel:
	@case "$(PPA_CHANNEL)" in \
		stable|daily-stable) \
			echo "Using container PPA channel $(PPA_CHANNEL) ($(RSYSLOG_APT_PPA))" ;; \
		*) \
			echo "ERROR: invalid PPA_CHANNEL=$(PPA_CHANNEL)." >&2; \
			echo "Expected one of: stable, daily-stable" >&2; \
			exit 1 ;; \
	esac

# Build the minimal rsyslog container.
build_minimal_image: check_build_channel ./minimal/Dockerfile ./minimal/rsyslog.conf
	@echo "--- Building minimal image: $(MINIMAL_IMAGE_TAG) ---"
	docker build $(DOCKER_BUILD_FLAGS) \
		              -t $(MINIMAL_IMAGE_TAG) \
		              $(OCI_BUILD_ARGS) \
		              $(PPA_BUILD_ARGS) \
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

check_release_inputs:
	@case "$(RELEASE_CHANNEL)" in \
		stable|daily-stable) ;; \
		*) \
			echo "ERROR: invalid RELEASE_CHANNEL=$(RELEASE_CHANNEL)." >&2; \
			echo "Expected one of: stable, daily-stable" >&2; \
			exit 1 ;; \
	esac
	@if [ "$(RELEASE_CHANNEL)" = "stable" ] && [ -z "$(RSYSLOG_VERSION)" ]; then \
		echo "ERROR: stable release targets require RSYSLOG_VERSION." >&2; \
		echo "Example: make release_build RSYSLOG_VERSION=8.2602.0" >&2; \
		exit 1; \
	fi
	@if [ -z "$(RELEASE_VERSION)" ]; then \
		echo "ERROR: invalid release input for RELEASE_CHANNEL=$(RELEASE_CHANNEL)." >&2; \
		echo "Stable example: make release_build RSYSLOG_VERSION=8.2602.0" >&2; \
		echo "Daily example: make release_build RELEASE_CHANNEL=daily-stable" >&2; \
		exit 1; \
	fi
	@if [ "$(VERSION)" != "$(DEFAULT_VERSION)" ] && [ "$(VERSION)" != "$(RELEASE_VERSION)" ]; then \
		echo "ERROR: VERSION=$(VERSION) does not match derived release tag $(RELEASE_VERSION)." >&2; \
		echo "Omit VERSION for release targets or set VERSION=$(RELEASE_VERSION)." >&2; \
		exit 1; \
	fi
	@echo "Using release channel $(RELEASE_CHANNEL)"
	@if [ -n "$(RSYSLOG_VERSION)" ]; then echo "Using rsyslog release version $(RSYSLOG_VERSION)"; fi
	@echo "Derived container tag $(RELEASE_VERSION)"

check_ppa_release_ready: check_release_inputs
	@echo "--- Checking Adiscon PPA readiness for VERSION=$(RELEASE_VERSION) ---"
	@docker run --rm ubuntu:$(RELEASE_UBUNTU_VERSION) bash -lc " \
		set -e; \
		export DEBIAN_FRONTEND=noninteractive; \
		apt-get update >/dev/null; \
		apt-get install -y --no-install-recommends ca-certificates software-properties-common >/dev/null; \
		if [ \"$(RELEASE_CHANNEL)\" = \"daily-stable\" ]; then \
			release_ppa=\"ppa:adiscon/daily-stable\"; \
		else \
			release_ppa=\"ppa:adiscon/v8-stable\"; \
		fi; \
		add-apt-repository -y \"\$$release_ppa\" >/dev/null; \
		apt-get update >/dev/null; \
		if [ \"$(RELEASE_CHANNEL)\" = \"daily-stable\" ]; then \
			resolved_version=\"\$$(apt-cache madison rsyslog | awk 'NR==1 { print \$$3; exit }')\"; \
		else \
			resolved_version=\"\$$(apt-cache madison rsyslog | awk '\$$3 ~ /^$(RSYSLOG_VERSION)-/ { print \$$3; exit }')\"; \
		fi; \
		if [ -z \"\$$resolved_version\" ]; then \
			if [ \"$(RELEASE_CHANNEL)\" = \"daily-stable\" ]; then \
				echo \"ERROR: no rsyslog package is available in \$$release_ppa.\" >&2; \
			else \
				echo \"ERROR: no rsyslog package matching $(RSYSLOG_VERSION)-* is available in \$$release_ppa.\" >&2; \
			fi; \
			exit 1; \
		fi; \
		echo \"Resolved PPA package version \$$resolved_version from \$$release_ppa.\""
	@if [ "$(RELEASE_CHANNEL)" = "daily-stable" ]; then \
		echo "PPA is ready for daily-stable"; \
	else \
		echo "PPA is ready for rsyslog $(RSYSLOG_VERSION)"; \
	fi

release_build: check_ppa_release_ready
	@echo "--- Manual release build for VERSION=$(RELEASE_VERSION) ---"
	$(MAKE) all VERSION=$(RELEASE_VERSION) PPA_CHANNEL=$(RELEASE_CHANNEL) REBUILD=$(REBUILD)

release_push: check_ppa_release_ready
	@echo "--- Manual release push for VERSION=$(RELEASE_VERSION) ---"
	$(MAKE) all_push VERSION=$(RELEASE_VERSION) PPA_CHANNEL=$(RELEASE_CHANNEL) REBUILD=$(REBUILD)

release_publish: release_push
	@if [ "$(PUSH_LATEST)" = "yes" ]; then \
		echo "--- Updating latest tags for VERSION=$(RELEASE_VERSION) ---"; \
		$(MAKE) push_latest VERSION=$(RELEASE_VERSION) PPA_CHANNEL=$(RELEASE_CHANNEL) REBUILD=$(REBUILD); \
	else \
		echo "Skipping latest tag update. Set PUSH_LATEST=yes to publish latest."; \
	fi

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
	@echo "  release_build      - Validates PPA readiness, then builds a release-tagged image set."
	@echo "  release_push       - Validates PPA readiness, then pushes release-tagged images."
	@echo "  release_publish    - Runs release_push and optionally updates ':latest'."
	@echo "                       Use PUSH_LATEST=yes to move latest."
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
	@echo "  PPA_CHANNEL        - Package source for image builds. Defaults to stable."
	@echo "                       stable -> ppa:adiscon/v8-stable"
	@echo "                       daily-stable -> ppa:adiscon/daily-stable"
	@echo "  RSYSLOG_VERSION    - Expected rsyslog release version, for example 8.2602.0."
	@echo "                       Required for RELEASE_CHANNEL=stable."
	@echo "  RELEASE_CHANNEL    - Release source. Defaults to stable."
	@echo "                       stable -> ppa:adiscon/v8-stable and 8.yymm.0 -> 20yy-mm"
	@echo "                       daily-stable -> ppa:adiscon/daily-stable and tag daily-stable"
	@echo "  PUSH_LATEST        - Set to 'yes' to let release_publish update ':latest'."
	@echo "  RELEASE_UBUNTU_VERSION - Ubuntu base used for the PPA readiness check."
	@echo "  REBUILD            - Set to 'yes' to force a full rebuild, bypassing Docker build cache."
	@echo "                       Example: make all REBUILD=yes"
	@echo ""
	@echo "Example Workflow:"
	@echo "  1. Local smoke build: make all"
	@echo "  2. Local daily build: make all PPA_CHANNEL=daily-stable"
	@echo "  3. Local release rehearsal: make VERSION=2026-03 all"
	@echo "  4. Force a full rebuild of all images: make rebuild_all"
	@echo "  5. Check PPA readiness: make check_ppa_release_ready RSYSLOG_VERSION=8.2602.0"
	@echo "  6. Manual release push: make release_push RSYSLOG_VERSION=8.2602.0"
	@echo "  7. Release push plus latest: make release_publish RSYSLOG_VERSION=8.2602.0 PUSH_LATEST=yes"
	@echo "  8. Daily channel build: make release_build RELEASE_CHANNEL=daily-stable"
