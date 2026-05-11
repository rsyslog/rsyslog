# imbeats sample container

This directory contains a sample-only Docker scaffold for running rsyslog with
a TLS-enabled `imbeats` input on port `5044/tcp`.

Status: this scaffold is not wired into `packaging/docker/rsyslog/Makefile`,
release automation, Docker Hub metadata, or `latest` tags yet. Build it
directly from this directory for local experiments only.

## Build

```sh
docker build \
  --build-arg BASE_IMAGE_TAG=rsyslog/rsyslog:latest \
  -t rsyslog-imbeats-sample .
```

The Dockerfile installs `rsyslog-gnutls` as a concrete TLS package example.
The package or package source that provides `imbeats.so` must be available in
the selected base image or configured apt sources; this sample does not publish
or pin that package. The image returns to the base image's `syslog:adm`
runtime user after package installation.

## Run

```sh
docker run --rm -p 5044:5044/tcp \
  -e TLS_CA_FILE=/run/certs/ca.pem \
  -e TLS_CERT_FILE=/run/certs/server-cert.pem \
  -e TLS_KEY_FILE=/run/certs/server-key.pem \
  -v "$PWD/certs:/run/certs:ro" \
  rsyslog-imbeats-sample
```

Environment variables:

- `IMBEATS_PORT`: input port, defaults to `5044`.
- `TLS_CA_FILE`: CA file path passed to the imbeats TLS stream driver.
- `TLS_CERT_FILE`: server certificate path.
- `TLS_KEY_FILE`: server key path.
- `TLS_AUTH_MODE`: stream driver auth mode, defaults to `anon`.
- `IMBEATS_OUTPUT_FILE`: output file, defaults to `/var/log/imbeats.log`.

The default `TLS_AUTH_MODE=anon` matches the common Elastic Agent and Filebeat
setup where the sender verifies the rsyslog server certificate but does not
present a client certificate. Use a stricter certificate-validation auth mode
only after configuring client certificates on the sender.
