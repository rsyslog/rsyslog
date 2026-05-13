# Local Container Testing

This file captures the local validation flow used for compatibility-format work.
It mirrors the GitHub Actions container path while avoiding external services
that are usually unnecessary for parser/config changes.

## Ubuntu 26.04 CI-style Build

Start from a clean build tree before switching into the container:

```sh
make distclean || true
rm -f tests/runtime_unit_linkedlist tests/runtime_unit_stringbuf
```

Run the Ubuntu 26.04 dev container with Elasticsearch, Kafka, and MySQL disabled.
Use `-j60` for local check runs on this host.

```sh
RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
CC='clang-21' \
CFLAGS='-g' \
RSYSLOG_CONFIGURE_OPTIONS_OVERRIDE='--enable-debug --enable-testbench --enable-imdiag --enable-omstdout --disable-elasticsearch --disable-elasticsearch-tests --disable-imkafka --disable-omkafka --disable-kafka-tests --disable-mysql --disable-mysql-tests' \
CI_MAKE_OPT='-j60' \
CI_MAKE_CHECK_OPT='-j60 TESTS=' \
CI_CHECK_CMD='check' \
devtools/devcontainer.sh --rm devtools/run-ci.sh
```

Run focused testbench scripts after the container build:

```sh
RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
devtools/devcontainer.sh --rm ./tests/compat-configformat-rainerscript.sh

RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
devtools/devcontainer.sh --rm ./tests/compat-configformat-yaml.sh
```

## Service Test Relevance Skips

Heavy service-backed tests self-skip inside the testbench when the local git
diff does not touch their relevant source paths. This applies to both
`devtools/run-ci.sh` and direct test scripts launched through
`devtools/devcontainer.sh`.

The initial gated services are:

- Elasticsearch: `plugins/omelasticsearch/**` and `runtime/*.[ch]`
- MySQL: `plugins/ommysql/**` and `runtime/*.[ch]`
- libdbi: `plugins/omlibdbi/**` and `runtime/*.[ch]`
- Kafka: `plugins/imkafka/**`, `plugins/omkafka/**`, and `runtime/*.[ch]`

Local detection uses the merge base with `origin/main` plus staged, unstaged,
and untracked files. If the git state cannot be resolved, the testbench runs
the service tests instead of skipping them.

Force service tests when intentionally validating them without relevant source
changes:

```sh
RSYSLOG_TESTBENCH_FORCE_SERVICE_TESTS=1 \
RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
devtools/devcontainer.sh --rm ./tests/es-basic.sh

RSYSLOG_TESTBENCH_FORCE_KAFKA_TESTS=1 \
RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
devtools/devcontainer.sh --rm ./tests/kafka-selftest.sh
```

When validating service-skip behavior, use `-j60` and record wall-clock runtime:

```sh
/usr/bin/time -p env \
RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
CI_MAKE_OPT='-j60' \
CI_MAKE_CHECK_OPT='-j60' \
CI_CHECK_CMD='check' \
devtools/devcontainer.sh --rm devtools/run-ci.sh
```

For broader local check runs, prefer:

```sh
make -j60 check
```

Clear obvious flakes locally, but do not treat unrelated flaky failures as part
of the change unless they reproduce against the focused tests.

## Clang Static Analyzer

Clean before switching from a normal container build to analyzer mode so
scan-build recompiles the tree:

```sh
make distclean || true
rm -rf scan-build-report clang-analyzer.log
```

Run the analyzer with the same Ubuntu 26.04 container and service exclusions:

```sh
RSYSLOG_DEV_CONTAINER='rsyslog/rsyslog_dev_base_ubuntu:26.04' \
SCAN_BUILD='scan-build' \
SCAN_BUILD_CC='clang' \
SCAN_BUILD_REPORT_DIR='scan-build-report' \
DOCKER_RUN_EXTRA_OPTS='-e SCAN_BUILD -e SCAN_BUILD_CC -e SCAN_BUILD_REPORT_DIR' \
RSYSLOG_CONFIGURE_OPTIONS_EXTRA='--disable-elasticsearch --disable-elasticsearch-tests --disable-imkafka --disable-omkafka --disable-kafka-tests --disable-mysql --disable-mysql-tests' \
devtools/devcontainer.sh --rm devtools/run-static-analyzer.sh 2>&1 | tee clang-analyzer.log
```

## Docker Storage

If Docker storage runs short, prune from least intrusive to most intrusive:

```sh
docker container prune
docker image prune
docker builder prune
docker system prune
docker system prune -a
```

Use `docker system prune -a` only when the cheaper prunes do not recover enough
space and there are no running containers that must be preserved.
