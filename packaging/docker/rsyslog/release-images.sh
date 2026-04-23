#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
	cat <<'EOF'
Usage:
  ./release-images.sh <check|build|push|publish> [options]

Release channel options:
  --channel <stable|daily-stable>  Select the package source channel.
  --daily                          Shortcut for --channel daily-stable.
  --stable                         Shortcut for --channel stable.

Stable release options:
  --rsyslog-version <8.yymm.0>     Required for the stable channel.

Additional options:
  --latest                         With publish, also update :latest tags.
  --rebuild                        Force docker builds to bypass cache.
  --ubuntu-version <version>       Override the readiness-check Ubuntu base.
  -h, --help                       Show this help text.

Examples:
  ./release-images.sh check --rsyslog-version 8.2604.0
  ./release-images.sh build --rsyslog-version 8.2604.0
  ./release-images.sh publish --rsyslog-version 8.2604.0 --latest
  ./release-images.sh build --daily
EOF
}

die() {
	echo "ERROR: $*" >&2
	exit 1
}

action=""
channel="stable"
rsyslog_version=""
push_latest="no"
rebuild="no"
release_ubuntu_version=""

while (($# > 0)); do
	case "$1" in
		check|build|push|publish)
			[[ -z "$action" ]] || die "only one command may be provided"
			action="$1"
			shift
			;;
		--channel)
			[[ $# -ge 2 ]] || die "--channel requires a value"
			channel="$2"
			shift 2
			;;
		--daily)
			channel="daily-stable"
			shift
			;;
		--stable)
			channel="stable"
			shift
			;;
		--rsyslog-version)
			[[ $# -ge 2 ]] || die "--rsyslog-version requires a value"
			rsyslog_version="$2"
			shift 2
			;;
		--latest)
			push_latest="yes"
			shift
			;;
		--rebuild)
			rebuild="yes"
			shift
			;;
		--ubuntu-version)
			[[ $# -ge 2 ]] || die "--ubuntu-version requires a value"
			release_ubuntu_version="$2"
			shift 2
			;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			die "unknown argument: $1"
			;;
	esac
done

[[ -n "$action" ]] || die "missing command (expected one of: check, build, push, publish)"

case "$channel" in
	stable|daily-stable) ;;
	*) die "invalid channel: $channel (expected stable or daily-stable)" ;;
esac

if [[ "$channel" == "stable" && -z "$rsyslog_version" ]]; then
	die "--rsyslog-version is required for the stable channel"
fi

if [[ "$channel" == "daily-stable" && -n "$rsyslog_version" ]]; then
	die "--rsyslog-version is only valid for the stable channel"
fi

if [[ "$action" != "publish" && "$push_latest" == "yes" ]]; then
	die "--latest is only valid with the publish command"
fi

case "$action" in
	check) make_target="check_ppa_release_ready" ;;
	build) make_target="release_build" ;;
	push) make_target="release_push" ;;
	publish) make_target="release_publish" ;;
	*) die "unsupported command: $action" ;;
esac

make_args=(
	-C "$script_dir"
	"$make_target"
	"RELEASE_CHANNEL=$channel"
	"PPA_CHANNEL=$channel"
	"REBUILD=$rebuild"
)

if [[ -n "$rsyslog_version" ]]; then
	make_args+=("RSYSLOG_VERSION=$rsyslog_version")
fi

if [[ -n "$release_ubuntu_version" ]]; then
	make_args+=("RELEASE_UBUNTU_VERSION=$release_ubuntu_version")
fi

if [[ "$push_latest" == "yes" ]]; then
	make_args+=("PUSH_LATEST=yes")
fi

exec make "${make_args[@]}"
