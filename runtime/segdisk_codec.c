/*
 * Queue-private binary codec used by segmentedDisk.
 *
 * The format is a sequence of network-byte-order TLVs. Unknown optional
 * fields can be skipped; field ids with the critical flag must be understood.
 */
#include "config.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <json.h>
#include "segdisk_codec.h"
#include "ruleset.h"

#define TLV_HDR_LEN 8u
#define TLV_CRITICAL 0x01u
#define TLV_MAX_PAYLOAD (128u * 1024u * 1024u)

enum tlv_type { TLV_U8 = 1, TLV_U16 = 2, TLV_U32 = 3, TLV_U64 = 4, TLV_BYTES = 5, TLV_TIME = 6 };
enum tlv_field {
    F_PROTOCOL = 1,
    F_SEVERITY = 2,
    F_FACILITY = 3,
    F_FLAGS = 4,
    F_GENTIME = 5,
    F_RECEIVED = 6,
    F_TIMESTAMP = 7,
    F_TAG = 8,
    F_RAWMSG = 9,
    F_HOSTNAME = 10,
    F_INPUTNAME = 11,
    F_RCVFROM = 12,
    F_RCVFROMIP = 13,
    F_RCVFROMPORT = 14,
    F_STRUCTURED_DATA = 15,
    F_JSON = 16,
    F_LOCALVARS = 17,
    F_APPNAME = 18,
    F_PROCID = 19,
    F_MSGID = 20,
    F_UUID = 21,
    F_RULESET = 22,
    F_MSG_OFFSET = 23,
    F_AFTER_PRI_OFFSET = 24,
    F_PARSE_SUCCESS = 25
};

typedef struct encbuf_s {
    unsigned char *data;
    size_t len;
    size_t cap;
} encbuf_t;

static void put16(unsigned char *p, uint16_t v) {
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)v;
}

static void put32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void put64(unsigned char *p, uint64_t v) {
    put32(p, (uint32_t)(v >> 32));
    put32(p + 4, (uint32_t)v);
}

