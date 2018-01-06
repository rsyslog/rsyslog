# Use an official Python runtime as a parent image
FROM centos:7

# Set the working directory to /app
WORKDIR /home/ci

# Copy the current directory contents into the container at /app
ADD . /home/ci

# Define environment variable
ENV SUDO=
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig \
    LD_LIBRARY_PATH=/usr/local/lib

# Install any needed packages
RUN yum -y upgrade && \
    yum -y install epel-release
RUN yum -y install clang git valgrind libtool autoconf automake flex bison python-docutils python-sphinx json-c-devel libuuid-devel libgcrypt-devel zlib-devel openssl-devel libcurl-devel gnutls-devel mysql-devel postgresql-devel libdbi-dbd-mysql libdbi-devel net-snmp-devel systemd-devel hiredis-devel qpid-proton-c-devel redhat-rpm-config python-devel elasticsearch libfaketime 

# Make port 80 available to the world outside this container
#EXPOSE 80


# Run app.py when the container launches
#CMD ["python", "app.py"]
