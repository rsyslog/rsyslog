# docker/Dockerfile
# Dockerfile for rsyslog/official-docker
# This container specializes in gathering logs from the Docker daemon via imdocker
# and forwarding them to a central collector.

# Build arguments passed from the Makefile.
# BASE_IMAGE_TAG will be the full tag of the minimal image (e.g., rsyslog/official-minimal:2025-04).
ARG BASE_IMAGE_TAG
ARG UBUNTU_VERSION="24.04" # Inherited from base, but good to keep for consistency/labels
ARG RSYSLOG_IMG_VERSION="unset" # Version passed from Makefile for image metadata

# Use the rsyslog/official-minimal image as the base.
# This ensures all the core rsyslog setup (PPA, basic install, permissions) is inherited.
FROM ${BASE_IMAGE_TAG}

# Re-declare ARGs after FROM to make them available to subsequent instructions like LABEL.
ARG UBUNTU_VERSION
ARG RSYSLOG_IMG_VERSION
ARG BASE_IMAGE_TAG

LABEL maintainer="Rainer Gerhards <rgerhards@adiscon.com>"
LABEL description="Rsyslog official Docker log collector container based on minimal. Gathers logs via imdocker."
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

RUN apt-get update && \
    apt-get install -y --no-install-recommends rsyslog-imhttp && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

RUN apt-get update && \
    apt-get install -y --no-install-recommends rsyslog-omhttp && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Copy the Docker-specific rsyslog configuration snippet.
# This snippet will typically configure imdocker to listen on the Docker socket
# and forward logs to your central collector.
# Using a numerical prefix (e.g., 10-) ensures load order if multiple snippets are added.
COPY 30-docker.conf /etc/rsyslog.d/30-docker.conf


# Define the container role for the entrypoint script
ENV RSYSLOG_ROLE=docker

# Inherit CMD from base image, no need to set
