/* SPDX-License-Identifier: Apache-2.0 */
/* Derive the exact platform oracle without signed negation of LONG_MIN. */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const unsigned long magnitude = (unsigned long)LONG_MAX + 1UL;
    const long long maximum = (long long)RAND_MAX * 2 + 1;
    const unsigned long expected = magnitude % RAND_MAX;
    if (magnitude % maximum == expected) return 77;
    printf("%lld %lu %lu\n", maximum, expected, magnitude % 10);
    return 0;
}
