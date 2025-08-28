# docker/Dockerfile
# Dockerfile for rsyslog/rsyslog-docker
# This container specializes in gathering logs from the Docker daemon via imdocker
# and forwarding them. It is based on the rsyslog/rsyslog (standard) image.

# Build arguments passed from the Makefile.
# BASE_IMAGE_TAG will be the full tag of the standard image (e.g., rsyslog/rsyslog:2025-04).
ARG BASE_IMAGE_TAG="rsyslog/rsyslog:latest"
ARG UBUNTU_VERSION="24.04" # Inherited from base, but good to keep for consistency/labels
ARG RSYSLOG_IMG_VERSION="unset" # Version passed from Makefile for image metadata

# Use the rsyslog/rsyslog (standard) image as the base.
# This ensures common modules and core rsyslog setup is inherited.
FROM ${BASE_IMAGE_TAG}

# Redeclare ARGs after FROM to make them available to subsequent instructions like LABEL.
ARG UBUNTU_VERSION
ARG RSYSLOG_IMG_VERSION
ARG BASE_IMAGE_TAG

LABEL maintainer="Rainer Gerhards <rgerhards@adiscon.com>"
LABEL description="Rsyslog container specialized for Docker log collection using imdocker, based on the rsyslog/rsyslog (standard) image."
LABEL com.adiscon.rsyslog.image.version="${RSYSLOG_IMG_VERSION}"
# Explicitly label the base image used for traceability.
LABEL com.adiscon.rsyslog.base.image="${BASE_IMAGE_TAG}"

# Set DEBIAN_FRONTEND to noninteractive to prevent interactive prompts during package installation.
ENV DEBIAN_FRONTEND=noninteractive

# Install the rsyslog-imdocker module.
# This module is necessary to read logs directly from the Docker daemon's logging driver.
# apt-get update is run to ensure the package lists are fresh before installing.
RUN apt-get update && \
    apt-get install -y --no-install-recommends rsyslog-imdocker && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Copy the Docker-specific rsyslog configuration snippet.
# This snippet will typically configure imdocker to listen on the Docker socket
# and define how to process/forward these logs.
# Using a numerical prefix (e.g., 30-) helps control load order.
COPY 30-docker.conf /etc/rsyslog.d/30-docker.conf


# Define the container role for the entrypoint script
ENV RSYSLOG_ROLE=docker

# Inherit CMD from base image, no need to set.
