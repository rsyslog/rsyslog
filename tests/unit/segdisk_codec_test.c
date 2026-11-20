/* SPDX-License-Identifier: Apache-2.0 */
/* Direct segmentedDisk codec regression tests. Mutate one TLV in an encoded
 * message without changing its framing; rejection must return INVALID_VALUE
 * and no message. Valid sentinels, the no-MSG boundary, legacy facility and
 * stale PRI offsets must replay with the exact expected fields. */
#include "config.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rsyslog.h"
#include "obj.h"
#include "msg.h"
#include "segdisk_codec.h"
#include "template.h"
#include "dirty.h"
#include "rainerscript.h"

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            abort();                                                                 \
        }                                                                            \
    } while (0)

/* The runtime is linked whole, as in the existing message unit test. These
 * daemon entry points are inert: no configuration, queue or daemon runs. */
rsconf_t *ourConf = NULL;
int iConfigVerify = 0;
int bHaveMainQueue = 0;
int MarkInterval = 20 * 60;

rsRetVal logmsgInternal(const int iErr, const syslog_pri_t pri, const uchar *const msg, int flags) {
    (void)iErr;
    (void)pri;
    (void)flags;
    fprintf(stderr, "rsyslog internal message: %s\n", msg);
    return RS_RET_OK;
}

rsRetVal submitMsg2(smsg_t *pMsg) {
    msgDestruct(&pMsg);
    return RS_RET_OK;
}

rsRetVal multiSubmitMsg2(multi_submit_t *const pMultiSub) {
    (void)pMultiSub;
    return RS_RET_OK;
}

rsRetVal multiSubmitFlush(multi_submit_t *pMultiSub) {
    (void)pMultiSub;
    return RS_RET_OK;
}

rsRetVal createMainQueue(qqueue_t **ppQueue, uchar *pszQueueName, struct nvlst *lst) {
    (void)ppQueue;
    (void)pszQueueName;
    (void)lst;
    return RS_RET_NOT_IMPLEMENTED;
}

rsRetVal startMainQueue(rsconf_t *cnf, qqueue_t *pQueue) {
    (void)cnf;
    (void)pQueue;
    return RS_RET_NOT_IMPLEMENTED;
}

rsRetVal queryLocalHostname(rsconf_t *const pConf) {
    (void)pConf;
    return RS_RET_OK;
}

/* Built-in module entry points live in tools/ and are only registered when
 * a configuration is loaded, which this test never does. */
#define INERT_MODINIT(name)                                                                         \
    rsRetVal name(int, int *, rsRetVal (**)(), rsRetVal (*)(uchar *, rsRetVal(**)()), modInfo_t *); \
    rsRetVal name(int iIFVersRequested, int *ipIFVersProvided, rsRetVal (**pQueryEtryPt)(),         \
                  rsRetVal (*pHostQueryEtryPt)(uchar *, rsRetVal(**)()), modInfo_t *pModInfo) {     \
        (void)iIFVersRequested;                                                                     \
        (void)ipIFVersProvided;                                                                     \
        (void)pQueryEtryPt;                                                                         \
        (void)pHostQueryEtryPt;                                                                     \
        (void)pModInfo;                                                                             \
        return RS_RET_NOT_IMPLEMENTED;                                                              \
    }
INERT_MODINIT(modInitFile)
INERT_MODINIT(modInitFwd)
INERT_MODINIT(modInitPipe)
INERT_MODINIT(modInitShell)
INERT_MODINIT(modInitUsrMsg)
INERT_MODINIT(modInitDiscard)
INERT_MODINIT(modInitpmrfc3164)
INERT_MODINIT(modInitpmrfc5424)
INERT_MODINIT(modInitsmfile)
INERT_MODINIT(modInitsmfwd)
INERT_MODINIT(modInitsmtradfile)
INERT_MODINIT(modInitsmtradfwd)


static void put16(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)v;
}

static void put32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static uint32_t get32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static unsigned char *field_value(unsigned char *buf, size_t len, unsigned field, size_t *width) {
    size_t pos = 0;
    while (len - pos >= 8) {
        const unsigned id = ((unsigned)buf[pos] << 8) | buf[pos + 1];
        const uint32_t n = get32(buf + pos + 4);
        CHECK(n <= len - pos - 8);
        if (id == field) {
            *width = n;
            return buf + pos + 8;
        }
        pos += 8 + n;
    }
    CHECK(0);
    return NULL;
}

