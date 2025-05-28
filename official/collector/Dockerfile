# Dockerfile for rsyslog/official-collector
# This container specializes in collecting logs, building on the rsyslog/official-minimal base.

# Build arguments passed from the Makefile.
# BASE_IMAGE_TAG will be the full tag of the minimal image (e.g., rsyslog/official-minimal:2025-04).
# Giving it a safe default (like 'ubuntu:latest') helps suppress warnings if not provided.
ARG BASE_IMAGE_TAG="ubuntu:latest"
ARG UBUNTU_VERSION="24.04" # Inherited from base, but good to keep for consistency/labels
ARG RSYSLOG_IMG_VERSION="unset" # Version passed from Makefile for image metadata

# Use the rsyslog/official-minimal image as the base.
# This ensures all the core rsyslog setup (PPA, basic install, permissions) is inherited.
FROM ${BASE_IMAGE_TAG}

# Re-declare ARGs after FROM to make them available to subsequent instructions like LABEL.
# This is crucial for BASE_IMAGE_TAG to be usable in labels, RUN commands, etc.
ARG UBUNTU_VERSION
ARG RSYSLOG_IMG_VERSION
ARG BASE_IMAGE_TAG

LABEL maintainer="Rainer Gerhards <rgerhards@adiscon.com>"
LABEL description="Rsyslog official collector container based on minimal, optimized for log collection. Ubuntu ${UBUNTU_VERSION}."
LABEL com.adiscon.rsyslog.image.version="${RSYSLOG_IMG_VERSION}"
# Explicitly label the base image used for traceability.
LABEL com.adiscon.rsyslog.base.image="${BASE_IMAGE_TAG}"

# Set DEBIAN_FRONTEND to noninteractive to prevent interactive prompts during package installation.
ENV DEBIAN_FRONTEND=noninteractive

# --- Optional: Install additional rsyslog modules/packages for collector functionality ---
# This section is for adding packages specific to the collector's role (e.g., database outputs, specific inputs).
# It's good practice to run 'apt-get update' before installing new packages in a new layer
# to ensure you get the latest available versions from the repositories/PPA.
#
# RUN apt-get update && \
#     apt-get install -y --no-install-recommends \
#         rsyslog-elasticsearch \
#         rsyslog-mmnormalize \
#         # Add other collector-specific rsyslog modules or general packages here.
#         # Example: a database client if logs are sent to a DB, or specific network tools.
#     && apt-get clean && \
#     rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
# ---------------------------------------------------------------------------------------

# Copy the collector-specific rsyslog configuration snippet.
# This file will be automatically included by the main /etc/rsyslog.conf
# (assuming the base rsyslog.conf contains an $IncludeConfig /etc/rsyslog.d/*.conf directive,
# which is standard for Debian/Ubuntu rsyslog packages).
# Using a numerical prefix (e.g., 10-) ensures load order if multiple snippets are added.
COPY 10-collector.conf 80-file-output.conf  /etc/rsyslog.d/

# Expose standard syslog ports.
# These ports will be exposed by default for UDP and TCP syslog reception.
EXPOSE 514/udp
EXPOSE 514/tcp

# Inherit CMD from base image:
# The CMD from the base rsyslog/official-minimal container will be automatically inherited.
# This is ideal as the base already provides the correct command to run rsyslogd in the foreground
# using the main rsyslog.conf, which in turn includes files from /etc/rsyslog.d/.
# No explicit CMD instruction is needed here.
