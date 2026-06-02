# docker/Dockerfile
# Dockerfile for rsyslog/rsyslog-docker
# This container specializes in gathering logs from the Docker daemon via imdocker
# and forwarding them. It is based on the rsyslog/rsyslog (standard) image.

# Build arguments passed from the Makefile.
# BASE_IMAGE_TAG will be the full tag of the standard image (e.g., rsyslog/rsyslog:2025-04).
ARG BASE_IMAGE_TAG="rsyslog/rsyslog:latest"
ARG UBUNTU_VERSION="24.04" # Inherited from base, but good to keep for consistency/labels
ARG RSYSLOG_IMG_VERSION="unset" # Version passed from Makefile for image metadata
ARG BUILD_DATE="unknown"
ARG VCS_REF="unknown"

# Use the rsyslog/rsyslog (standard) image as the base.
# This ensures common modules and core rsyslog setup is inherited.
FROM ${BASE_IMAGE_TAG}

# Re-declare ARGs after FROM to make them available to subsequent instructions like LABEL.
ARG UBUNTU_VERSION
ARG RSYSLOG_IMG_VERSION
ARG BASE_IMAGE_TAG
ARG BUILD_DATE
ARG VCS_REF

# The standard base image defaults to syslog:adm; switch back to root for
# package installation in this derived image. Common dockerlogs deployments
# also rely on Docker daemon or socket access that is root-scoped.
USER root

LABEL maintainer="Rainer Gerhards <rgerhards@adiscon.com>"
LABEL description="Rsyslog container specialized for Docker log collection using imdocker, based on the rsyslog/rsyslog (standard) image."
LABEL com.adiscon.rsyslog.image.version="${RSYSLOG_IMG_VERSION}"
# Explicitly label the base image used for traceability.
LABEL com.adiscon.rsyslog.base.image="${BASE_IMAGE_TAG}"
LABEL org.opencontainers.image.title="rsyslog/rsyslog-dockerlogs"
LABEL org.opencontainers.image.description="Rsyslog image specialized for Docker daemon log collection and forwarding."
LABEL org.opencontainers.image.url="https://www.rsyslog.com/"
LABEL org.opencontainers.image.documentation="https://www.rsyslog.com/doc/containers/dockerlogs.html"
LABEL org.opencontainers.image.source="https://github.com/rsyslog/rsyslog"
LABEL org.opencontainers.image.licenses="Apache-2.0"
LABEL org.opencontainers.image.version="${RSYSLOG_IMG_VERSION}"
LABEL org.opencontainers.image.created="${BUILD_DATE}"
LABEL org.opencontainers.image.revision="${VCS_REF}"

# Set DEBIAN_FRONTEND to noninteractive to prevent interactive prompts during package installation.
ENV DEBIAN_FRONTEND=noninteractive

# Install the rsyslog-imdocker module.
# This module is necessary to read logs directly from the Docker daemon's logging driver.
# apt-get update is run to ensure the package lists are fresh before installing.
RUN apt-get -o APT::Update::Error-Mode=any update && \
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