static void check_record(const unsigned char *base,
                         size_t len,
                         unsigned field,
                         uint32_t value,
                         int expect_ok,
                         unsigned expected_facility,
                         int expected_msg_offset,
                         int expected_after_pri) {
    unsigned char *copy = malloc(len);
    smsg_t *decoded = NULL;
    size_t width;
    CHECK(copy != NULL);
    memcpy(copy, base, len);
    unsigned char *slot = field_value(copy, len, field, &width);
    CHECK(width == 2 || width == 4);
    if (width == 2)
        put16(slot, value);
    else
        put32(slot, value);
    const rsRetVal result = segdiskCodecDecode(copy, len, &decoded);
    if (expect_ok) {
        CHECK(result == RS_RET_OK && decoded != NULL);
        CHECK(decoded->iFacility == expected_facility);
        CHECK(decoded->offMSG == expected_msg_offset);
        CHECK(decoded->offAfterPRI == expected_after_pri);
        msgDestruct(&decoded);
    } else {
        CHECK(result == RS_RET_INVALID_VALUE && decoded == NULL);
    }
    free(copy);
}

int main(void) {
    const char *errObj = NULL;
    obj_if_t objIF;
    smsg_t *msg = NULL;
    unsigned char *record = NULL;
    size_t len = 0;
    size_t raw_width;
    memset(&objIF, 0, sizeof(objIF));
    setenv("RSYSLOG_MODDIR", "../runtime/.libs/", 0);
    CHECK(rsrtInit(&errObj, &objIF) == RS_RET_OK);
    CHECK(msgConstruct(&msg) == RS_RET_OK);
    MsgSetRawMsg(msg, "<13>test", 8);
    msg->iFacility = 1;
    msg->iSeverity = 5;
    MsgSetMSGoffs(msg, 4);
    CHECK(MsgSetAfterPRIOffs(msg, 4) == RS_RET_OK);
    CHECK(segdiskCodecEncode(msg, &record, &len) == RS_RET_OK);

    /* F1: reject invalid PRI, but replay legacy shifted local0 as facility 16. */
    check_record(record, len, 2, 8, 0, 0, 0, 0);
    check_record(record, len, 3, 25, 0, 0, 0, 0);
    check_record(record, len, 3, 128, 1, 16, 4, 4);
    /* Low shifted values overlap valid ordinals; preserve ordinal precedence. */
    check_record(record, len, 3, 8, 1, 8, 4, 4);
    check_record(record, len, 3, 16, 1, 16, 4, 4);
    check_record(record, len, 3, 24, 1, 24, 4, 4);
    check_record(record, len, 3, 192, 1, 192, 4, 4);

    /* F2: the raw message has eight bytes; nine is the valid no-MSG offset. */
    check_record(record, len, 23, 10, 0, 0, 0, 0);
    check_record(record, len, 23, 9, 1, 1, 9, 4);
    check_record(record, len, 23, UINT32_MAX, 1, 1, -1, 4);

    /* A huge After-PRI offset is malformed; modest stale values recompute. */
    check_record(record, len, 24, UINT32_MAX, 0, 0, 0, 0);
    check_record(record, len, 24, 9, 1, 1, 4, 4);
    check_record(record, len, 24, 3, 1, 1, 4, 4);
    check_record(record, len, 24, 0, 1, 1, 4, 4);

    /* Rewritten raw text with no PRI makes the old offset stale but valid. */
    unsigned char *rewritten = malloc(len);
    CHECK(rewritten != NULL);
    memcpy(rewritten, record, len);
    unsigned char *raw = field_value(rewritten, len, 9, &raw_width);
    CHECK(raw_width == 8);
    raw[0] = 'x';
    check_record(rewritten, len, 24, 4, 1, 1, 4, 0);
    free(rewritten);

    free(record);
    msgDestruct(&msg);
    CHECK(rsrtExit() == RS_RET_OK);
    return 0;
}
