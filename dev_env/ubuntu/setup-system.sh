export DEBIAN_FRONTEND=noninteractive
apt-get update -q -y
apt-get install -y software-properties-common python-software-properties
add-apt-repository ppa:qpid/released -y
apt-get update -q -y
apt-get upgrade -y
apt-get install -q -y \
     mysql-server \
     pkg-config \
     libtool \
     autoconf \
     autoconf-archive \
     autotools-dev \
     automake \
     python-docutils \
     pkg-config \
     libtool \
     gdb \
     valgrind \
     libdbi-dev \
     libczmq-dev \
     libsnmp-dev \
     libmysqlclient-dev \
     libglib2.0-dev \
     libtokyocabinet-dev \
     zlib1g-dev \
     uuid-dev \
     libgcrypt11-dev \
     bison \
     flex \
     clang \
     libcurl4-gnutls-dev \
     python-docutils  \
     wget \
     libgnutls28-dev \
     libsystemd-dev \
     libhiredis-dev \
     librdkafka-dev  \
     libnet1-dev \
     libgrok1 libgrok-dev \
     libmongoc-dev \
     libbson-dev \
     git \
     faketime libdbd-mysql autoconf-archive

#     postgresql-client postgresql-server-dev-9.5 
