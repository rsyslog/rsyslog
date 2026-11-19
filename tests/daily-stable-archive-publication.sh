#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Others
#
# This file is part of the rsyslog project, released under ASL 2.0.
# Verify the pull-model publication path of the daily-stable archive.
#
# archive-publication.sh (signed publication bundle, format v1):
# - init/add write the exact manifest header and one line per file, in
#   the order added, and copy each file below files/<prefix>/
# - add rejects bad paths (.., ., //, absolute, trailing /, bad
#   characters), duplicates, immutable entries after metadata and any
#   entry after InRelease
# - sign + verify succeed with the matching allowed-signers entry; verify
#   fails for a tampered file, a tampered manifest, an extra file and a
#   signature by another key, a duplicate path, a missing InRelease, or an
#   InRelease published as an immutable object
#
# debian-daily-stable.sh fetch-verified-indexes (served by a local HTTP
# server so that HTTP status codes are real):
# - a good signed InRelease and matching indexes are copied into place
# - a Packages.xz that does not match the signed SHA-256/size fails
# - an InRelease signed by another key, altered after signing, or missing
#   a required index fails
# - HTTP 404 for InRelease starts from empty indexes and succeeds
# - HTTP 500 for InRelease fails (no fallback to empty indexes)
# The oracle is the exit status of each command plus the files it leaves.
# The loopback server writes its port after listening. Poll that file and
# allow up to 60 seconds for Python startup on loaded runners.
set -eu

test_dir=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)
srcdir="$test_dir" bash "$test_dir/diag.sh" solaris-subset-skip "${0##*/}"
repo_dir="$test_dir/.."
publication="$repo_dir/devtools/release/archive-publication.sh"
helper="$repo_dir/devtools/release/debian-daily-stable.sh"
work_dir="$(pwd)/daily-stable-archive-publication-work.$$"

for tool in sha256sum ssh-keygen gpg gpgconf gpgv curl python3 awk; do
	command -v "$tool" >/dev/null 2>&1 || exit 77
done

