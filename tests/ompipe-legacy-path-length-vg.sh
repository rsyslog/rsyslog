#!/bin/bash
# Run the legacy ompipe path-boundary regression under Valgrind so the original
# out-of-bounds NUL write is a deterministic test failure.
# Sanitizer builds exercise the base test directly; Valgrind cannot launch
# binaries compiled with AddressSanitizer or ThreadSanitizer.
case "${CFLAGS:-}" in
	*-fsanitize=*)
		exit 77
		;;
esac

# Some distribution images ship Valgrind without the loader symbols it needs
# to start. Keep the ordinary regression active and skip only this unavailable
# Valgrind oracle in that environment.
if ! valgrind --error-exitcode=10 true >/dev/null 2>&1; then
	exit 77
fi

export USE_VALGRIND="YES"
. ${srcdir:-.}/ompipe-legacy-path-length.sh
