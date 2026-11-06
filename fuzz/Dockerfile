FROM alpine:3.23

ARG CASR_VERSION=2.13.0

ENV AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
    AFL_MAP_SIZE=2097152 \
    AFL_NO_UI=1 \
    AFL_SKIP_CPUFREQ=1 \
    ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:symbolize=1 \
    CARGO_HOME=/tmp/cargo \
    PATH=/usr/local/bin:${PATH} \
    UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1

RUN set -eux; \
    apk add --no-cache \
        afl++ \
        autoconf \
        autoconf-archive \
        automake \
        bash \
        bison \
        build-base \
        ca-certificates \
        cargo \
        clang \
        compiler-rt \
        coreutils \
        curl \
        file \
        findutils \
        flex \
        gdb \
        git \
        libestr-dev \
        libfastjson-dev \
        libtool \
        linux-headers \
        make \
        openssl-dev \
        patch \
        perl \
        pkgconf \
        python3 \
        rust \
        tar \
        util-linux-dev \
        xz \
        zlib-dev; \
    update-ca-certificates; \
    printf '%s\n' \
        '#!/bin/sh' \
        '. /etc/os-release' \
        'case "${1:-}" in' \
        '  -i|--id) printf "Distributor ID:\t%s\n" "${ID:-alpine}" ;;' \
        '  -d|--description) printf "Description:\t%s\n" "${PRETTY_NAME:-Alpine Linux}" ;;' \
        '  -r|--release) printf "Release:\t%s\n" "${VERSION_ID:-unknown}" ;;' \
        '  -c|--codename) printf "Codename:\tn/a\n" ;;' \
        '  -a|--all|"")' \
        '    printf "Distributor ID:\t%s\n" "${ID:-alpine}"' \
        '    printf "Description:\t%s\n" "${PRETTY_NAME:-Alpine Linux}"' \
        '    printf "Release:\t%s\n" "${VERSION_ID:-unknown}"' \
        '    printf "Codename:\tn/a\n"' \
        '    ;;' \
        '  *) printf "unsupported lsb_release option: %s\n" "$1" >&2; exit 2 ;;' \
        'esac' \
        > /usr/local/bin/lsb_release; \
    chmod 755 /usr/local/bin/lsb_release; \
    cargo install --locked --root /usr/local --version "${CASR_VERSION}" casr; \
    command -v afl-clang-fast; \
    command -v afl-fuzz; \
    command -v casr-afl; \
    rm -rf "${CARGO_HOME}" /root/.cargo

WORKDIR /src

CMD ["/bin/sh"]