server_pid=""
gnupg_dir=""
cleanup() {
	if [ -n "$server_pid" ]; then
		kill "$server_pid" 2>/dev/null || true
		wait "$server_pid" 2>/dev/null || true
	fi
	if [ -n "$gnupg_dir" ]; then
		GNUPGHOME="$gnupg_dir/good" gpgconf --kill all >/dev/null 2>&1 || true
		GNUPGHOME="$gnupg_dir/other" gpgconf --kill all >/dev/null 2>&1 || true
		rm -rf -- "$gnupg_dir"
	fi
	rm -rf -- "$work_dir"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
mkdir -p "$work_dir"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

prefix=apt/daily-stable/debian/13
bundle="$work_dir/bundle"
printf 'pool\n' >"$work_dir/pool.deb"
printf 'index\n' >"$work_dir/Packages.xz"
printf 'release\n' >"$work_dir/Release"
printf 'inrelease\n' >"$work_dir/InRelease"

# ---- bundle format and add rules -----------------------------------------

"$publication" init "$bundle" "$prefix" 123456 2 || fail "init failed"
"$publication" add immutable "$bundle" "$work_dir/pool.deb" \
	pool/main/r/rsyslog/x.deb || fail "add immutable failed"
"$publication" add immutable "$bundle" "$work_dir/Packages.xz" \
	dists/trixie/main/binary-amd64/by-hash/SHA256/abc || fail "add by-hash failed"
# shellcheck disable=SC2016 # 'a$b' is a literal bad path
for bad in ../x a/../b a/./b ./a a//b /abs a/ 'a b' 'a;b' 'a$b' ''; do
	if "$publication" add immutable "$bundle" "$work_dir/pool.deb" "$bad" \
		2>/dev/null; then
		fail "add accepted bad path '$bad'"
	fi
done
if "$publication" add immutable "$bundle" "$work_dir/Release" \
	pool/main/r/rsyslog/x.deb 2>/dev/null; then
	fail "add accepted a duplicate path"
fi
"$publication" add metadata "$bundle" "$work_dir/Packages.xz" \
	dists/trixie/main/binary-amd64/Packages.xz || fail "add metadata failed"
if "$publication" add immutable "$bundle" "$work_dir/pool.deb" \
	pool/main/r/rsyslog/y.deb 2>/dev/null; then
	fail "add accepted immutable after metadata"
fi
"$publication" add metadata "$bundle" "$work_dir/Release" \
	dists/trixie/Release || fail "add Release failed"
"$publication" add metadata "$bundle" "$work_dir/InRelease" \
	dists/trixie/InRelease || fail "add InRelease failed"
if "$publication" add metadata "$bundle" "$work_dir/Release" \
	dists/trixie/Release.gpg 2>/dev/null; then
	fail "add accepted an entry after InRelease"
fi
if "$publication" init "$bundle" "$prefix" 1 1 2>/dev/null; then
	fail "init reused a non-empty bundle directory"
fi
for bad_prefix in ../apt /apt apt//x; do
	if "$publication" init "$work_dir/p" "$bad_prefix" 1 1 2>/dev/null; then
		fail "init accepted bad prefix '$bad_prefix'"
	fi
	rm -rf "$work_dir/p"
done

sha() { sha256sum "$1" | awk '{print $1}'; }
{
	printf 'rsyslog-archive-publication v1\n'
	printf 'prefix %s\n' "$prefix"
	printf 'run 123456 2\n'
	printf 'immutable %s 5 %s/pool/main/r/rsyslog/x.deb\n' \
		"$(sha "$work_dir/pool.deb")" "$prefix"
	printf 'immutable %s 6 %s/dists/trixie/main/binary-amd64/by-hash/SHA256/abc\n' \
		"$(sha "$work_dir/Packages.xz")" "$prefix"
	printf 'metadata %s 6 %s/dists/trixie/main/binary-amd64/Packages.xz\n' \
		"$(sha "$work_dir/Packages.xz")" "$prefix"
	printf 'metadata %s 8 %s/dists/trixie/Release\n' \
		"$(sha "$work_dir/Release")" "$prefix"
	printf 'metadata %s 10 %s/dists/trixie/InRelease\n' \
		"$(sha "$work_dir/InRelease")" "$prefix"
} >"$work_dir/expected-manifest"
grep -Eq '^created [0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$' \
	"$bundle/manifest" || fail "manifest has no valid created line"
[ "$(sed -n 4p "$bundle/manifest" | cut -d' ' -f1)" = created ] ||
	fail "created is not manifest line 4"
grep -v '^created ' "$bundle/manifest" | cmp -s - "$work_dir/expected-manifest" ||
	fail "manifest differs from the expected v1 layout"
cmp -s "$work_dir/pool.deb" "$bundle/files/$prefix/pool/main/r/rsyslog/x.deb" ||
	fail "bundle file content differs"
[ "$(find "$bundle/files" -type f | wc -l)" -eq 5 ] ||
	fail "bundle holds an unexpected number of files"

# ---- sign and verify -----------------------------------------------------

ssh-keygen -q -t ed25519 -N '' -C test -f "$work_dir/key" ||
	fail "ssh-keygen could not create a test key"
ssh-keygen -q -t ed25519 -N '' -C other -f "$work_dir/other-key"
printf 'rsyslog-archive-publication namespaces="rsyslog-archive-publication" %s\n' \
	"$(cut -d' ' -f1,2 "$work_dir/key.pub")" >"$work_dir/allowed"
printf 'rsyslog-archive-publication namespaces="rsyslog-archive-publication" %s\n' \
	"$(cut -d' ' -f1,2 "$work_dir/other-key.pub")" >"$work_dir/allowed-other"

"$publication" sign "$bundle" "$work_dir/key" || fail "sign failed"
"$publication" verify "$bundle" "$work_dir/allowed" ||
	fail "verify rejected a good bundle"
if "$publication" verify "$bundle" "$work_dir/allowed-other" 2>/dev/null; then
	fail "verify accepted a signature by another key"
fi
if "$publication" add metadata "$bundle" "$work_dir/Release" x 2>/dev/null; then
	fail "add modified a signed bundle"
fi

tamper_copy() {
	rm -rf "$work_dir/t"
	cp -a "$bundle" "$work_dir/t"
}
tamper_copy
printf 'evil\n' >"$work_dir/t/files/$prefix/pool/main/r/rsyslog/x.deb"
if "$publication" verify "$work_dir/t" "$work_dir/allowed" 2>/dev/null; then
	fail "verify accepted a tampered file"
fi
tamper_copy
sed 's/^run 123456 2$/run 123456 3/' "$bundle/manifest" >"$work_dir/t/manifest"
if "$publication" verify "$work_dir/t" "$work_dir/allowed" 2>/dev/null; then
	fail "verify accepted a tampered manifest"
fi
tamper_copy
printf 'extra\n' >"$work_dir/t/files/$prefix/extra"
if "$publication" verify "$work_dir/t" "$work_dir/allowed" 2>/dev/null; then
	fail "verify accepted a file not listed in the manifest"
fi
tamper_copy
rm "$work_dir/t/manifest.sig"
if "$publication" verify "$work_dir/t" "$work_dir/allowed" 2>/dev/null; then
	fail "verify accepted an unsigned bundle"
fi
# verify enforces prefix and order on its own, even for a validly signed
# manifest that "add" would never have written.
pool_sha="$(sha "$work_dir/pool.deb")"
tamper_copy
mkdir -p "$work_dir/t/files/other"
cp "$work_dir/pool.deb" "$work_dir/t/files/other/x.deb"
awk -v entry="immutable $pool_sha 5 other/x.deb" \
	'{ print } NR == 4 { print entry }' "$bundle/manifest" >"$work_dir/t/manifest"
"$publication" sign "$work_dir/t" "$work_dir/key"
if "$publication" verify "$work_dir/t" "$work_dir/allowed" 2>/dev/null; then
	fail "verify accepted a path outside the prefix"
fi
tamper_copy
cp "$work_dir/pool.deb" "$work_dir/t/files/$prefix/pool/main/r/rsyslog/late.deb"
awk -v entry="immutable $pool_sha 5 $prefix/pool/main/r/rsyslog/late.deb" \
	'$4 ~ /\/InRelease$/ { print entry } { print }' "$bundle/manifest" >"$work_dir/t/manifest"
"$publication" sign "$work_dir/t" "$work_dir/key"
if "$publication" verify "$work_dir/t" "$work_dir/allowed" 2>/dev/null; then
	fail "verify accepted an immutable entry after metadata"
fi

tamper_copy
awk '{ print } NR == 5 { print }' "$bundle/manifest" >"$work_dir/t/manifest"
"$publication" sign "$work_dir/t" "$work_dir/key"
if "$publication" verify "$work_dir/t" "$work_dir/allowed" 2>/dev/null; then
	fail "verify accepted a duplicate path"
fi
tamper_copy
sed '$d' "$bundle/manifest" >"$work_dir/t/manifest"
rm "$work_dir/t/files/$prefix/dists/trixie/InRelease"
"$publication" sign "$work_dir/t" "$work_dir/key"
if "$publication" verify "$work_dir/t" "$work_dir/allowed" 2>/dev/null; then
	fail "verify accepted a bundle missing its terminal InRelease"
fi
tamper_copy
sed 's/^metadata \(.*\/InRelease\)$/immutable \1/' "$bundle/manifest" >"$work_dir/t/manifest"
"$publication" sign "$work_dir/t" "$work_dir/key"
if "$publication" verify "$work_dir/t" "$work_dir/allowed" 2>/dev/null; then
	fail "verify accepted InRelease as an immutable entry"
fi

# ---- fetch-verified-indexes ---------------------------------------------

# gpg-agent sockets live in GNUPGHOME; keep its path short.
gnupg_dir="$(mktemp -d)"
export GNUPGHOME="$gnupg_dir/good"
other_home="$gnupg_dir/other"
install -m 700 -d "$GNUPGHOME" "$other_home"
gpg --batch --pinentry-mode loopback --passphrase '' --quick-generate-key \
	"archive test <test@example.invalid>" ed25519 sign 0 >/dev/null 2>&1 ||
	fail "could not create a GPG test key"
GNUPGHOME="$other_home" gpg --batch --pinentry-mode loopback --passphrase '' \
	--quick-generate-key "other <other@example.invalid>" ed25519 sign 0 \
	>/dev/null 2>&1
fpr="$(gpg --batch --list-secret-keys --with-colons |
	awk -F: '$1 == "fpr" {print $10; exit}')"

