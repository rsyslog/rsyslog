/* SPDX-License-Identifier: Apache-2.0
 * Complete a diagnostic response with bounded, strictly advancing sends.
 * The transport callback reports the number of bytes actually accepted.
 */
#ifndef IMDIAG_RESPONSE_H
#define IMDIAG_RESPONSE_H

static rsRetVal imdiagSendAll(void *stream,
                              uchar *buffer,
                              const ssize_t length,
                              rsRetVal (*sendChunk)(void *, uchar *, ssize_t *)) {
    ssize_t sent = 0;
    while (sent < length) {
        ssize_t chunk = length - sent;
        const rsRetVal ret = sendChunk(stream, buffer + sent, &chunk);
        if (ret != RS_RET_OK) return ret;
        if (chunk <= 0 || chunk > length - sent) return RS_RET_IO_ERROR;
        sent += chunk;
    }
    return RS_RET_OK;
}
#endif