static uint16_t get16(const unsigned char *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t get32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint64_t get64(const unsigned char *p) {
    return ((uint64_t)get32(p) << 32) | get32(p + 4);
}

uint32_t segdiskCrc32c(const void *vbuf, size_t len) {
    const unsigned char *buf = vbuf;
    uint32_t crc = ~0u;
    for (size_t i = 0; i < len; ++i) {
        crc ^= buf[i];
        for (unsigned int bit = 0; bit < 8; ++bit) crc = (crc & 1u) ? (crc >> 1) ^ 0x82f63b78u : crc >> 1;
    }
    return ~crc;
}

static rsRetVal reserve(encbuf_t *b, size_t add) {
    if (add > TLV_MAX_PAYLOAD || b->len > TLV_MAX_PAYLOAD - add) return RS_RET_FILE_TOO_LARGE;
    const size_t need = b->len + add;
    if (need <= b->cap) return RS_RET_OK;
    size_t cap = b->cap == 0 ? 1024 : b->cap;
    while (cap < need) {
        if (cap > TLV_MAX_PAYLOAD / 2) {
            cap = TLV_MAX_PAYLOAD;
            break;
        }
        cap *= 2;
    }
    unsigned char *next = realloc(b->data, cap);
    if (next == NULL) return RS_RET_OUT_OF_MEMORY;
    b->data = next;
    b->cap = cap;
    return RS_RET_OK;
}

static rsRetVal add_tlv(
    encbuf_t *b, uint16_t field, unsigned char type, unsigned char flags, const void *data, size_t len) {
    if (len > UINT32_MAX) return RS_RET_FILE_TOO_LARGE;
    rsRetVal r = reserve(b, TLV_HDR_LEN + len);
    if (r != RS_RET_OK) return r;
    put16(b->data + b->len, field);
    b->data[b->len + 2] = type;
    b->data[b->len + 3] = flags;
    put32(b->data + b->len + 4, (uint32_t)len);
    if (len != 0) memcpy(b->data + b->len + TLV_HDR_LEN, data, len);
    b->len += TLV_HDR_LEN + len;
    return RS_RET_OK;
}

static rsRetVal add_u8(encbuf_t *b, uint16_t f, uint8_t v) {
    return add_tlv(b, f, TLV_U8, 0, &v, 1);
}

static rsRetVal add_u16(encbuf_t *b, uint16_t f, uint16_t v) {
    unsigned char d[2];
    put16(d, v);
    return add_tlv(b, f, TLV_U16, 0, d, sizeof(d));
}

static rsRetVal add_u32(encbuf_t *b, uint16_t f, uint32_t v) {
    unsigned char d[4];
    put32(d, v);
    return add_tlv(b, f, TLV_U32, 0, d, sizeof(d));
}

static rsRetVal add_u64(encbuf_t *b, uint16_t f, uint64_t v) {
    unsigned char d[8];
    put64(d, v);
    return add_tlv(b, f, TLV_U64, 0, d, sizeof(d));
}

static rsRetVal add_bytes(encbuf_t *b, uint16_t f, const void *p, size_t len) {
    return p == NULL ? RS_RET_OK : add_tlv(b, f, TLV_BYTES, 0, p, len);
}

static rsRetVal add_time(encbuf_t *b, uint16_t f, const struct syslogTime *t) {
    unsigned char d[20] = {0};
    d[0] = t->timeType;
    d[1] = t->month;
    d[2] = t->day;
    d[3] = t->wday;
    d[4] = t->hour;
    d[5] = t->minute;
    d[6] = t->second;
    d[7] = t->secfracPrecision;
    d[8] = t->OffsetMinute;
    d[9] = t->OffsetHour;
    d[10] = (unsigned char)t->OffsetMode;
    d[11] = t->inUTC;
    put16(d + 12, (uint16_t)t->year);
    put32(d + 14, (uint32_t)t->secfrac);
    return add_tlv(b, f, TLV_TIME, 0, d, sizeof(d));
}

static rsRetVal add_json(encbuf_t *b, uint16_t f, smsg_t *msg, struct json_object *json) {
    char *copy = NULL;
    if (json == NULL) return RS_RET_OK;
    MsgLock(msg);
    const char *text = json_object_to_json_string_ext(json, JSON_C_TO_STRING_PLAIN);
    if (text != NULL) copy = strdup(text);
    MsgUnlock(msg);
    if (copy == NULL) return RS_RET_OUT_OF_MEMORY;
    const rsRetVal r = add_bytes(b, f, copy, strlen(copy));
    free(copy);
    return r;
}

static rsRetVal add_cstr(encbuf_t *b, uint16_t f, smsg_t *msg, cstr_t *text) {
    if (text == NULL) return RS_RET_OK;
    MsgLock(msg);
    const rsRetVal r = add_bytes(b, f, text->pBuf, text->iStrLen);
    MsgUnlock(msg);
    return r;
}

rsRetVal segdiskCodecEncode(smsg_t *msg, unsigned char **buf, size_t *len) {
    encbuf_t b = {0};
    uchar *text;
    int textlen;
    rsRetVal r;
#define ADD(call)                                 \
    do {                                          \
        if ((r = (call)) != RS_RET_OK) goto fail; \
    } while (0)
    if (msg == NULL || buf == NULL || len == NULL) return RS_RET_PARAM_ERROR;
    ADD(add_u16(&b, F_PROTOCOL, (uint16_t)msg->iProtocolVersion));
    ADD(add_u16(&b, F_SEVERITY, msg->iSeverity));
    ADD(add_u16(&b, F_FACILITY, msg->iFacility));
    ADD(add_u32(&b, F_FLAGS, (uint32_t)msg->msgFlags));
    ADD(add_u64(&b, F_GENTIME, (uint64_t)(int64_t)msg->ttGenTime));
    ADD(add_time(&b, F_RECEIVED, &msg->tRcvdAt));
    ADD(add_time(&b, F_TIMESTAMP, &msg->tTIMESTAMP));
    getTAG(msg, &text, &textlen, LOCK_MUTEX);
    ADD(add_bytes(&b, F_TAG, text, (size_t)textlen));
    getRawMsg(msg, &text, &textlen);
    ADD(add_bytes(&b, F_RAWMSG, text, (size_t)textlen));
    ADD(add_bytes(&b, F_HOSTNAME, msg->pszHOSTNAME, (size_t)msg->iLenHOSTNAME));
    getInputName(msg, &text, &textlen);
    ADD(add_bytes(&b, F_INPUTNAME, text, (size_t)textlen));
    text = getRcvFrom(msg);
    ADD(add_bytes(&b, F_RCVFROM, text, strlen((char *)text)));
    text = getRcvFromIP(msg);
    ADD(add_bytes(&b, F_RCVFROMIP, text, strlen((char *)text)));
    text = getRcvFromPort(msg);
    ADD(add_bytes(&b, F_RCVFROMPORT, text, strlen((char *)text)));
    if (msg->pszStrucData != NULL) ADD(add_bytes(&b, F_STRUCTURED_DATA, msg->pszStrucData, msg->lenStrucData));
    ADD(add_json(&b, F_JSON, msg, msg->json));
    ADD(add_json(&b, F_LOCALVARS, msg, msg->localvars));
    ADD(add_cstr(&b, F_APPNAME, msg, msg->pCSAPPNAME));
    ADD(add_cstr(&b, F_PROCID, msg, msg->pCSPROCID));
    ADD(add_cstr(&b, F_MSGID, msg, msg->pCSMSGID));
    if (msg->pszUUID != NULL) ADD(add_bytes(&b, F_UUID, msg->pszUUID, strlen((char *)msg->pszUUID)));
    if (msg->pRuleset != NULL) {
        text = rulesetGetName(msg->pRuleset);
        ADD(add_bytes(&b, F_RULESET, text, strlen((char *)text)));
    }
    ADD(add_u32(&b, F_MSG_OFFSET, (uint32_t)msg->offMSG));
    ADD(add_u32(&b, F_AFTER_PRI_OFFSET, (uint32_t)msg->offAfterPRI));
    ADD(add_u8(&b, F_PARSE_SUCCESS, (uint8_t)msg->bParseSuccess));
    *buf = b.data;
    *len = b.len;
    return RS_RET_OK;
fail:
    free(b.data);
    return r;
#undef ADD
}

static rsRetVal dup_text(const unsigned char *p, uint32_t len, char **out) {
    char *s;
    if ((s = malloc((size_t)len + 1)) == NULL) return RS_RET_OUT_OF_MEMORY;
    memcpy(s, p, len);
    s[len] = '\0';
    *out = s;
    return RS_RET_OK;
}

static rsRetVal decode_time(const unsigned char *p, uint32_t len, struct syslogTime *t) {
    if (len != 20) return RS_RET_INVALID_VALUE;
    memset(t, 0, sizeof(*t));
    t->timeType = p[0];
    t->month = p[1];
    t->day = p[2];
    t->wday = p[3];
    t->hour = p[4];
    t->minute = p[5];
    t->second = p[6];
    t->secfracPrecision = p[7];
    t->OffsetMinute = p[8];
    t->OffsetHour = p[9];
    t->OffsetMode = (char)p[10];
    t->inUTC = p[11];
    t->year = (short)get16(p + 12);
    t->secfrac = (int)get32(p + 14);
    return RS_RET_OK;
}

rsRetVal segdiskCodecDecode(const unsigned char *buf, size_t len, smsg_t **out) {
    smsg_t *msg = NULL;
    uint32_t seen = 0;
    size_t pos = 0;
    rsRetVal r = msgConstructForDeserializer(&msg);
    if (r != RS_RET_OK) return r;
    while (pos < len) {
        if (len - pos < TLV_HDR_LEN) {
            r = RS_RET_INVALID_VALUE;
            goto fail;
        }
        const uint16_t field = get16(buf + pos);
        const unsigned char type = buf[pos + 2];
        const unsigned char flags = buf[pos + 3];
        const uint32_t n = get32(buf + pos + 4);
        pos += TLV_HDR_LEN;
        if (n > TLV_MAX_PAYLOAD || n > len - pos) {
            r = RS_RET_INVALID_VALUE;
            goto fail;
        }
        const unsigned char *p = buf + pos;
        char *s = NULL;
        if (field >= 1 && field <= 31) {
            const uint32_t mask = 1u << (field - 1);
            if ((seen & mask) != 0) {
                r = RS_RET_INVALID_VALUE;
                goto fail;
            }
            seen |= mask;
        }
#define NEED(t, nbytes)                     \
    do {                                    \
        if (type != (t) || n != (nbytes)) { \
            r = RS_RET_INVALID_VALUE;       \
            goto fail;                      \
        }                                   \
    } while (0)
#define TEXT_CALL(call)                                       \
    do {                                                      \
        if ((r = dup_text(p, n, &s)) != RS_RET_OK) goto fail; \
        r = (call);                                           \
        free(s);                                              \
        s = NULL;                                             \
        if (r != RS_RET_OK) goto fail;                        \
    } while (0)
        switch (field) {
            case F_PROTOCOL:
                NEED(TLV_U16, 2);
                setProtocolVersion(msg, get16(p));
                break;
            case F_SEVERITY:
                NEED(TLV_U16, 2);
                msg->iSeverity = get16(p);
                break;
            case F_FACILITY:
                NEED(TLV_U16, 2);
                msg->iFacility = get16(p);
                break;
            case F_FLAGS:
                NEED(TLV_U32, 4);
                msg->msgFlags = (int)get32(p);
                break;
            case F_GENTIME:
                NEED(TLV_U64, 8);
                msg->ttGenTime = (time_t)(int64_t)get64(p);
                break;
            case F_RECEIVED:
                NEED(TLV_TIME, 20);
                if ((r = decode_time(p, n, &msg->tRcvdAt)) != RS_RET_OK) goto fail;
                break;
            case F_TIMESTAMP:
                NEED(TLV_TIME, 20);
                if ((r = decode_time(p, n, &msg->tTIMESTAMP)) != RS_RET_OK) goto fail;
                break;
            case F_TAG:
                if (type != TLV_BYTES) {
                    r = RS_RET_INVALID_VALUE;
                    goto fail;
                }
                MsgSetTAG(msg, p, n);
                break;
            case F_RAWMSG:
                if (type != TLV_BYTES) {
                    r = RS_RET_INVALID_VALUE;
                    goto fail;
                }
                MsgSetRawMsg(msg, (const char *)p, n);
                break;
            case F_HOSTNAME:
                if (type != TLV_BYTES) {
                    r = RS_RET_INVALID_VALUE;
                    goto fail;
                }
                MsgSetHOSTNAME(msg, p, n);
                break;
            case F_INPUTNAME:
                if (type != TLV_BYTES || (r = MsgSetInputNameStr(msg, p, n)) != RS_RET_OK) goto fail;
                break;
            case F_RCVFROM:
                if (type != TLV_BYTES || (r = MsgSetRcvFromText(msg, p, n)) != RS_RET_OK) goto fail;
                break;
            case F_RCVFROMIP:
                if (type != TLV_BYTES || (r = MsgSetRcvFromIPText(msg, p, n)) != RS_RET_OK) goto fail;
                break;
            case F_RCVFROMPORT:
                if (type != TLV_BYTES || (r = MsgSetRcvFromPortText(msg, p, n)) != RS_RET_OK) goto fail;
                break;
            case F_STRUCTURED_DATA:
                TEXT_CALL(MsgSetStructuredData(msg, s));
                break;
            case F_JSON:
            case F_LOCALVARS: {
                if (type != TLV_BYTES || (r = dup_text(p, n, &s)) != RS_RET_OK) goto fail;
                struct json_object *j = json_tokener_parse(s);
                free(s);
                s = NULL;
                if (j == NULL) {
                    r = RS_RET_INVALID_VALUE;
                    goto fail;
                }
                if (field == F_JSON)
                    msg->json = j;
                else
                    msg->localvars = j;
                break;
            }
            case F_APPNAME:
                TEXT_CALL(MsgSetAPPNAME(msg, s));
                break;
            case F_PROCID:
                TEXT_CALL(MsgSetPROCID(msg, s));
                break;
            case F_MSGID:
                TEXT_CALL(MsgSetMSGID(msg, s));
                break;
            case F_UUID:
                if (type != TLV_BYTES || (r = dup_text(p, n, &s)) != RS_RET_OK) goto fail;
                msg->pszUUID = (uchar *)s;
                s = NULL;
                break;
            case F_RULESET:
                if (type != TLV_BYTES || (r = dup_text(p, n, &s)) != RS_RET_OK) goto fail;
                {
                    cstr_t *cs = NULL;
                    if (rsCStrConstructFromszStr(&cs, (uchar *)s) != RS_RET_OK) {
                        free(s);
                        r = RS_RET_OUT_OF_MEMORY;
                        goto fail;
                    }
                    cstrFinalize(cs);
                    r = MsgSetRulesetByName(msg, cs);
                    rsCStrDestruct(&cs);
                    free(s);
                    s = NULL;
                    if (r != RS_RET_OK) goto fail;
                }
                break;
            case F_MSG_OFFSET:
                NEED(TLV_U32, 4);
                MsgSetMSGoffs(msg, (int)get32(p));
                break;
            case F_AFTER_PRI_OFFSET:
                NEED(TLV_U32, 4);
                if ((r = MsgSetAfterPRIOffs(msg, (int)get32(p))) != RS_RET_OK) goto fail;
                break;
            case F_PARSE_SUCCESS:
                NEED(TLV_U8, 1);
                MsgSetParseSuccess(msg, p[0]);
                break;
            default:
                if ((flags & TLV_CRITICAL) != 0) {
                    r = RS_RET_INVALID_VALUE;
                    goto fail;
                }
                break;
        }
#undef NEED
#undef TEXT_CALL
        pos += n;
    }
    if ((seen & ((1u << (F_RAWMSG - 1)) | (1u << (F_MSG_OFFSET - 1)))) !=
        ((1u << (F_RAWMSG - 1)) | (1u << (F_MSG_OFFSET - 1)))) {
        r = RS_RET_INVALID_VALUE;
        goto fail;
    }
    *out = msg;
    return RS_RET_OK;
fail:
    if (msg != NULL) msgDestruct(&msg);
    return r;
}
