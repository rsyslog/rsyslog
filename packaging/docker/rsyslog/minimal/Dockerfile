# Dockerfile for rsyslog/rsyslog-minimal based on Ubuntu 24.04 LTS
# Using Adiscon PPA for the latest rsyslog version
# Optimized to perform cleanup in the same RUN step as installations

# Build arguments passed from the Makefile
ARG UBUNTU_VERSION="24.04"
ARG RSYSLOG_IMG_VERSION="unset" # New ARG to pick up version from Makefile
ARG RSYSLOG_APT_PPA="ppa:adiscon/v8-stable"
ARG BUILD_DATE="unknown"
ARG VCS_REF="unknown"

FROM ubuntu:${UBUNTU_VERSION}

# Re-declare ARGs after FROM to make them available to subsequent instructions like LABEL
ARG UBUNTU_VERSION
ARG RSYSLOG_IMG_VERSION
ARG RSYSLOG_APT_PPA
ARG BUILD_DATE
ARG VCS_REF

LABEL maintainer="Rainer Gerhards <rgerhards@adiscon.com>"
LABEL description="Minimal rsyslog container based on Ubuntu ${UBUNTU_VERSION} with Adiscon PPA for latest rsyslog. Optimized for size."
LABEL com.adiscon.rsyslog.image.version="${RSYSLOG_IMG_VERSION}"
LABEL org.opencontainers.image.title="rsyslog/rsyslog-minimal"
LABEL org.opencontainers.image.description="Minimal rsyslog container based on Ubuntu ${UBUNTU_VERSION} with the Adiscon rsyslog packages."
LABEL org.opencontainers.image.url="https://www.rsyslog.com/"
LABEL org.opencontainers.image.documentation="https://www.rsyslog.com/doc/containers/minimal.html"
LABEL org.opencontainers.image.source="https://github.com/rsyslog/rsyslog"
LABEL org.opencontainers.image.licenses="Apache-2.0"
LABEL org.opencontainers.image.version="${RSYSLOG_IMG_VERSION}"
LABEL org.opencontainers.image.created="${BUILD_DATE}"
LABEL org.opencontainers.image.revision="${VCS_REF}"

# Set DEBIAN_FRONTEND to noninteractive to prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# 1. Combined RUN instruction for all apt operations:
#    - Update apt cache.
#    - Install software-properties-common (provides add-apt-repository) and ca-certificates.
#    - Add the selected Adiscon PPA for rsyslog.
#    - Update apt cache again after adding the PPA to fetch package lists from it.
#    - Install rsyslog with --no-install-recommends for minimal footprint.
#    - Purge build-time-only dependencies (software-properties-common) and auto-remove any unused packages.
#    - Clean apt caches and remove temporary files to minimize layer size.
#    - Create the writable runtime paths used by the shipped config.
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        software-properties-common \
        ca-certificates \
    && add-apt-repository -y "${RSYSLOG_APT_PPA}" \
    && apt-get update && \
    apt-get install -y --no-install-recommends tzdata rsyslog rsyslog-omstdout \
    && apt-get purge -y --auto-remove software-properties-common \
    && apt-get autoremove -y \
    && apt-get clean \
    && rm -rf /etc/rsyslog.d/50-default.conf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
    && mkdir -p /etc/rsyslog.d /var/log /var/spool/rsyslog \
    && chown root:syslog /var/log \
    && chmod 0775 /var/log \
    && chown syslog:adm /var/spool/rsyslog \
    && chmod 0700 /var/spool/rsyslog

# Copy custom rsyslog configuration file into the container.
# This file defines how rsyslog behaves (e.g., inputs, outputs, rules).
# For Docker best practices, consider directing logs to stdout/stderr in your rsyslog.conf.
COPY rsyslog.conf /etc/rsyslog.conf
COPY 01-main-queue.conf /etc/rsyslog.d/
COPY start.sh /usr/local/bin/start.sh
RUN chmod 0755 /usr/local/bin/start.sh
# Set the entrypoint (this will run the script when the container starts)
ENTRYPOINT ["/usr/local/bin/start.sh"]

# Define the container role for the entrypoint script
ENV RSYSLOG_ROLE=minimal

# The minimal image is safe to run as the packaged syslog user for
# unprivileged container workloads.
USER syslog:adm

# Set the default command to run rsyslog in the foreground.
# -n: Prevents rsyslog from forking (essential for Docker containers).
# -f /etc/rsyslog.conf: Specifies the configuration file to use.
CMD ["rsyslogd", "-n", "-f", "/etc/rsyslog.conf"]