www="$work_dir/www"
make_repo() {
	# make_repo <name> <signing-gnupg-home>: a tiny trixie archive whose
	# InRelease lists amd64 Packages.xz and Sources.xz.
	local root="$www/$1/dists/trixie"
	mkdir -p "$root/main/binary-amd64" "$root/main/source"
	printf 'packages-%s\n' "$1" >"$root/main/binary-amd64/Packages.xz"
	printf 'sources-%s\n' "$1" >"$root/main/source/Sources.xz"
	{
		printf 'Origin: test\nSuite: trixie\nCodename: trixie\nSHA256:\n'
		for f in main/binary-amd64/Packages.xz main/source/Sources.xz; do
			printf ' %s %16s %s\n' "$(sha "$root/$f")" \
				"$(wc -c <"$root/$f" | tr -d ' ')" "$f"
		done
	} >"$root/Release"
	GNUPGHOME="$2" gpg --batch --yes --clearsign \
		--output "$root/InRelease" "$root/Release"
}
make_repo good "$GNUPGHOME"
make_repo tampered "$GNUPGHOME"
printf 'evil\n' >"$www/tampered/dists/trixie/main/binary-amd64/Packages.xz"
make_repo foreign "$other_home"
make_repo altered "$GNUPGHOME"
sed 's/^Origin: test$/Origin: evil/' "$www/altered/dists/trixie/InRelease" >"$work_dir/altered-release"
mv "$work_dir/altered-release" "$www/altered/dists/trixie/InRelease"
make_repo missing-index "$GNUPGHOME"
sed '/ main\/source\/Sources.xz$/d' "$www/missing-index/dists/trixie/Release" >"$work_dir/missing-release"
gpg --batch --yes --clearsign --output "$www/missing-index/dists/trixie/InRelease" "$work_dir/missing-release"
mkdir -p "$www/empty"

