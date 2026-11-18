#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Others
#
# This file is part of the rsyslog project, released under ASL 2.0.
# Verify the ssh backend of the daily-stable archive store end to end,
# with the server script called directly instead of over ssh:
#
# - fetch of a missing object succeeds and creates no file
# - put-immutable stores once, accepts an identical retry and rejects
#   different content at the same path
# - put-metadata replaces content atomically
# - path traversal, symlinks, bad checksums, metadata writes over immutable
#   files and unknown requests are refused
set -eu

test_dir=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)
repo_dir="$test_dir/.."
client="$repo_dir/devtools/release/archive-store.sh"
server="$repo_dir/devtools/release/archive-store-server.sh"
work_dir="$(pwd)/daily-stable-archive-store-work.$$"

command -v sha256sum >/dev/null 2>&1 || exit 77

cleanup() {
	rm -rf -- "$work_dir"
}
trap cleanup EXIT HUP INT TERM
mkdir -p "$work_dir/root"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

cat >"$work_dir/fake-ssh" <<EOF
#!/bin/bash
SSH_ORIGINAL_COMMAND="\$1" exec "$server" "$work_dir/root"
EOF
chmod +x "$work_dir/fake-ssh"

export ARCHIVE_BACKEND=ssh
export ARCHIVE_PREFIX=apt/daily-stable/debian/13
export ARCHIVE_SSH_COMMAND="$work_dir/fake-ssh"
stored="$work_dir/root/$ARCHIVE_PREFIX"

printf 'one\n' >"$work_dir/a"
printf 'two\n' >"$work_dir/b"

"$client" fetch dists/trixie/InRelease "$work_dir/missing" ||
	fail "fetch of a missing object failed"
[ ! -e "$work_dir/missing" ] || fail "fetch of a missing object created a file"

"$client" put-immutable "$work_dir/a" pool/main/r/x.deb ||
	fail "first immutable upload failed"
"$client" put-immutable "$work_dir/a" pool/main/r/x.deb ||
	fail "identical immutable retry failed"
if "$client" put-immutable "$work_dir/b" pool/main/r/x.deb 2>/dev/null; then
	fail "immutable collision was accepted"
fi
cmp -s "$work_dir/a" "$stored/pool/main/r/x.deb" ||
	fail "immutable object was changed"

"$client" put-metadata "$work_dir/a" dists/trixie/InRelease ||
	fail "metadata upload failed"
"$client" put-metadata "$work_dir/b" dists/trixie/InRelease ||
	fail "metadata replacement failed"
"$client" fetch dists/trixie/InRelease "$work_dir/fetched" ||
	fail "metadata fetch failed"
cmp -s "$work_dir/b" "$work_dir/fetched" || fail "fetched metadata differs"
if find "$stored" -name '.upload.*' | grep -q .; then
	fail "temporary upload files were left behind"
fi

if "$client" fetch ../../etc/passwd "$work_dir/x" 2>/dev/null; then
	fail "client accepted path traversal"
fi
zero=0000000000000000000000000000000000000000000000000000000000000000
mkdir -p "$work_dir/outside"
ln -s "$work_dir/outside" "$work_dir/root/link"
for request in "get ../../etc/passwd" "get /etc/passwd" "rm -rf /" \
	"get a/./b" "put-metadata x/. $zero" "get link/f" \
	"put-metadata link/f $(sha256sum <"$work_dir/a" | awk '{print $1}')" \
	"put-metadata $ARCHIVE_PREFIX/pool/main/r/x.deb $(sha256sum <"$work_dir/a" | awk '{print $1}')" \
	"put-metadata $ARCHIVE_PREFIX/pool $zero" \
	"put-metadata x/y $zero" "put-immutable x/z $zero"; do
	if SSH_ORIGINAL_COMMAND="$request" "$server" "$work_dir/root" \
		<"$work_dir/a" >/dev/null 2>&1; then
		fail "server accepted: $request"
	fi
done
[ ! -e "$work_dir/root/x/y" ] || fail "upload with a bad checksum was stored"
[ ! -e "$work_dir/root/x/z" ] || fail "immutable upload with a bad checksum was stored"
[ -z "$(ls -A "$work_dir/outside")" ] || fail "write escaped through a symlink"
cmp -s "$work_dir/a" "$stored/pool/main/r/x.deb" ||
	fail "metadata request replaced an immutable file"
if find "$work_dir/root" -name '.upload.*' | grep -q .; then
	fail "rejected uploads left temporary files behind"
fi

echo "daily-stable archive store checks passed"
