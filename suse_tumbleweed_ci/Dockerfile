# container for rsyslog CI runs
# creates the build environment
FROM opensuse:tumbleweed
WORKDIR /home/ci
ADD common/projects_setup.sh /home/ci
ENV SUDO=
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig \
    LD_LIBRARY_PATH=/usr/local/lib

# Install any needed packages
RUN zypper --non-interactive install git valgrind libtool autoconf automake flex bison python-docutils python-sphinx libuuid-devel libgcrypt-devel zlib-devel libopenssl-devel libcurl-devel libgnutls-devel libmysqlclient-devel postgresql-devel net-snmp-devel systemd-devel hiredis-devel libfaketime \
	make \
	gcc \
	clang \
	gdb \
	python-devel \
	qpid-proton \
	qpid-proton-devel \
	libdbi-devel \
	libdbi-drivers-dbd-mysql \
	autoconf-archive \
	pcre-devel
#RUN echo "core" > /proc/sys/kernel/core_pattern
