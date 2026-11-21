#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Rainer Gerhards and Others
#
# This file is part of the rsyslog project, released under ASL 2.0.
# Verify the pull-model metadata path of the RPM daily-stable archive.
#
# archive-publication.sh with an RPM-shaped bundle:
# - repomd.xml must be followed immediately by its repomd.xml.asc and a
#   repomd.xml.asc must directly follow its repomd.xml (add and verify)
# - the bundle must end with a repomd.xml.asc; a bundle that ends in any
#   other metadata, or in a lone repomd.xml, is rejected
# - a good RPM bundle signs and verifies
#
# rpm-daily-stable.sh fetch-verified-metadata (served by a local HTTP
# server so that HTTP status codes are real; the test hook
# ARCHIVE_FETCH_ALLOW_HTTP=true permits http:// for the loopback server):
# - a good signed repomd.xml and matching data files are copied into place
#   for every requested repository subdirectory
# - a tampered data file (checksum or size mismatch), a tampered
#   repomd.xml, a signature by another key, a missing .asc, a non-sha256
#   checksum, DTD/entity declarations and location hrefs that are not a
#   plain file below repodata/ (traversal, absolute, nested, xml:base)
#   fail and leave no metadata behind
# - HTTP 404 for repomd.xml starts that repository from scratch
# - HTTP 500 for repomd.xml fails (no fallback to an empty repository)
# - HTTP 404 for repomd.xml with publication-status.json present fails,
#   also when only one subdirectory is 404 and the others are 200, unless
#   ARCHIVE_ALLOW_EMPTY_INDEX=true (then it warns and starts from scratch)
# - HTTP 404 for repomd.xml with publication-status.json answering 500 fails
# The oracle is the exit status of each command plus the files it leaves.
# The loopback server writes its port after listening. Poll that file and
# allow up to 60 seconds for Python startup on loaded runners.
set -eu

test_dir=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)
srcdir="$test_dir" bash "$test_dir/diag.sh" solaris-subset-skip "${0##*/}"
repo_dir="$test_dir/.."
publication="$repo_dir/devtools/release/archive-publication.sh"
helper="$repo_dir/devtools/release/rpm-daily-stable.sh"
work_dir="$(pwd)/daily-stable-rpm-metadata-fetch-work.$$"

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

sha() { sha256sum "$1" | awk '{print $1}'; }

# ---- RPM bundle shape ----------------------------------------------------

prefix=rpm/daily-stable/fedora/44
bundle="$work_dir/bundle"
printf 'rpm\n' >"$work_dir/x.rpm"
printf 'primary\n' >"$work_dir/primary"
printf 'repomd-a\n' >"$work_dir/repomd-a.xml"
printf 'asc-a\n' >"$work_dir/repomd-a.xml.asc"
printf 'repomd-b\n' >"$work_dir/repomd-b.xml"
printf 'asc-b\n' >"$work_dir/repomd-b.xml.asc"
printf 'key\n' >"$work_dir/key.asc"

"$publication" init "$bundle" "$prefix" 42 1 || fail "init failed"
"$publication" add immutable "$bundle" "$work_dir/x.rpm" \
	x86_64/Packages/x.rpm || fail "add rpm failed"
"$publication" add immutable "$bundle" "$work_dir/primary" \
	x86_64/repodata/0123-primary.xml.gz || fail "add hashed repodata failed"
"$publication" add metadata "$bundle" "$work_dir/key.asc" \
	rsyslog-archive-keyring.asc || fail "add key failed"
if "$publication" add immutable "$bundle" "$work_dir/repomd-a.xml" \
	x86_64/repodata/repomd.xml 2>/dev/null; then
	fail "add accepted repomd.xml as immutable"
fi
if "$publication" add metadata "$bundle" "$work_dir/repomd-a.xml.asc" \
	x86_64/repodata/repomd.xml.asc 2>/dev/null; then
	fail "add accepted repomd.xml.asc without its repomd.xml"
fi
"$publication" add metadata "$bundle" "$work_dir/repomd-a.xml" \
	x86_64/repodata/repomd.xml || fail "add repomd.xml failed"
if "$publication" add metadata "$bundle" "$work_dir/key.asc" \
	other-metadata 2>/dev/null; then
	fail "add accepted an entry between repomd.xml and its .asc"
fi
if "$publication" add metadata "$bundle" "$work_dir/repomd-b.xml.asc" \
	x86_64/repodata/repomd.xml.asc.bak 2>/dev/null; then
	fail "add accepted a wrong signature name after repomd.xml"