# Serve $www; any path below /error/ answers HTTP 500.
cat >"$work_dir/server.py" <<'EOF'
import functools
import http.server
import os
import socketserver
import sys


class Handler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith("/error/"):
            self.send_error(500)
            return
        super().do_GET()

    def log_message(self, *args):
        pass


handler = functools.partial(Handler, directory=sys.argv[1])
# We only need a loopback socket. HTTPServer resolves a hostname when it
# binds, although the handler serves this fixture without needing one.
server = socketserver.TCPServer(("127.0.0.1", 0), handler)
with open(sys.argv[2] + ".tmp", "w") as stream:
    stream.write(str(server.server_address[1]))
os.rename(sys.argv[2] + ".tmp", sys.argv[2])
server.serve_forever()
EOF
python3 -I "$work_dir/server.py" "$www" "$work_dir/port" >"$work_dir/server.log" 2>&1 &
server_pid=$!
server_deadline=$((SECONDS + 60))
while [ ! -s "$work_dir/port" ]; do
	if ! kill -0 "$server_pid" 2>/dev/null; then
		cat "$work_dir/server.log" >&2
		fail "test HTTP server exited before reporting its port"
	fi
	if [ "$SECONDS" -ge "$server_deadline" ]; then
		cat "$work_dir/server.log" >&2
		fail "test HTTP server did not report its port within 60 seconds"
	fi
	sleep 0.1
done
base="http://127.0.0.1:$(cat "$work_dir/port")"
export ARCHIVE_FETCH_RETRIES=0

fetch() {
	rm -rf "$work_dir/apt-repo"
	"$helper" fetch-verified-indexes "$base/$1" "$work_dir/apt-repo" \
		trixie main "$fpr" amd64
}

fetch good >/dev/null 2>&1 || fail "fetch rejected a good archive"
for f in main/binary-amd64/Packages.xz main/source/Sources.xz; do
	cmp -s "$www/good/dists/trixie/$f" "$work_dir/apt-repo/dists/trixie/$f" ||
		fail "fetch did not copy $f"
done
for name in tampered foreign altered missing-index error; do
	if fetch "$name" >/dev/null 2>&1; then
		fail "fetch accepted the '$name' archive"
	fi
	if find "$work_dir/apt-repo" -type f | grep -q .; then
		fail "fetch of the '$name' archive left index files behind"
	fi
done
fetch empty >/dev/null 2>&1 || fail "fetch failed on an empty prefix (404)"
if find "$work_dir/apt-repo" -type f | grep -q .; then
	fail "fetch of an empty prefix created index files"
fi
[ -d "$work_dir/apt-repo/dists/trixie/main/binary-amd64" ] ||
	fail "fetch of an empty prefix did not create the index directories"

echo "daily-stable archive publication checks passed"
