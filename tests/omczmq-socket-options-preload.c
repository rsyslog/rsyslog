/* SPDX-License-Identifier: Apache-2.0
 * Observe real CZMQ socket options before endpoint attachment. Suppress only
 * attachment and transmission so this regression needs no broker or timing.
 */
#include <czmq.h>
#include <stdio.h>
#include <stdlib.h>

int zsock_attach(zsock_t *self, const char *endpoints, bool serverish) {
    (void)serverish;
    const char *path = getenv("OMCZMQ_OPTIONS_LOG");
    FILE *stream = path == NULL ? NULL : fopen(path, "a");
    if (stream == NULL) return -1;
    fprintf(stream, "%s sndhwm=%d sndtimeo=%d\n", endpoints, zsock_sndhwm(self), zsock_sndtimeo(self));
    return fclose(stream) == 0 ? 0 : -1;
}

int zstr_send(void *dest, const char *string) {
    (void)dest;
    (void)string;
    return 0;
}
