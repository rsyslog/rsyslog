# Copyright 2026 Adiscon GmbH and others
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Daily RHEL/CentOS/EPEL package-build host image.
# Successor to rsyslog/rsyslog_dev_pkg_base_fedora:36 for EL8–EL10 mock.
#
# Image: rsyslog/rsyslog_dev_pkg_base_rocky:9
# Base:  Rocky Linux 9 + EPEL (current mock / mock-core-configs with EL10 templates)
# hadolint ignore=DL3041
FROM quay.io/rockylinux/rockylinux:9

# Workaround for buildkit + yum/dnf when ulimit is unlimited
# https://github.com/docker/buildx/issues/379#issuecomment-1196517905
RUN echo "* soft nofile 1024000" >> /etc/security/limits.conf && \
	echo "* hard nofile 1024000" >> /etc/security/limits.conf && \
	echo "session required pam_limits.so" >> /etc/pam.d/system-auth && \
	echo "ulimit -n 1024000" >> /etc/profile.d/ulimit.sh

# hadolint ignore=DL3041
RUN dnf -y update && \
	dnf -y install epel-release dnf-plugins-core && \
	dnf config-manager --set-enabled crb && \
	dnf -y update && \
	dnf clean all

# mock-core-configs from EPEL 9 ships centos-stream-10 / epel-10 / rhel-10 templates
# hadolint ignore=DL3041
RUN dnf -y install \
	wget \
	mock \
	mock-core-configs \
	git \
	sudo \
	mc \
	vim \
	screen \
	rsync \
	createrepo_c \
	rpm-sign \
	crypto-policies-scripts \
	subscription-manager && \
	dnf clean all

RUN update-crypto-policies --set LEGACY || true

RUN mkdir /private-files
VOLUME /private-files

RUN useradd pkg --uid 1001 -ms /bin/bash && \
	usermod -aG wheel pkg && \
	usermod -aG mock pkg && \
	passwd -d pkg && \
	echo "pkg ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/pkg && \
	chmod 0440 /etc/sudoers.d/pkg

WORKDIR /home/pkg
COPY extra/.rpmmacros ./

# CACHEBUST must be referenced in RUN to invalidate the git clone layer.
ARG CACHEBUST=1
RUN echo "CACHEBUST=${CACHEBUST}" && \
	git config --global --add safe.directory /home/pkg/rsyslog-pkg-rhel-centos && \
	git clone https://github.com/rsyslog/rsyslog-pkg-rhel-centos.git
WORKDIR /home/pkg/rsyslog-pkg-rhel-centos
RUN git remote add alorbach https://github.com/alorbach/rsyslog-pkg-rhel-centos.git
RUN sed -i "s/szLocalUser=test/szLocalUser=pkg/g" config.sh || \
	sed -i "s/szLocalUser\=test/szLocalUser\=pkg/g" config.sh
RUN mkdir /home/pkg/rsyslog-pkg-rhel-centos/yumrepo
VOLUME /home/pkg/rsyslog-pkg-rhel-centos/yumrepo

COPY extra/initenv.sh ./
COPY extra/do_upload.sh ./
COPY extra/do_upload_delete.sh ./
COPY extra/sync_remote.sh ./
COPY extra/sync_remote_delete.sh ./
COPY extra/cleanup_repo.sh ./
COPY extra/resignrepo.sh ./
RUN chmod +x ./initenv.sh \
	./do_upload.sh \
	./do_upload_delete.sh \
	./sync_remote.sh \
	./sync_remote_delete.sh \
	./cleanup_repo.sh \
	./resignrepo.sh

COPY extra/adisconextra.repo /etc/yum.repos.d/
RUN curl -fsSL -o /etc/pki/rpm-gpg/RPM-GPG-KEY-Adiscon \
	https://download.adiscon.com/rpms/RPM-GPG-KEY-Adiscon && \
	rpm --import /etc/pki/rpm-gpg/RPM-GPG-KEY-Adiscon

# Host-side configure/make dist deps (match Rocky 9 package CI install set).
# protobuf-c-compiler was present on Fedora 36 pkg_base and is required before
# ./configure when building modules that generate protobuf-c sources.
# hadolint ignore=DL3041
RUN dnf -y update && \
	dnf -y install \
	autoconf \
	autoconf-archive \
	make \
	automake \
	flex \
	gcc \
	gcc-c++ \
	gdb \
	libtool \
	bison \
	python3-docutils \
	libestr-devel \
	libfastjson4-devel \
	zlib-devel \
	libuuid-devel \
	libgcrypt-devel \
	libcurl-devel \
	libyaml-devel \
	protobuf-c-compiler \
	protobuf-c-devel \
	snappy-devel \
	file \
	dos2unix && \
	dnf clean all && \
	command -v protoc-gen-c

# Sanity: current mock-core-configs ships centos-stream+epel-* (and an
# epel-10 alias symlink), not legacy bare epel-8/epel-9 cfg names. Project
# etc-mock overlays restore epel-N-x86_64 names after initenv.
RUN set -eux; \
	for f in \
		/etc/mock/templates/centos-stream-8.tpl \
		/etc/mock/templates/centos-stream-9.tpl \
		/etc/mock/templates/centos-stream-10.tpl \
		/etc/mock/templates/epel-8.tpl \
		/etc/mock/templates/epel-9.tpl \
		/etc/mock/templates/epel-10.tpl \
		/etc/mock/templates/rhel-8.tpl \
		/etc/mock/templates/rhel-9.tpl \
		/etc/mock/templates/rhel-10.tpl \
		/etc/mock/centos-stream+epel-8-x86_64.cfg \
		/etc/mock/centos-stream+epel-9-x86_64.cfg \
		/etc/mock/centos-stream+epel-10-x86_64.cfg \
		/etc/mock/epel-10-x86_64.cfg \
		/etc/mock/rhel-8-x86_64.cfg \
		/etc/mock/rhel-9-x86_64.cfg \
		/etc/mock/rhel-10-x86_64.cfg; do \
		test -e "$f"; \
	done; \
	mock --version