fi
"$publication" add metadata "$bundle" "$work_dir/repomd-a.xml.asc" \
	x86_64/repodata/repomd.xml.asc || fail "add repomd.xml.asc failed"
"$publication" add metadata "$bundle" "$work_dir/repomd-b.xml" \
	SRPMS/repodata/repomd.xml || fail "add second repomd.xml failed"
"$publication" add metadata "$bundle" "$work_dir/repomd-b.xml.asc" \
	SRPMS/repodata/repomd.xml.asc || fail "add second repomd.xml.asc failed"

ssh-keygen -q -t ed25519 -N '' -C test -f "$work_dir/sshkey" ||
	fail "ssh-keygen could not create a test key"
printf 'rsyslog-archive-publication namespaces="rsyslog-archive-publication" %s\n' \
	"$(cut -d' ' -f1,2 "$work_dir/sshkey.pub")" >"$work_dir/allowed"
"$publication" sign "$bundle" "$work_dir/sshkey" || fail "sign failed"
"$publication" verify "$bundle" "$work_dir/allowed" ||
	fail "verify rejected a good RPM bundle"

resign() {
	"$publication" sign "$work_dir/t" "$work_dir/sshkey"
	if "$publication" verify "$work_dir/t" "$work_dir/allowed" 2>/dev/null; then
		fail "verify accepted $1"
	fi
}
tamper_copy() {
	rm -rf "$work_dir/t"
	cp -a "$bundle" "$work_dir/t"
}
tamper_copy
sed '$d' "$bundle/manifest" >"$work_dir/t/manifest"
rm "$work_dir/t/files/$prefix/SRPMS/repodata/repomd.xml.asc"
resign "a bundle ending in a lone repomd.xml"
tamper_copy
sed '$d' "$bundle/manifest" >"$work_dir/t/manifest"
sed '$d' "$work_dir/t/manifest" >"$work_dir/t/m2"
mv "$work_dir/t/m2" "$work_dir/t/manifest"
rm "$work_dir/t/files/$prefix/SRPMS/repodata/repomd.xml.asc" \
	"$work_dir/t/files/$prefix/SRPMS/repodata/repomd.xml"
printf 'metadata %s 4 %s/z-extra\n' "$(sha "$work_dir/key.asc")" "$prefix" \
	>>"$work_dir/t/manifest"
printf 'key\n' >"$work_dir/t/files/$prefix/z-extra"
resign "a bundle ending in other metadata"
tamper_copy
# swap the final two entries: repomd.xml.asc before its repomd.xml
{
	sed '$d' "$bundle/manifest" | sed '$d'
	tail -n 1 "$bundle/manifest"
	tail -n 2 "$bundle/manifest" | head -n 1
} >"$work_dir/t/manifest"
resign "a repomd.xml.asc before its repomd.xml"
tamper_copy
sed 's/^metadata \(.*\/SRPMS\/repodata\/repomd.xml.asc\)$/immutable \1/' \
	"$bundle/manifest" >"$work_dir/t/manifest"
resign "repomd.xml.asc as an immutable entry"

# ---- fetch-verified-metadata --------------------------------------------

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

# write_repomd <file> <href> <type> <digest> <size> [extra-location-attr]
# [extra-top]: a repomd.xml with one data entry (type "primary").
write_repomd() {
	{
		printf '%s' "${7:-}"
		printf '<?xml version="1.0" encoding="UTF-8"?>\n'
		printf '<repomd xmlns="http://linux.duke.edu/metadata/repo">\n'
		printf '  <revision>1</revision>\n'
		printf '  <data type="primary">\n'
		printf '    <checksum type="%s">%s</checksum>\n' "$3" "$4"
		printf '    <location href="%s"%s/>\n' "$2" "${6:-}"
		[ "$5" = - ] || printf '    <size>%s</size>\n' "$5"
		printf '  </data>\n</repomd>\n'
	} >"$1"
}

