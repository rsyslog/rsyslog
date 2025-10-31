# Dockerfile for rsyslog/rsyslog-collector
# This container specializes in collecting logs, building on the rsyslog/rsyslog (standard) base image.

# Build arguments passed from the Makefile.
# BASE_IMAGE_TAG will be the full tag of the standard image (e.g., rsyslog/rsyslog:2025-04).
# Giving it a safe default helps suppress warnings if not provided directly.
ARG BASE_IMAGE_TAG="rsyslog/rsyslog:latest"
# Inherited from base, but good to keep for consistency/labels
ARG UBUNTU_VERSION="24.04"
# Version passed from Makefile for image metadata
ARG RSYSLOG_IMG_VERSION="unset"

# Use the rsyslog/rsyslog (standard) image as the base.
# This ensures common modules, core rsyslog setup, and imhttp are inherited.
FROM ${BASE_IMAGE_TAG}

# Re-declare ARGs after FROM to make them available to subsequent instructions like LABEL.
ARG UBUNTU_VERSION
ARG RSYSLOG_IMG_VERSION
ARG BASE_IMAGE_TAG

LABEL maintainer="Rainer Gerhards <rgerhards@adiscon.com>"
LABEL description="Rsyslog collector container, based on the standard image, optimized for ETL. Ubuntu ${UBUNTU_VERSION}."
LABEL com.adiscon.rsyslog.image.version="${RSYSLOG_IMG_VERSION}"
# Explicitly label the base image used for traceability.
LABEL com.adiscon.rsyslog.base.image="${BASE_IMAGE_TAG}"

# Set DEBIAN_FRONTEND to noninteractive to prevent interactive prompts during package installation.
ENV DEBIAN_FRONTEND=noninteractive

# Install additional rsyslog modules/packages specific to the collector's role.
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        rsyslog-elasticsearch \
        rsyslog-omhttp \
        rsyslog-relp \
    && apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Copy the collector-specific rsyslog configuration snippets.
# These files will configure inputs (like TCP/UDP syslog) and
# outputs (e.g., to Elasticsearch, or a central rsyslog via omhttp).
COPY 10-collector.conf 70-vespa_ai.conf /etc/rsyslog.d/

# Expose standard syslog ports.
# These ports will be exposed by default for UDP and TCP syslog reception.
EXPOSE 514/udp
EXPOSE 514/tcp
EXPOSE 2514/tcp

# Default config toggles for an entrypoint script to use
ENV ENABLE_UDP=on
ENV ENABLE_TCP=on
ENV ENABLE_RELP=on
ENV ENABLE_VESPA=on
ENV WRITE_ALL_FILE=on
ENV WRITE_JSON_FILE=on

# Define the container role for the entrypoint script
ENV RSYSLOG_ROLE=collector

# Inherit CMD from base image (rsyslog/rsyslog).
# No explicit CMD instruction is needed here.
