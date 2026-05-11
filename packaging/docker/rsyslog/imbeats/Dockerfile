# Dockerfile for a sample imbeats rsyslog container.
# Sample status: this image scaffold is not wired into the rsyslog container
# Makefile, release flow, Docker Hub metadata, or latest tags yet.

# BASE_IMAGE_TAG may point at a local or published standard rsyslog image.
ARG BASE_IMAGE_TAG="rsyslog/rsyslog:latest"

FROM ${BASE_IMAGE_TAG}

ARG BASE_IMAGE_TAG

# The standard base image defaults to syslog:adm; switch to root for package
# installation in this derived sample image.
USER root

LABEL maintainer="Rainer Gerhards <rgerhards@adiscon.com>"
LABEL description="Sample rsyslog imbeats container. Not published or wired into release automation."
LABEL com.adiscon.rsyslog.base.image="${BASE_IMAGE_TAG}"
LABEL org.opencontainers.image.title="rsyslog/rsyslog-imbeats-sample"
LABEL org.opencontainers.image.description="Sample-only rsyslog container for a TLS-enabled imbeats input."
LABEL org.opencontainers.image.source="https://github.com/rsyslog/rsyslog"
LABEL org.opencontainers.image.licenses="Apache-2.0"

ENV DEBIAN_FRONTEND=noninteractive

# Install a concrete TLS package example for the gtls stream driver.
# The package or repository source that provides imbeats.so must also be
# available in the selected base image or configured apt sources. This sample
# does not publish or pin such a package.
RUN apt-get update && \
    apt-get install -y --no-install-recommends rsyslog-gnutls && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

COPY 10-imbeats.conf /etc/rsyslog.d/10-imbeats.conf

EXPOSE 5044/tcp

ENV IMBEATS_PORT="5044"
ENV TLS_CA_FILE=""
ENV TLS_CERT_FILE=""
ENV TLS_KEY_FILE=""
ENV TLS_AUTH_MODE="anon"
ENV IMBEATS_OUTPUT_FILE="/var/log/imbeats.log"
ENV RSYSLOG_ROLE=imbeats-sample

# Return to the base image's unprivileged runtime user. The sample listens on
# port 5044 and writes under /var/log, which the base image prepares for syslog.
USER syslog:adm

# Inherit CMD from the base image.