# make_repo <name> <signing-gnupg-home>: a tiny repository with the two
# subdirectories x86_64 and SRPMS, each with one signed primary file.
make_repo() {
	local name="$1" home="$2" sub dir data digest size
	for sub in x86_64 SRPMS; do
		dir="$www/$name/$sub/repodata"
		mkdir -p "$dir"
		data="primary-$name-$sub"
		printf '%s\n' "$data" >"$dir/$data"
		digest="$(sha "$dir/$data")"
		size="$(wc -c <"$dir/$data" | tr -d ' ')"
		write_repomd "$dir/repomd.xml" "repodata/$data" sha256 "$digest" "$size"
		GNUPGHOME="$home" gpg --batch --yes --armor --detach-sign \
			--output "$dir/repomd.xml.asc" "$dir/repomd.xml"
	done
}
# resign_repo <name> <sub>: sign the (modified) repomd.xml again.
resign_repo() {
	local dir="$www/$1/$2/repodata"
	gpg --batch --yes --armor --detach-sign \
		--output "$dir/repomd.xml.asc" "$dir/repomd.xml"
}
# evil_repo <name> <href> <type> <extra-location-attr> [extra-top]
# A validly signed repomd.xml in x86_64 whose single entry is odd.
evil_repo() {
	local name="$1" dir
	make_repo "$name" "$GNUPGHOME"
	dir="$www/$name/x86_64/repodata"
	write_repomd "$dir/repomd.xml" "$2" "$3" \
		"$(sha "$dir/primary-$name-x86_64")" \
		"$(wc -c <"$dir/primary-$name-x86_64" | tr -d ' ')" "$4" "${5:-}"
	resign_repo "$name" x86_64
}

make_repo good "$GNUPGHOME"
make_repo foreign "$other_home"

make_repo tampered-data "$GNUPGHOME"
printf 'evil\n' >"$www/tampered-data/x86_64/repodata/primary-tampered-data-x86_64"

make_repo wrong-size "$GNUPGHOME"
dir="$www/wrong-size/x86_64/repodata"
write_repomd "$dir/repomd.xml" "repodata/primary-wrong-size-x86_64" sha256 \
	"$(sha "$dir/primary-wrong-size-x86_64")" 3
resign_repo wrong-size x86_64

make_repo tampered-repomd "$GNUPGHOME"
sed 's/<revision>1</<revision>2</' \
	"$www/tampered-repomd/x86_64/repodata/repomd.xml" >"$work_dir/r.xml"
mv "$work_dir/r.xml" "$www/tampered-repomd/x86_64/repodata/repomd.xml"

make_repo no-asc "$GNUPGHOME"
rm "$www/no-asc/SRPMS/repodata/repomd.xml.asc"

make_repo srpms-foreign "$GNUPGHOME"
cp "$www/foreign/SRPMS/repodata/repomd.xml.asc" \
	"$www/srpms-foreign/SRPMS/repodata/repomd.xml.asc"
# the foreign signature is over other content as well; also exercise a
# repomd.xml that is validly signed by the foreign key
cp "$www/foreign/SRPMS/repodata/repomd.xml" \
	"$www/srpms-foreign/SRPMS/repodata/repomd.xml"
cp "$www/foreign/SRPMS/repodata/primary-foreign-SRPMS" \
	"$www/srpms-foreign/SRPMS/repodata/primary-foreign-SRPMS"

evil_repo md5 repodata/primary-md5-x86_64 md5 ""
evil_repo traversal ../../x86_64/Packages/evil sha256 ""
evil_repo traversal2 repodata/../x sha256 ""
evil_repo traversal3 repodata/.. sha256 ""
evil_repo absolute /etc/passwd sha256 ""
evil_repo url http://127.0.0.1/repodata/x sha256 ""
evil_repo nested repodata/sub/primary-nested-x86_64 sha256 ""
evil_repo other-dir Packages/primary-other-dir-x86_64 sha256 ""
evil_repo xmlbase repodata/primary-xmlbase-x86_64 sha256 \
	' xml:base="http://evil.invalid/"'
evil_repo dtd repodata/primary-dtd-x86_64 sha256 "" \
	'<!DOCTYPE repomd [<!ENTITY x "y">]>
'
mkdir -p "$www/empty" "$www/half/x86_64"
# x86_64 exists, SRPMS does not: that repository starts from scratch.
cp -a "$www/good/x86_64/." "$www/half/x86_64/"
# Same shapes, but the prefix has been published before.
mkdir -p "$www/published" "$www/half-published/x86_64" "$www/status500"
cp -a "$www/good/x86_64/." "$www/half-published/x86_64/"
printf '{}\n' >"$www/published/publication-status.json"
printf '{}\n' >"$www/half-published/publication-status.json"

