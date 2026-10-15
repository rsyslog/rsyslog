/* SPDX-License-Identifier: Apache-2.0
 * Exercise the production response loop with native transport callbacks.
 * Exact received bytes prove both full and partial sends; zero/error controls
 * must stop immediately, so a broken transport cannot spin or claim success.
 */
#include "config.h"
#include <stdio.h>
#include <string.h>
#include "rsyslog.h"
#include "../../plugins/imdiag/imdiag-response.h"

typedef struct {
    char received[16];
    size_t used;
    int calls;
    int mode;
} transport_t;

static rsRetVal sendChunk(void *context, uchar *buffer, ssize_t *length) {
    transport_t *transport = context;
    ++transport->calls;
    if (transport->mode == 2) {
        *length = 0;
        return RS_RET_OK;
    }
    if (transport->mode == 3) return RS_RET_IO_ERROR;
    if (transport->mode == 1 && *length > 2) *length = 2;
    memcpy(transport->received + transport->used, buffer, (size_t)*length);
    transport->used += (size_t)*length;
    return RS_RET_OK;
}

int main(void) {
    uchar response[] = "response\n";
    for (int mode = 0; mode < 4; ++mode) {
        transport_t transport = {.mode = mode};
        const rsRetVal ret = imdiagSendAll(&transport, response, sizeof(response) - 1, sendChunk);
        if (mode < 2) {
            if (ret != RS_RET_OK || transport.used != sizeof(response) - 1 ||
                memcmp(transport.received, response, sizeof(response) - 1) || transport.calls != (mode == 0 ? 1 : 5)) {
                fprintf(stderr, "response send failed for mode %d\n", mode);
                return 1;
            }
        } else if (ret != RS_RET_IO_ERROR || transport.used != 0 || transport.calls != 1) {
            fprintf(stderr, "response failure was not bounded for mode %d\n", mode);
            return 1;
        }
    }
    return 0;
}
