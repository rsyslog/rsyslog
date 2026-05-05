#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DOCKERFILE="$REPO_ROOT/packaging/docker/dev_env/ubuntu/base/24.04/Dockerfile"
if [ "$(id -u)" -eq 0 ] && [ -z "${SUDO_USER:-}" ]; then
	BUILD_USER="$(stat -c %U "$REPO_ROOT")"
else
	BUILD_USER="${SUDO_USER:-${USER:-$(id -un)}}"
fi
BUILD_HOME="$(getent passwd "$BUILD_USER" | cut -d: -f6)"
CACHE_DIR="${RSYSLOG_DEV_SETUP_CACHE:-$BUILD_HOME/.cache/rsyslog-dev-deps}"
STAMP_DIR="/usr/local/share/rsyslog-dev-setup/stamps"
ALLOW_WINDOWS_MOUNT=0
VERIFY_BUILD=0
APT_MIRROR=""
ORIGINAL_ARGS=("$@")

usage() {
	cat <<'USAGE'
Usage: devtools/codex-setup.sh [options]

Prepare an Ubuntu 24.04 WSL environment for full rsyslog development.

Options:
  --apt-mirror URL        Rewrite Ubuntu apt sources to URL before updating.
  --verify-build          Run autoreconf, configure, and make after setup.
  --allow-windows-mount   Permit running from /mnt/*, not recommended.
  -h, --help              Show this help text.
USAGE
}

while [ "$#" -gt 0 ]; do
	case "$1" in
		--apt-mirror)
			APT_MIRROR="${2:-}"
			if [ -z "$APT_MIRROR" ]; then
				echo "error: --apt-mirror requires a URL" >&2
				exit 2
			fi
			shift 2
			;;
		--verify-build)
			VERIFY_BUILD=1
			shift
			;;
		--allow-windows-mount)
			ALLOW_WINDOWS_MOUNT=1
			shift
			;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			echo "error: unknown option: $1" >&2
			usage >&2
			exit 2
			;;
	esac
done

if [ "$(id -u)" -ne 0 ]; then
	exec sudo -E "$0" "${ORIGINAL_ARGS[@]}"
fi

log() {
	printf '\n==> %s\n' "$*"
}

run_as_build_user() {
	if [ "$BUILD_USER" = "root" ]; then
		"$@"
	else
		sudo -H -u "$BUILD_USER" "$@"
	fi
}

require_ubuntu_2404() {
	if [ ! -r /etc/os-release ]; then
		echo "error: /etc/os-release is missing" >&2
		exit 1
	fi
	. /etc/os-release
	if [ "${ID:-}" != "ubuntu" ] || [ "${VERSION_ID:-}" != "24.04" ]; then
		echo "error: this setup targets Ubuntu 24.04; found ${PRETTY_NAME:-unknown}" >&2
		exit 1
	fi
}

refuse_windows_mount_checkout() {
	case "$REPO_ROOT" in
		/mnt/*)
			if [ "$ALLOW_WINDOWS_MOUNT" -ne 1 ]; then
				cat >&2 <<'EOF'
error: rsyslog checkout is under /mnt/*.

Use a WSL-native checkout, for example /home/<user>/rsyslog, to avoid CRLF,
filemode, symlink, and build performance problems. Re-run with
--allow-windows-mount only if you intentionally accept those risks.
EOF
				exit 1
			fi
			;;
	esac
}

configure_git_line_endings() {
	log "Configuring WSL Git for LF checkouts"
	run_as_build_user git config --global core.autocrlf false
	run_as_build_user git config --global core.eol lf
	run_as_build_user git config --global core.filemode true
	run_as_build_user git -C "$REPO_ROOT" config core.filemode true
}

rewrite_apt_mirror() {
	local mirror="$1"
	local sources="/etc/apt/sources.list.d/ubuntu.sources"
	local backup="/etc/apt/ubuntu.sources.codex-backup"
	local legacy_backup="$sources.codex-backup"

	if [ -z "$mirror" ]; then
		return
	fi

	if [ ! -f "$sources" ]; then
		echo "error: cannot apply --apt-mirror because $sources is missing" >&2
		exit 1
	fi

	log "Rewriting Ubuntu apt sources to $mirror"
	if [ ! -f "$backup" ]; then
		cp "$sources" "$backup"
	fi
	if [ -f "$legacy_backup" ]; then
		rm -f "$legacy_backup"
	fi

	python3 - "$sources" "$mirror" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
mirror = sys.argv[2].rstrip("/") + "/"
text = path.read_text()
lines = []
for line in text.splitlines():
    if line.startswith("URIs: "):
        lines.append("URIs: " + mirror)
    else:
        lines.append(line)
path.write_text("\n".join(lines) + "\n")
PY
}

apt_update() {
	log "Updating apt package indexes"
	if apt-get update; then
		return
	fi
	cat >&2 <<'EOF'

apt-get update failed. If the default Canonical mirror times out from WSL,
re-run with a responding mirror, for example:

  sudo ./devtools/codex-setup.sh --apt-mirror http://de.archive.ubuntu.com/ubuntu/
EOF
	exit 1
}

install_apt_packages() {
	log "Installing Ubuntu 24.04 rsyslog development packages"
	apt-get install -y \
		autoconf \
		autoconf-archive \
		automake \
		autotools-dev \
		bison \
		ca-certificates \
		clang \
		clang-tools \
		cmake \
		cmake-curses-gui \
		curl \
		default-jdk \
		default-jre \
		faketime \
		flex \
		g++ \
		gcc \
		gcc-14 \
		gdb \
		git \
		gnupg \
		libaprutil1-dev \
		libbson-dev \
		libcap-ng-dev \
		libcap-ng0 \
		libcivetweb-dev \
		libczmq-dev \
		libcurl4-gnutls-dev \
		libdbd-mysql \
		libdbi-dev \
		libevent-dev \
		libgcrypt20-dev \
		libglib2.0-dev \
		libgnutls28-dev \
		libhiredis-dev \
		libkrb5-dev \
		liblz4-dev \
		libmaxminddb-dev \
		libmbedtls-dev \
		libmongoc-dev \
		libmysqlclient-dev \
		libnet1-dev \
		libpcap-dev \
		libprotobuf-c-dev \
		librabbitmq-dev \
		libsasl2-2 \
		libsasl2-dev \
		libsodium-dev \
		libsasl2-modules \
		libsnappy-dev \
		libsnmp-dev \
		libssl-dev \
		libsystemd-dev \
		libtirpc-dev \
		libtokyocabinet-dev \
		libtool \
		libtool-bin \
		libwolfssl-dev \
		libyaml-dev \
		libzstd-dev \
		logrotate \
		lsof \
		make \
		mysql-server \
		net-tools \
		pkg-config \
		postgresql-client \
		libpq-dev \
		protobuf-c-compiler \
		python3-dev \
		python3-docutils \
		python3-pip \
		python3-pysnmp4 \
		python3-sphinx \
		ruby-dev \
		software-properties-common \
		sudo \
		swig \
		tcl-dev \
		uuid-dev \
		valgrind \
		vim \
		wget \
		zlib1g-dev \
		zstd
}

install_obs_packages() {
	local list_file="/etc/apt/sources.list.d/home-rgerhards.sources"
	local keyring="/usr/share/keyrings/home-rgerhards.gpg"

	log "Installing rsyslog helper packages from the rgerhards OBS repository"
	if [ ! -f "$keyring" ]; then
		wget -nv https://download.opensuse.org/repositories/home:rgerhards/xUbuntu_22.04/Release.key -O - \
			| gpg --dearmor > "$keyring"
	fi

	cat > "$list_file" <<EOF
Types: deb
URIs: http://download.opensuse.org/repositories/home:/rgerhards/xUbuntu_22.04/
Suites: /
Signed-By: $keyring
EOF

	apt-get update
	apt-get install -y \
		libestr-dev \
		liblogging-stdlog-dev \
		liblognorm-dev
}

prepare_build_cache() {
	mkdir -p "$CACHE_DIR" "$STAMP_DIR"
	chown -R "$BUILD_USER:$BUILD_USER" "$CACHE_DIR"
}

stamp_exists() {
	test -f "$STAMP_DIR/$1"
}

write_stamp() {
	touch "$STAMP_DIR/$1"
}

clone_clean() {
	local url="$1"
	local dir="$2"
	rm -rf "$dir"
	run_as_build_user git clone --depth 1 "$url" "$dir"
}

multiarch_libdir() {
	printf '/usr/lib/%s\n' "$(gcc -print-multiarch)"
}

build_codestyle() {
	local stamp="codestyle"
	local dir="$CACHE_DIR/codestyle"
	if stamp_exists "$stamp"; then
		log "codestyle already installed"
		return
	fi
	log "Building rsyslog codestyle checker"
	clone_clean https://github.com/rsyslog/codestyle "$dir"
	run_as_build_user gcc --std=c99 "$dir/stylecheck.c" -o "$dir/stylecheck"
	install -m 0755 "$dir/stylecheck" /usr/bin/rsyslog_stylecheck
	write_stamp "$stamp"
}

build_libfastjson() {
	local stamp="libfastjson"
	local dir="$CACHE_DIR/libfastjson"
	if stamp_exists "$stamp"; then
		log "libfastjson already installed"
		return
	fi
	local libdir
	log "Building libfastjson"
	clone_clean https://github.com/rsyslog/libfastjson.git "$dir"
	cd "$dir"
	libdir="$(multiarch_libdir)"
	autoreconf -fi
	./configure --prefix=/usr --libdir="$libdir" --includedir=/usr/include
	make -j"$(nproc)"
	make install
	rm -f "$libdir/libfastjson.a"
	write_stamp "$stamp"
}

build_faup() {
	local stamp="faup"
	local dir="$CACHE_DIR/faup"
	if stamp_exists "$stamp"; then
		log "faup already installed"
		return
	fi
	log "Building faup"
	clone_clean https://github.com/stricaud/faup.git "$dir"
	cmake -S "$dir" -B "$dir/build"
	make -C "$dir/build" -j"$(nproc)"
	make -C "$dir/build" install
	rm -f /usr/local/lib/libfaupl.a
	write_stamp "$stamp"
}

build_libksi() {
	local stamp="libksi"
	local dir="$CACHE_DIR/libksi"
	if stamp_exists "$stamp"; then
		log "libksi already installed"
		return
	fi
	log "Building Guardtime libksi"
	clone_clean https://github.com/guardtime/libksi.git "$dir"
	cd "$dir"
	autoreconf -fvi
	./configure --prefix=/usr
	make -j"$(nproc)"
	make install
	rm -f /usr/lib/libksi.a
	write_stamp "$stamp"
}

build_librelp() {
	local stamp="librelp"
	local dir="$CACHE_DIR/librelp"
	if stamp_exists "$stamp"; then
		log "librelp already installed"
		return
	fi
	log "Building librelp"
	clone_clean https://github.com/rsyslog/librelp.git "$dir"
	cd "$dir"
	autoreconf -fi
	./configure --prefix=/usr --enable-compile-warnings=yes --libdir=/usr/lib --includedir=/usr/include
	make -j"$(nproc)"
	make install
	rm -f /usr/lib/librelp.a
	write_stamp "$stamp"
}

build_qpid_proton() {
	local stamp="qpid-proton"
	local dir="$CACHE_DIR/qpid-proton"
	if stamp_exists "$stamp"; then
		log "qpid-proton already installed"
		return
	fi
	log "Building qpid-proton"
	clone_clean https://github.com/apache/qpid-proton.git "$dir"
	cmake -S "$dir" -B "$dir/build" -DCMAKE_INSTALL_PREFIX=/usr -DSYSINSTALL_BINDINGS=ON -DBUILD_TESTING=OFF
	make -C "$dir/build" -j"$(nproc)"
	make -C "$dir/build" install
	rm -f /usr/lib/libqpid-proton-core.a /usr/lib/libqpid-proton-cpp.a \
		/usr/lib/libqpid-proton-proactor.a /usr/lib/libqpid-proton.a
	write_stamp "$stamp"
}

build_librdkafka() {
	local stamp="librdkafka-v1.5.3"
	local dir="$CACHE_DIR/librdkafka"
	if stamp_exists "$stamp"; then
		log "librdkafka v1.5.3 already installed"
		return
	fi
	log "Building librdkafka v1.5.3"
	clone_clean https://github.com/edenhill/librdkafka "$dir"
	cd "$dir"
	run_as_build_user git fetch --depth 1 origin tag v1.5.3
	run_as_build_user git checkout v1.5.3
	(
		unset CFLAGS
		./configure --prefix=/usr --libdir=/usr/lib --CFLAGS="-g"
		make -j"$(nproc)"
	)
	make install
	rm -f /usr/lib/librdkafka.a /usr/lib/librdkafka++.a
	write_stamp "$stamp"
}

build_kafkacat() {
	local stamp="kafkacat"
	local dir="$CACHE_DIR/kafkacat"
	if stamp_exists "$stamp"; then
		log "kafkacat already installed"
		return
	fi
	log "Building kafkacat"
	clone_clean https://github.com/edenhill/kafkacat "$dir"
	cd "$dir"
	(
		unset CFLAGS
		./configure --prefix=/usr --CFLAGS="-g"
		make -j"$(nproc)"
	)
	make install
	write_stamp "$stamp"
}

install_python_packages() {
	log "Installing Python helper packages"
	python3 -m pip install pyasn1 pysnmp --break-system-packages --upgrade --no-cache-dir
}

prepare_service_dirs() {
	log "Preparing service runtime directories"
	mkdir -p /var/run/mysqld
	chown mysql:mysql /var/run/mysqld
	chmod o+rx /var/run/mysqld
	mkdir -p /var/log/mysql
	: > /var/log/mysql/error.log
}

extract_configure_options() {
	awk '
		/^ENV[[:space:]]+RSYSLOG_CONFIGURE_OPTIONS="/ {
			in_opts=1
			sub(/^ENV[[:space:]]+RSYSLOG_CONFIGURE_OPTIONS="/, "")
		}
		in_opts {
			line=$0
			sub(/[[:space:]]*\\$/, "", line)
			sub(/[[:space:]]*"$/, "", line)
			gsub(/^[[:space:]]+|[[:space:]]+$/, "", line)
			if (line != "") {
				printf "%s ", line
			}
			if ($0 ~ /"$/) {
				exit
			}
		}
	' "$DOCKERFILE"
}

verify_build() {
	log "Verifying rsyslog build with Ubuntu 24.04 container configure options"
	local configure_options
	configure_options="$(extract_configure_options)"
	printf 'Using RSYSLOG_CONFIGURE_OPTIONS:\n%s\n' "$configure_options"
	run_as_build_user env \
		PKG_CONFIG_PATH=/usr/local/lib/pkgconfig \
		LD_LIBRARY_PATH=/usr/local/lib \
		C_INCLUDE_PATH=/usr/include/tirpc/ \
		MYSQLD_START_CMD="sudo mysqld_safe --pid-file=/var/run/mysqld/mysqld.pid" \
		MYSQLD_STOP_CMD="sudo kill \$(sudo cat /var/run/mysqld/mysqld.pid)" \
		ASAN_SYMBOLIZER_PATH=/usr/lib/llvm-18/bin/llvm-symbolizer \
		RSYSLOG_CONFIGURE_OPTIONS="$configure_options" \
		bash -c '
			set -euo pipefail
			cd "$1"
			autoreconf -fvi
			./configure $RSYSLOG_CONFIGURE_OPTIONS
			make -j"$(nproc)"
		' _ "$REPO_ROOT"
}

main() {
	require_ubuntu_2404
	refuse_windows_mount_checkout
	configure_git_line_endings
	rewrite_apt_mirror "$APT_MIRROR"
	apt_update
	install_apt_packages
	install_obs_packages
	prepare_build_cache
	build_codestyle
	build_libfastjson
	build_faup
	build_libksi
	build_librelp
	build_qpid_proton
	build_librdkafka
	build_kafkacat
	install_python_packages
	prepare_service_dirs
	if [ "$VERIFY_BUILD" -eq 1 ]; then
		verify_build
	fi
	log "rsyslog Ubuntu 24.04 WSL development setup complete"
}

main "$@"
