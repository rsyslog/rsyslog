# Rocky Linux 9 daily RPM package-build base image

Image name/tag (successor to `rsyslog/rsyslog_dev_pkg_base_fedora:36`):

```text
rsyslog/rsyslog_dev_pkg_base_rocky:9
```

Why: Fedora 36 is EOL and its mock-core-configs lacks EL10 templates
(`centos-stream-10.tpl`, `epel-10.tpl`, RHEL 10 configs). Rocky 9 + EPEL
(mock-core-configs 44.4+) ships a current mock stack that can build
epel-8/9/10 and rhel-8/9/10.

## Build and publish

On a host with Docker and registry push rights:

```bash
cd packaging/docker/dev_env/rocky/pkg_base/9
./build.sh
docker push rsyslog/rsyslog_dev_pkg_base_rocky:9
```

Or from `packaging/docker/dev_env` (Makefile target names are prefixed with
`.-` by the existing path mangling; prefer `./build.sh`):

```bash
make '.-rocky-pkg_base-9-'
```

## Verify EL10 templates

```bash
docker run --rm rsyslog/rsyslog_dev_pkg_base_rocky:9 \
  ls /etc/mock/templates/centos-stream-10.tpl \
     /etc/mock/templates/epel-10.tpl \
     /etc/mock/templates/rhel-10.tpl \
     /etc/mock/centos-stream+epel-8-x86_64.cfg \
     /etc/mock/centos-stream+epel-9-x86_64.cfg \
     /etc/mock/epel-10-x86_64.cfg

docker run --rm --privileged rsyslog/rsyslog_dev_pkg_base_rocky:9 \
  mock -r epel-10-x86_64 --quiet --shell true
```

## Daily host cut-over (`run_daily_pkg_build.sh`)

Replace the container image reference

```text
rsyslog/rsyslog_dev_pkg_base_fedora:36
```

with

```text
rsyslog/rsyslog_dev_pkg_base_rocky:9
```

Keep the same privileged run flags and volume mounts used today for
`/private-files`, yumrepo, and `/home/pkg/scripts/pkg_build_docker_script.sh`.
Interactive entry helpers: `./run.sh` and `./run-noadm.sh` (require
`private/private-env.sh` with `PKGPRIVATEBASEDIR` and repo credentials).

Private files expected under `private/mount` (not committed):
`passfile.txt`, `.gnupg/`, `.ssh/`, and optional `rhel/` entitlement trees.