# Serve $www; any path below /error/ answers HTTP 500, as does
# publication-status.json below /status500/.
cat >"$work_dir/server.py" <<'PYEOF'
import functools
import http.server
import os
import socketserver
import sys


class Handler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith("/error/") or self.path == "/status500/publication-status.json":
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
PYEOF
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
export ARCHIVE_FETCH_ALLOW_HTTP=true

fetch() {
	rm -rf "$work_dir/rpm-repo"
	"$helper" fetch-verified-metadata "$base/$1" "$work_dir/rpm-repo" \
		"$fpr" x86_64 SRPMS
}
no_files_left() {
	! find "$work_dir/rpm-repo" -type f | grep -q .
}

fetch good || fail "fetch rejected a good repository"
for sub in x86_64 SRPMS; do
	for f in repomd.xml repomd.xml.asc "primary-good-$sub"; do
		cmp -s "$www/good/$sub/repodata/$f" "$work_dir/rpm-repo/$sub/repodata/$f" ||
			fail "fetch did not copy $sub/repodata/$f"
	done
done
# an https-only default refuses the plain-http loopback server
if ARCHIVE_FETCH_ALLOW_HTTP=false fetch good >/dev/null 2>&1; then
	fail "fetch accepted a plain http URL without the test hook"
fi
# stale local metadata is never reused
mkdir -p "$work_dir/rpm-repo/x86_64/repodata"
printf 'stale\n' >"$work_dir/rpm-repo/x86_64/repodata/stale"
"$helper" fetch-verified-metadata "$base/empty" "$work_dir/rpm-repo" \
	"$fpr" x86_64 SRPMS || fail "fetch failed on an empty prefix (404)"
no_files_left || fail "fetch of an empty prefix left stale metadata behind"

for name in tampered-data wrong-size tampered-repomd foreign no-asc \
	srpms-foreign md5 traversal traversal2 traversal3 absolute url nested \
	other-dir xmlbase dtd error; do
	if fetch "$name" >/dev/null 2>&1; then
		fail "fetch accepted the '$name' repository"
	fi
	no_files_left || fail "fetch of the '$name' repository left metadata behind"
done
if "$helper" fetch-verified-metadata "$base/good" "$work_dir/rpm-repo" \
	0123456789ABCDEF0123456789ABCDEF01234567 x86_64 2>/dev/null; then
	fail "fetch accepted an archive key that is not in the keyring"
fi
if "$helper" fetch-verified-metadata "$base/good" "$work_dir/rpm-repo" \
	"$fpr" ../x86_64 2>/dev/null; then
	fail "fetch accepted a repository subdirectory with .."
fi

fetch empty || fail "fetch failed on an empty prefix (404)"
no_files_left || fail "fetch of an empty prefix created metadata files"
fetch half || fail "fetch failed when only SRPMS is missing (404)"
[ -f "$work_dir/rpm-repo/x86_64/repodata/repomd.xml" ] ||
	fail "fetch of a half-empty prefix lost the existing x86_64 metadata"
[ ! -e "$work_dir/rpm-repo/SRPMS/repodata" ] ||
	fail "fetch invented SRPMS metadata"

# 404 for repomd.xml: publication-status.json decides.
for name in published half-published status500; do
	if fetch "$name" >"$work_dir/$name.out" 2>"$work_dir/$name.err"; then
		fail "fetch accepted the '$name' prefix with a missing repomd.xml"
	fi
	no_files_left || fail "fetch of the '$name' prefix left metadata behind"
done
grep -q "publication-status.json" "$work_dir/published.err" ||
	fail "failure for a published prefix does not name publication-status.json"
for name in published half-published; do
	ARCHIVE_ALLOW_EMPTY_INDEX=true fetch "$name" >/dev/null 2>"$work_dir/override.err" ||
		fail "override did not allow a scratch start for '$name'"
	grep -Eq "WARNING|::warning" "$work_dir/override.err" ||
		fail "override for '$name' did not log a warning"
done
[ -f "$work_dir/rpm-repo/x86_64/repodata/repomd.xml" ] ||
	fail "override lost the existing x86_64 metadata of a half-published prefix"
[ ! -e "$work_dir/rpm-repo/SRPMS/repodata" ] ||
	fail "override invented SRPMS metadata"
if ARCHIVE_ALLOW_EMPTY_INDEX=true fetch status500 >/dev/null 2>&1; then
	fail "override accepted a publication-status.json error"
fi

echo "daily-stable rpm metadata fetch checks passed"
