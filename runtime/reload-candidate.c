/* reload-candidate.c
 *
 * Capture-only configuration parsing used by transactional HUP reload. The
 * normal parser is reused for RainerScript and YAML, but dispatch into
 * globals, modules, actions, queues, and inputs is suppressed. A rejected
 * candidate therefore cannot mutate the active generation or external
 * resources.
 *
 * Concurrency & Locking:
 *   The parser and capture context are main-thread-only and non-reentrant.
 *   Runtime workers never access candidate objects.
 */
#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#include "rsyslog.h"
#include "action.h"
#include "reload-candidate.h"
#include "stringbuf.h"
#include "translate.h"
#ifdef HAVE_LIBYAML
    #include "yamlconf.h"
#endif
#include "grammar/parserif.h"
#include "grammar/grammar.h"
#include "grammar/rainerscript.h"

typedef struct rsReloadCandidateObject_s {
    struct cnfobj *object;
    struct rsReloadCandidateObject_s *next;
} rsReloadCandidateObject_t;

struct rsReloadCandidate_s {
    rsReloadCandidateObject_t *head;
    rsReloadCandidateObject_t *tail;
    size_t objectCount;
    int parseError;
    rsRetVal captureError;
};

static rsReloadCandidate_t *captureCandidate = NULL;
static rsReloadNormalizedGraphBuilderV1_t *sourceBuilder = NULL;
static es_str_t *sourceDefaultSerialized = NULL;
static struct nvlst *sourceGlobalParams = NULL;
static struct nvlst *sourceGlobalParamsTail = NULL;
static int sourceHaveGlobal = 0;
static size_t sourceOrdinal = 0;
static size_t sourceDefaultActionOrdinal = 0;
static int sourceError = 0;
static int sourceHaveMainQueue = 0;
typedef struct sourceInputOrdinal_s {
    char *type;
    size_t length;
    size_t count;
    struct sourceInputOrdinal_s *next;
} sourceInputOrdinal_t;
static sourceInputOrdinal_t *sourceInputOrdinals = NULL;

void rsReloadCandidateSourceNoteError(void) {
    if (sourceBuilder != NULL) sourceError = 1;
}

/* This compact SHA-256 implementation keeps reload fingerprints available in
 * minimal builds where OpenSSL is optional. It is used only on the control
 * path to avoid exposing raw configuration values through graph consumers. */
typedef struct reloadSha256_s {
    uint32_t state[8];
    uint64_t bitCount;
    unsigned char block[64];
    size_t blockLen;
} reloadSha256_t;

static uint32_t rotr32(const uint32_t value, const unsigned shift) {
    return (value >> shift) | (value << (32 - shift));
}

static void sha256Block(reloadSha256_t *ctx, const unsigned char *block) {
    static const uint32_t k[64] = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
        0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
        0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
        0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
    };
    uint32_t w[64];
    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];
    unsigned i;

    for (i = 0; i < 16; ++i) {
        w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) | ((uint32_t)block[i * 4 + 2] << 8) |
               block[i * 4 + 3];
    }
    for (; i < 64; ++i) {
        const uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = (uint32_t)((uint64_t)w[i - 16] + s0 + w[i - 7] + s1);
    }
    for (i = 0; i < 64; ++i) {
        const uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        const uint32_t ch = (e & f) ^ (~e & g);
        const uint32_t temp1 = (uint32_t)((uint64_t)h + s1 + ch + k[i] + w[i]);
        const uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temp2 = (uint32_t)((uint64_t)s0 + maj);
        h = g;
        g = f;
        f = e;
        e = (uint32_t)((uint64_t)d + temp1);
        d = c;
        c = b;
        b = a;
        a = (uint32_t)((uint64_t)temp1 + temp2);
    }
    ctx->state[0] = (uint32_t)((uint64_t)ctx->state[0] + a);
    ctx->state[1] = (uint32_t)((uint64_t)ctx->state[1] + b);
    ctx->state[2] = (uint32_t)((uint64_t)ctx->state[2] + c);
    ctx->state[3] = (uint32_t)((uint64_t)ctx->state[3] + d);
    ctx->state[4] = (uint32_t)((uint64_t)ctx->state[4] + e);
    ctx->state[5] = (uint32_t)((uint64_t)ctx->state[5] + f);
    ctx->state[6] = (uint32_t)((uint64_t)ctx->state[6] + g);
    ctx->state[7] = (uint32_t)((uint64_t)ctx->state[7] + h);
}

static void sha256Update(reloadSha256_t *ctx, const unsigned char *data, size_t length) {
    while (length != 0) {
        const size_t space = sizeof(ctx->block) - ctx->blockLen;
        const size_t take = length < space ? length : space;
        memcpy(ctx->block + ctx->blockLen, data, take);
        ctx->blockLen += take;
        data += take;
        length -= take;
        if (ctx->blockLen == sizeof(ctx->block)) {
            sha256Block(ctx, ctx->block);
            ctx->bitCount += 512;
            ctx->blockLen = 0;
        }
    }
}

static void sha256Final(reloadSha256_t *ctx, unsigned char digest[32]) {
    size_t i;
    const uint64_t bitCount = ctx->bitCount + (uint64_t)ctx->blockLen * 8;

    ctx->block[ctx->blockLen++] = 0x80;
    if (ctx->blockLen > 56) {
        memset(ctx->block + ctx->blockLen, 0, sizeof(ctx->block) - ctx->blockLen);
        sha256Block(ctx, ctx->block);
        ctx->blockLen = 0;
    }
    memset(ctx->block + ctx->blockLen, 0, 56 - ctx->blockLen);
    for (i = 0; i < 8; ++i) ctx->block[63 - i] = (unsigned char)(bitCount >> (i * 8));
    sha256Block(ctx, ctx->block);
    for (i = 0; i < 8; ++i) {
        digest[i * 4] = (unsigned char)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (unsigned char)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (unsigned char)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (unsigned char)ctx->state[i];
    }
}

static rsRetVal sha256Fingerprint(const es_str_t *serialized, char **fingerprint) {
    static const char hex[] = "0123456789abcdef";
    static const char domain[] = "rsyslog.reload.candidate.sha256.v1\\0";
    reloadSha256_t ctx = {
        .state = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU, 0x1f83d9abU,
                  0x5be0cd19U},
    };
    unsigned char digest[32];
    char *result;
    size_t i;
    DEFiRet;

    if (serialized == NULL || fingerprint == NULL || *fingerprint != NULL) return RS_RET_PARAM_ERROR;
    sha256Update(&ctx, (const unsigned char *)domain, sizeof(domain));
    sha256Update(&ctx, (const unsigned char *)es_getBufAddr((es_str_t *)serialized), es_strlen((es_str_t *)serialized));
    sha256Final(&ctx, digest);
    CHKmalloc(result = malloc(sizeof("sha256:") - 1 + sizeof(digest) * 2 + 1));
    memcpy(result, "sha256:", sizeof("sha256:") - 1);
    for (i = 0; i < sizeof(digest); ++i) {
        result[sizeof("sha256:") - 1 + i * 2] = hex[digest[i] >> 4];
        result[sizeof("sha256:") - 1 + i * 2 + 1] = hex[digest[i] & 0x0f];
    }
    result[sizeof("sha256:") - 1 + sizeof(digest) * 2] = '\0';
    *fingerprint = result;
    return RS_RET_OK;

finalize_it:
    RETiRet;
}

/* The grammar uses case-insensitive parameter names. Keep the serialization
 * equally insensitive so YAML's mapping order and spelling cannot affect a
 * candidate diff. Length-delimited hex fields avoid delimiter ambiguities. */
static rsRetVal appendBytes(es_str_t **out, const void *data, const size_t len) {
    static const char hex[] = "0123456789abcdef";
    const unsigned char *bytes = data;
    char lenbuf[32];
    size_t i;
    const int n = snprintf(lenbuf, sizeof(lenbuf), "%zu:", len);

    if (n < 0 || (size_t)n >= sizeof(lenbuf) || es_addBuf(out, lenbuf, (size_t)n) != 0) return RS_RET_OUT_OF_MEMORY;
    for (i = 0; i < len; ++i) {
        const char pair[2] = {hex[bytes[i] >> 4], hex[bytes[i] & 0x0f]};
        if (es_addBuf(out, pair, sizeof(pair)) != 0) return RS_RET_OUT_OF_MEMORY;
    }
    return es_addChar(out, ';') == 0 ? RS_RET_OK : RS_RET_OUT_OF_MEMORY;
}

static rsRetVal appendToken(es_str_t **out, const char *token) {
    return es_addBuf(out, token, strlen(token)) == 0 ? RS_RET_OK : RS_RET_OUT_OF_MEMORY;
}

static rsRetVal appendUnsigned(es_str_t **out, const unsigned long long value) {
    char buf[32];
    const int n = snprintf(buf, sizeof(buf), "%llu;", value);

    return n < 0 || (size_t)n >= sizeof(buf) || es_addBuf(out, buf, (size_t)n) != 0 ? RS_RET_OUT_OF_MEMORY : RS_RET_OK;
}

static rsRetVal appendSigned(es_str_t **out, const long long value) {
    char buf[32];
    const int n = snprintf(buf, sizeof(buf), "%lld;", value);

    return n < 0 || (size_t)n >= sizeof(buf) || es_addBuf(out, buf, (size_t)n) != 0 ? RS_RET_OUT_OF_MEMORY : RS_RET_OK;
}

static rsRetVal appendEStr(es_str_t **out, const es_str_t *str) {
    return str == NULL ? appendBytes(out, "", 0)
                       : appendBytes(out, es_getBufAddr((es_str_t *)str), es_strlen((es_str_t *)str));
}

static rsRetVal appendFoldedEStr(es_str_t **out, const es_str_t *str) {
    const unsigned char *value;
    size_t i;
    DEFiRet;

    if (str == NULL) return appendBytes(out, "", 0);
    value = (const unsigned char *)es_getBufAddr((es_str_t *)str);
    CHKiRet(appendUnsigned(out, es_strlen((es_str_t *)str)));
    for (i = 0; i < es_strlen((es_str_t *)str); ++i) {
        const unsigned char c = value[i];
        const char pair[2] = {"0123456789abcdef"[(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c) >> 4],
                              "0123456789abcdef"[(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c) & 0x0f]};
        if (es_addBuf(out, pair, sizeof(pair)) != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    if (es_addChar(out, ';') != 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);

finalize_it:
    RETiRet;
}

static int compareNvlst(const void *left, const void *right) {
    const struct nvlst *const *a = left;
    const struct nvlst *const *b = right;
    const size_t aLen = (*a)->name == NULL ? 0 : es_strlen((*a)->name);
    const size_t bLen = (*b)->name == NULL ? 0 : es_strlen((*b)->name);
    const unsigned char *aName =
        aLen == 0 ? (const unsigned char *)"" : (const unsigned char *)es_getBufAddr((*a)->name);
    const unsigned char *bName =
        bLen == 0 ? (const unsigned char *)"" : (const unsigned char *)es_getBufAddr((*b)->name);
    size_t i;

    for (i = 0; i < aLen && i < bLen; ++i) {
        const unsigned char ac = aName[i] >= 'A' && aName[i] <= 'Z' ? aName[i] + ('a' - 'A') : aName[i];
        const unsigned char bc = bName[i] >= 'A' && bName[i] <= 'Z' ? bName[i] + ('a' - 'A') : bName[i];
        if (ac != bc) return ac < bc ? -1 : 1;
    }
    if (aLen != bLen) return aLen < bLen ? -1 : 1;
    return (*a)->val.datatype < (*b)->val.datatype ? -1 : (*a)->val.datatype > (*b)->val.datatype;
}

static rsRetVal appendNvlstValue(es_str_t **out, const struct svar *value) {
    const struct cnfarray *array;
    int i;
    DEFiRet;

    if (value == NULL) return RS_RET_PARAM_ERROR;
    CHKiRet(appendUnsigned(out, (unsigned char)value->datatype));
    switch (value->datatype) {
        case 'S':
            CHKiRet(appendEStr(out, value->d.estr));
            break;
        case 'N':
            CHKiRet(appendSigned(out, value->d.n));
            break;
        case 'A':
            array = value->d.ar;
            if (array == NULL || array->nmemb < 0) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            CHKiRet(appendSigned(out, array->nmemb));
            for (i = 0; i < array->nmemb; ++i) CHKiRet(appendEStr(out, array->arr[i]));
            break;
        default:
            /* YAML and RainerScript configuration parameters use S/N/A. Do
             * not guess how an unsupported raw union member should be read. */
            ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
    }

finalize_it:
    RETiRet;
}

static rsRetVal appendNvlstEntries(es_str_t **out, const struct nvlst **entries, const size_t count) {
    size_t i;
    DEFiRet;

    if (count != 0) {
        qsort(entries, count, sizeof(*entries), compareNvlst);
        for (i = 1; i < count; ++i) {
            const es_str_t *previous = entries[i - 1]->name;
            const es_str_t *current = entries[i]->name;
            size_t j;

            if (previous == NULL || current == NULL ||
                es_strlen((es_str_t *)previous) != es_strlen((es_str_t *)current)) {
                continue;
            }
            for (j = 0; j < es_strlen((es_str_t *)previous); ++j) {
                unsigned char a = es_getBufAddr((es_str_t *)previous)[j];
                unsigned char b = es_getBufAddr((es_str_t *)current)[j];
                if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
                if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
                if (a != b) break;
            }
            if (j == es_strlen((es_str_t *)previous)) ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
        }
    }
    CHKiRet(appendToken(out, "P"));
    CHKiRet(appendUnsigned(out, count));
    for (i = 0; i < count; ++i) {
        CHKiRet(appendFoldedEStr(out, entries[i]->name));
        CHKiRet(appendNvlstValue(out, &entries[i]->val));
    }
    CHKiRet(appendToken(out, "."));

finalize_it:
    RETiRet;
}

static rsRetVal appendNvlst(es_str_t **out, const struct nvlst *list) {
    const struct nvlst *entry;
    const struct nvlst **entries = NULL;
    size_t count = 0;
    size_t i = 0;
    DEFiRet;

    for (entry = list; entry != NULL; entry = entry->next) ++count;
    if (count != 0) {
        CHKmalloc(entries = calloc(count, sizeof(*entries)));
        for (entry = list; entry != NULL; entry = entry->next) entries[i++] = entry;
    }
    CHKiRet(appendNvlstEntries(out, entries, count));

finalize_it:
    free(entries);
    RETiRet;
}

static int equalFoldedName(const es_str_t *left, const es_str_t *right) {
    size_t i;
    const size_t leftLen = left == NULL ? 0 : es_strlen((es_str_t *)left);
    const size_t rightLen = right == NULL ? 0 : es_strlen((es_str_t *)right);

    if (leftLen == 0 || leftLen != rightLen) return 0;
    for (i = 0; i < leftLen; ++i) {
        unsigned char a = es_getBufAddr((es_str_t *)left)[i];
        unsigned char b = es_getBufAddr((es_str_t *)right)[i];
        if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if (a != b) return 0;
    }
    return 1;
}

static rsRetVal appendGlobalMap(es_str_t **out, const struct nvlst *list) {
    const struct nvlst *entry;
    const struct nvlst **entries = NULL;
    size_t count = 0;
    size_t capacity = 0;
    size_t i;
    DEFiRet;

    for (entry = list; entry != NULL; entry = entry->next) {
        for (i = 0; i < count; ++i) {
            if (equalFoldedName(entries[i]->name, entry->name)) break;
        }
        if (i != count) {
            entries[i] = entry;
            continue;
        }
        if (count == capacity) {
            const size_t newCapacity = capacity == 0 ? 8 : capacity * 2;
            if (newCapacity < capacity || newCapacity > SIZE_MAX / sizeof(*entries))
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            CHKmalloc(entries = realloc(entries, newCapacity * sizeof(*entries)));
            capacity = newCapacity;
        }
        entries[count++] = entry;
    }
    CHKiRet(appendNvlstEntries(out, entries, count));

finalize_it:
    free(entries);
    RETiRet;
}

static rsRetVal validateNvlst(const struct nvlst *list) {
    es_str_t *serialized = NULL;
    DEFiRet;

    CHKmalloc(serialized = es_newStr(64));
    CHKiRet(appendNvlst(&serialized, list));

finalize_it:
    if (serialized != NULL) es_deleteStr(serialized);
    RETiRet;
}

static const struct nvlst *findNvlst(const struct nvlst *list, const char *name) {
    size_t nameLen = strlen(name);

    for (; list != NULL; list = list->next) {
        const unsigned char *entryName;
        size_t i;
        if (list->name == NULL || es_strlen(list->name) != nameLen) continue;
        entryName = (const unsigned char *)es_getBufAddr(list->name);
        for (i = 0; i < nameLen; ++i) {
            const unsigned char c =
                entryName[i] >= 'A' && entryName[i] <= 'Z' ? entryName[i] + ('a' - 'A') : entryName[i];
            const unsigned char wanted = name[i] >= 'A' && name[i] <= 'Z' ? name[i] + ('a' - 'A') : name[i];
            if (c != wanted) break;
        }
        if (i == nameLen) return list;
    }
    return NULL;
}

static const char *nvlstString(const struct nvlst *list, const char *name, size_t *length) {
    const struct nvlst *entry = findNvlst(list, name);

    if (length != NULL) *length = 0;
    if (entry == NULL || entry->val.datatype != 'S' || entry->val.d.estr == NULL) return NULL;
    if (length != NULL) *length = es_strlen(entry->val.d.estr);
    return (const char *)es_getBufAddr(entry->val.d.estr);
}

static rsRetVal appendExpr(es_str_t **out, const struct cnfexpr *expr);

static rsRetVal appendExpr(es_str_t **out, const struct cnfexpr *expr) {
    const struct cnfarray *array;
    const struct cnffunc *func;
    int i;
    DEFiRet;

    if (expr == NULL) return RS_RET_PARAM_ERROR;
    CHKiRet(appendToken(out, "E"));
    CHKiRet(appendUnsigned(out, expr->nodetype));
    switch (expr->nodetype) {
        case 'N':
            CHKiRet(appendSigned(out, ((const struct cnfnumval *)expr)->val));
            break;
        case 'S':
            CHKiRet(appendEStr(out, ((const struct cnfstringval *)expr)->estr));
            break;
        case 'V':
            CHKiRet(appendBytes(out, ((const struct cnfvar *)expr)->name, strlen(((const struct cnfvar *)expr)->name)));
            break;
        case 'A':
            array = (const struct cnfarray *)expr;
            if (array->nmemb < 0) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            CHKiRet(appendSigned(out, array->nmemb));
            for (i = 0; i < array->nmemb; ++i) CHKiRet(appendEStr(out, array->arr[i]));
            break;
        case 'F':
            func = (const struct cnffunc *)expr;
            CHKiRet(appendEStr(out, func->fname));
            CHKiRet(appendUnsigned(out, func->nParams));
            for (i = 0; i < func->nParams; ++i) CHKiRet(appendExpr(out, func->expr[i]));
            break;
        case S_FUNC_EXISTS:
            CHKiRet(appendBytes(out, ((const struct cnffuncexists *)expr)->varname,
                                strlen(((const struct cnffuncexists *)expr)->varname)));
            break;
        case NOT:
        case 'M':
            CHKiRet(appendExpr(out, expr->r));
            break;
        case CMP_EQ:
        case CMP_NE:
        case CMP_LE:
        case CMP_GE:
        case CMP_LT:
        case CMP_GT:
        case CMP_CONTAINS:
        case CMP_CONTAINSI:
        case CMP_STARTSWITH:
        case CMP_STARTSWITHI:
        case CMP_ENDSWITH:
        case OR:
        case AND:
        case '&':
        case '+':
        case '-':
        case '*':
        case '/':
        case '%':
            CHKiRet(appendExpr(out, expr->l));
            CHKiRet(appendExpr(out, expr->r));
            break;
        default:
            ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
    }
    CHKiRet(appendToken(out, "."));

finalize_it:
    RETiRet;
}

static rsRetVal appendStmtList(es_str_t **out, const struct cnfstmt *stmt);

static rsRetVal appendActionReference(es_str_t **out, const struct nvlst *syntax) {
    const char *name;
    size_t nameLen;
    DEFiRet;

    if (syntax == NULL) return RS_RET_PARAM_ERROR;
    name = nvlstString(syntax, "name", &nameLen);
    CHKiRet(appendToken(out, "A"));
    if (name == NULL) {
        /* The surrounding statement-list position is the stable anonymous
         * slot. Do not include mutable action parameters in a ruleset hash. */
        CHKiRet(appendToken(out, "anonymous"));
    } else {
        CHKiRet(appendToken(out, "named"));
        CHKiRet(appendBytes(out, (const uchar *)name, nameLen));
    }
    CHKiRet(appendToken(out, "."));

finalize_it:
    RETiRet;
}

static rsRetVal appendStmt(es_str_t **out, const struct cnfstmt *stmt) {
    unsigned canonicalType;
    DEFiRet;

    if (stmt == NULL) return RS_RET_PARAM_ERROR;
    canonicalType = stmt->nodetype;
    if (stmt->nodetype == S_ACT || stmt->nodetype == S_RELOAD_ACT)
        canonicalType = S_RELOAD_ACT;
    else if (stmt->nodetype == S_PRIFILT || stmt->nodetype == S_RELOAD_PRIFILT)
        canonicalType = S_RELOAD_PRIFILT;
    else if (stmt->nodetype == S_PROPFILT || stmt->nodetype == S_RELOAD_PROPFILT)
        canonicalType = S_RELOAD_PROPFILT;
    CHKiRet(appendToken(out, "T"));
    CHKiRet(appendUnsigned(out, canonicalType));
    switch (stmt->nodetype) {
        case S_NOP:
        case S_STOP:
            break;
        case S_RELOAD_ACT:
            CHKiRet(appendActionReference(out, stmt->d.reload_action));
            break;
        case S_ACT:
            if (stmt->d.act == NULL || stmt->d.act->pSyntaxLst == NULL) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
            CHKiRet(appendActionReference(out, stmt->d.act->pSyntaxLst));
            break;
        case S_IF:
            CHKiRet(appendExpr(out, stmt->d.s_if.expr));
            CHKiRet(appendStmtList(out, stmt->d.s_if.t_then));
            CHKiRet(appendStmtList(out, stmt->d.s_if.t_else));
            break;
        case S_FOREACH:
            if (stmt->d.s_foreach.iter == NULL) ABORT_FINALIZE(RS_RET_PARAM_ERROR);
            CHKiRet(appendBytes(out, stmt->d.s_foreach.iter->var, strlen(stmt->d.s_foreach.iter->var)));
            CHKiRet(appendExpr(out, stmt->d.s_foreach.iter->collection));
            CHKiRet(appendStmtList(out, stmt->d.s_foreach.body));
            break;
        case S_SET:
            CHKiRet(appendBytes(out, stmt->d.s_set.varname, strlen((const char *)stmt->d.s_set.varname)));
            CHKiRet(appendSigned(out, stmt->d.s_set.force_reset));
            CHKiRet(appendExpr(out, stmt->d.s_set.expr));
            break;
        case S_UNSET:
            CHKiRet(appendBytes(out, stmt->d.s_unset.varname, strlen((const char *)stmt->d.s_unset.varname)));
            break;
        case S_CALL:
            CHKiRet(appendEStr(out, stmt->d.s_call.name));
            break;
        case S_CALL_INDIRECT:
            CHKiRet(appendExpr(out, stmt->d.s_call_ind.expr));
            break;
        case S_RELOAD_PRIFILT:
        case S_PRIFILT:
            CHKiRet(
                appendBytes(out, stmt->printable, stmt->printable == NULL ? 0 : strlen((const char *)stmt->printable)));
            CHKiRet(appendStmtList(out, stmt->d.s_prifilt.t_then));
            CHKiRet(appendStmtList(out, stmt->d.s_prifilt.t_else));
            break;
        case S_RELOAD_PROPFILT:
        case S_PROPFILT:
            CHKiRet(
                appendBytes(out, stmt->printable, stmt->printable == NULL ? 0 : strlen((const char *)stmt->printable)));
            CHKiRet(appendStmtList(out, stmt->d.s_propfilt.t_then));
            CHKiRet(appendStmtList(out, stmt->d.s_propfilt.t_else));
            break;
        case S_RELOAD_LOOKUP_TABLE:
            CHKiRet(appendBytes(out, stmt->d.s_reload_lookup_table.table_name,
                                stmt->d.s_reload_lookup_table.table_name == NULL
                                    ? 0
                                    : strlen((const char *)stmt->d.s_reload_lookup_table.table_name)));
            CHKiRet(appendBytes(out, stmt->d.s_reload_lookup_table.stub_value,
                                stmt->d.s_reload_lookup_table.stub_value == NULL
                                    ? 0
                                    : strlen((const char *)stmt->d.s_reload_lookup_table.stub_value)));
            break;
        default:
            /* Candidate parsing deliberately never creates S_ACT, decoded
             * filters, or module runtime state. An unfamiliar raw node cannot
             * safely be called equivalent to an active configuration. */
            ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
    }
    CHKiRet(appendToken(out, "."));

finalize_it:
    RETiRet;
}

static rsRetVal appendStmtList(es_str_t **out, const struct cnfstmt *stmt) {
    DEFiRet;

    CHKiRet(appendToken(out, "L"));
    while (stmt != NULL) {
        CHKiRet(appendStmt(out, stmt));
        stmt = stmt->next;
    }
    CHKiRet(appendToken(out, "."));

finalize_it:
    RETiRet;
}

static rsRetVal appendObject(es_str_t **out, const struct cnfobj *object) {
    const struct objlst *subobject;
    DEFiRet;

    if (object == NULL) return RS_RET_PARAM_ERROR;
    CHKiRet(appendToken(out, "O"));
    CHKiRet(appendUnsigned(out, object->objType));
    CHKiRet(appendNvlst(out, object->nvlst));
    CHKiRet(appendStmtList(out, object->script));
    CHKiRet(appendToken(out, "C"));
    for (subobject = object->subobjs; subobject != NULL; subobject = subobject->next) {
        CHKiRet(appendObject(out, subobject->obj));
    }
    CHKiRet(appendToken(out, "."));

finalize_it:
    RETiRet;
}

static rsRetVal objectFingerprint(const struct cnfobj *object, char **fingerprint) {
    es_str_t *serialized = NULL;
    DEFiRet;

    if (object == NULL || fingerprint == NULL || *fingerprint != NULL) return RS_RET_PARAM_ERROR;
    CHKmalloc(serialized = es_newStr(256));
    CHKiRet(appendToken(&serialized, "rsyslog.reload.candidate.v1;"));
    CHKiRet(appendObject(&serialized, object));
    CHKiRet(sha256Fingerprint(serialized, fingerprint));

finalize_it:
    if (serialized != NULL) es_deleteStr(serialized);
    RETiRet;
}

static rsRetVal defaultRulesetFingerprint(const rsReloadCandidate_t *candidate, char **fingerprint) {
    const rsReloadCandidateObject_t *entry;
    es_str_t *serialized = NULL;
    DEFiRet;

    if (candidate == NULL || fingerprint == NULL || *fingerprint != NULL) return RS_RET_PARAM_ERROR;
    CHKmalloc(serialized = es_newStr(256));
    CHKiRet(appendToken(&serialized, "rsyslog.reload.default-ruleset.v1;"));
    for (entry = candidate->head; entry != NULL; entry = entry->next) {
        if (entry->object->objType == CNFOBJ_RULESET && entry->object->nvlst == NULL) {
            CHKiRet(appendObject(&serialized, entry->object));
        }
    }
    CHKiRet(sha256Fingerprint(serialized, fingerprint));

finalize_it:
    if (serialized != NULL) es_deleteStr(serialized);
    RETiRet;
}

static rsRetVal globalFingerprint(const rsReloadCandidate_t *candidate, char **fingerprint) {
    const rsReloadCandidateObject_t *candidateEntry;
    struct nvlst *params = NULL;
    struct nvlst *tail = NULL;
    es_str_t *serialized = NULL;
    DEFiRet;

    if (candidate == NULL || fingerprint == NULL || *fingerprint != NULL) return RS_RET_PARAM_ERROR;
    for (candidateEntry = candidate->head; candidateEntry != NULL; candidateEntry = candidateEntry->next) {
        if (candidateEntry->object->objType != CNFOBJ_GLOBAL) continue;
        if (candidateEntry->object->script != NULL || candidateEntry->object->subobjs != NULL)
            return RS_RET_NOT_IMPLEMENTED;
        CHKiRet(validateNvlst(candidateEntry->object->nvlst));
        {
            struct nvlst *copy = rsconfTranslateCloneNvlst(candidateEntry->object->nvlst);
            if (candidateEntry->object->nvlst != NULL && copy == NULL) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            if (tail == NULL)
                params = copy;
            else
                tail->next = copy;
            if (copy != NULL) {
                tail = copy;
                while (tail->next != NULL) tail = tail->next;
            }
        }
    }
    CHKmalloc(serialized = es_newStr(256));
    CHKiRet(appendToken(&serialized, "rsyslog.reload.global.v1;"));
    CHKiRet(appendGlobalMap(&serialized, params));
    CHKiRet(sha256Fingerprint(serialized, fingerprint));

finalize_it:
    nvlstDestruct(params);
    if (serialized != NULL) es_deleteStr(serialized);
    RETiRet;
}

static rsReloadObjectKind_t objectKind(const struct cnfobj *object) {
    switch (object->objType) {
        case CNFOBJ_GLOBAL:
            return RS_RELOAD_OBJ_GLOBAL;
        case CNFOBJ_MAINQ:
            return RS_RELOAD_OBJ_MAIN_QUEUE;
        case CNFOBJ_MODULE:
            return RS_RELOAD_OBJ_MODULE;
        case CNFOBJ_INPUT:
            return RS_RELOAD_OBJ_INPUT;
        case CNFOBJ_RULESET:
            return RS_RELOAD_OBJ_RULESET;
        case CNFOBJ_TPL:
            return RS_RELOAD_OBJ_TEMPLATE;
        case CNFOBJ_ACTION:
            return RS_RELOAD_OBJ_ACTION;
        case CNFOBJ_PARSER:
            return RS_RELOAD_OBJ_PARSER;
        case CNFOBJ_TIMEZONE:
            return RS_RELOAD_OBJ_TIMEZONE;
        case CNFOBJ_LOOKUP_TABLE:
            return RS_RELOAD_OBJ_LOOKUP_TABLE;
        case CNFOBJ_DYN_STATS:
            return RS_RELOAD_OBJ_DYN_STATS;
        case CNFOBJ_PERCTILE_STATS:
            return RS_RELOAD_OBJ_PERCTILE_STATS;
        case CNFOBJ_RATELIMIT:
            return RS_RELOAD_OBJ_RATELIMIT;
        case CNFOBJ_PROPERTY:
            return RS_RELOAD_OBJ_PROPERTY;
        case CNFOBJ_CONSTANT:
            return RS_RELOAD_OBJ_CONSTANT;
        default:
            return RS_RELOAD_OBJ_OPAQUE;
    }
}

static rsRetVal makeIdentity(const struct cnfobj *object, const size_t ordinal, char **identity) {
    const char *value = NULL;
    const char *type = cnfobjType2str(object->objType);
    const char *key = "name";
    size_t valueLen = 0;
    size_t required;
    DEFiRet;

    if (object == NULL || identity == NULL || *identity != NULL || type == NULL) return RS_RET_PARAM_ERROR;
    switch (object->objType) {
        case CNFOBJ_GLOBAL:
            *identity = strdup("global");
            return *identity == NULL ? RS_RET_OUT_OF_MEMORY : RS_RET_OK;
        case CNFOBJ_MAINQ:
            *identity = strdup("main_queue");
            return *identity == NULL ? RS_RET_OUT_OF_MEMORY : RS_RET_OK;
        case CNFOBJ_MODULE:
            key = "load";
            break;
        case CNFOBJ_TIMEZONE:
            key = "id";
            break;
        case CNFOBJ_INPUT:
            key = "name";
            value = nvlstString(object->nvlst, key, &valueLen);
            if (value == NULL) {
                const char *const inputType = nvlstString(object->nvlst, "type", &valueLen);
                const int n = inputType == NULL
                                  ? snprintf(NULL, 0, "%s:anonymous:%zu", type, ordinal)
                                  : snprintf(NULL, 0, "%s:%.*s:anonymous:%zu", type, (int)valueLen, inputType, ordinal);
                if (n < 0) return RS_RET_OUT_OF_MEMORY;
                CHKmalloc(*identity = malloc((size_t)n + 1));
                if (inputType == NULL)
                    snprintf(*identity, (size_t)n + 1, "%s:anonymous:%zu", type, ordinal);
                else {
                    char *p;
                    size_t i;
                    snprintf(*identity, (size_t)n + 1, "%s:%.*s:anonymous:%zu", type, (int)valueLen, inputType,
                             ordinal);
                    for (p = *identity + strlen(type) + 1, i = 0; i < valueLen; ++p, ++i) {
                        if (*p >= 'A' && *p <= 'Z') *p += 'a' - 'A';
                    }
                }
                return RS_RET_OK;
            }
            break;
        case CNFOBJ_RULESET:
            value = nvlstString(object->nvlst, key, &valueLen);
            if (value == NULL && object->nvlst == NULL) {
                *identity = strdup("ruleset:default");
                return *identity == NULL ? RS_RET_OUT_OF_MEMORY : RS_RET_OK;
            }
            break;
        case CNFOBJ_ACTION:
        case CNFOBJ_TPL:
        case CNFOBJ_PROPERTY:
        case CNFOBJ_CONSTANT:
        case CNFOBJ_LOOKUP_TABLE:
        case CNFOBJ_PARSER:
        case CNFOBJ_DYN_STATS:
        case CNFOBJ_PERCTILE_STATS:
        case CNFOBJ_RATELIMIT:
        default:
            break;
    }
    if (value == NULL) value = nvlstString(object->nvlst, key, &valueLen);
    if (value == NULL) {
        const int n = snprintf(NULL, 0, "%s:anonymous:%zu", type, ordinal);
        if (n < 0) return RS_RET_OUT_OF_MEMORY;
        CHKmalloc(*identity = malloc((size_t)n + 1));
        snprintf(*identity, (size_t)n + 1, "%s:anonymous:%zu", type, ordinal);
        return RS_RET_OK;
    }
    if (valueLen > SIZE_MAX - strlen(type) - 2) return RS_RET_OUT_OF_MEMORY;
    required = strlen(type) + 1 + valueLen + 1;
    CHKmalloc(*identity = malloc(required));
    snprintf(*identity, required, "%s:%.*s", type, (int)valueLen, value);
    return RS_RET_OK;

finalize_it:
    free(*identity);
    *identity = NULL;
    RETiRet;
}

static int equalFoldedString(const char *left, const size_t leftLen, const char *right, const size_t rightLen) {
    size_t i;

    if (leftLen != rightLen) return 0;
    for (i = 0; i < leftLen; ++i) {
        unsigned char a = (unsigned char)left[i];
        unsigned char b = (unsigned char)right[i];
        if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if (a != b) return 0;
    }
    return 1;
}

static size_t inputTypeOrdinal(const rsReloadCandidate_t *candidate, const rsReloadCandidateObject_t *target) {
    const rsReloadCandidateObject_t *entry;
    const char *targetType;
    size_t targetTypeLen;
    size_t ordinal = 0;

    targetType = nvlstString(target->object->nvlst, "type", &targetTypeLen);
    for (entry = candidate->head; entry != NULL; entry = entry->next) {
        const char *entryType;
        size_t entryTypeLen;

        if (entry->object->objType != CNFOBJ_INPUT) continue;
        if (nvlstString(entry->object->nvlst, "name", NULL) != NULL) {
            if (entry == target) break;
            continue;
        }
        entryType = nvlstString(entry->object->nvlst, "type", &entryTypeLen);
        if ((targetType == NULL && entryType == NULL) ||
            (targetType != NULL && entryType != NULL &&
             equalFoldedString(targetType, targetTypeLen, entryType, entryTypeLen))) {
            ++ordinal;
        }
        if (entry == target) break;
    }
    return ordinal;
}

static void sourceInputOrdinalsDestruct(void) {
    sourceInputOrdinal_t *entry;
    while (sourceInputOrdinals != NULL) {
        entry = sourceInputOrdinals;
        sourceInputOrdinals = entry->next;
        free(entry->type);
        free(entry);
    }
}

static rsRetVal sourceInputTypeOrdinal(const struct cnfobj *object, size_t *ordinal) {
    const char *type;
    size_t length;
    sourceInputOrdinal_t *entry;

    if (object == NULL || ordinal == NULL) return RS_RET_PARAM_ERROR;
    *ordinal = 0;
    if (nvlstString(object->nvlst, "name", NULL) != NULL) return RS_RET_OK;
    type = nvlstString(object->nvlst, "type", &length);
    for (entry = sourceInputOrdinals; entry != NULL; entry = entry->next) {
        if ((type == NULL && entry->length == 0) ||
            (type != NULL && equalFoldedString(type, length, entry->type, entry->length))) {
            *ordinal = ++entry->count;
            return RS_RET_OK;
        }
    }
    entry = calloc(1, sizeof(*entry));
    if (entry == NULL) return RS_RET_OUT_OF_MEMORY;
    entry->type = type == NULL ? strdup("") : strndup(type, length);
    if (entry->type == NULL) {
        free(entry);
        return RS_RET_OUT_OF_MEMORY;
    }
    entry->length = length;
    entry->count = 1;
    entry->next = sourceInputOrdinals;
    sourceInputOrdinals = entry;
    *ordinal = 1;
    return RS_RET_OK;
}

static void lowerRulesetIdentity(char *identity) {
    char *value = strchr(identity, ':');

    if (value == NULL) return;
    for (++value; *value != '\0'; ++value) {
        if (*value >= 'A' && *value <= 'Z') *value += 'a' - 'A';
    }
}

static rsRetVal addActionNodes(rsReloadNormalizedGraphBuilderV1_t *builder,
                               const char *rulesetIdentity,
                               const struct cnfstmt *stmt,
                               size_t *ordinal) {
    char *identity = NULL;
    char *fingerprint = NULL;
    const char *name;
    size_t nameLen;
    es_str_t *serialized = NULL;
    DEFiRet;

    for (; stmt != NULL; stmt = stmt->next) {
        if (stmt->nodetype == S_RELOAD_ACT || stmt->nodetype == S_ACT) {
            const struct nvlst *const actionSyntax = stmt->nodetype == S_RELOAD_ACT
                                                         ? stmt->d.reload_action
                                                         : (stmt->d.act == NULL ? NULL : stmt->d.act->pSyntaxLst);

            if (actionSyntax == NULL) ABORT_FINALIZE(RS_RET_NOT_IMPLEMENTED);
            name = nvlstString(actionSyntax, "name", &nameLen);
            if (name == NULL) {
                ++*ordinal;
                const int n = snprintf(NULL, 0, "action:%s:anonymous:%zu", rulesetIdentity, *ordinal);
                if (n < 0) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                CHKmalloc(identity = malloc((size_t)n + 1));
                snprintf(identity, (size_t)n + 1, "action:%s:anonymous:%zu", rulesetIdentity, *ordinal);
            } else {
                const size_t prefixLen = strlen("action:");
                if (nameLen > SIZE_MAX - prefixLen - 1) ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                CHKmalloc(identity = malloc(prefixLen + nameLen + 1));
                snprintf(identity, prefixLen + nameLen + 1, "action:%.*s", (int)nameLen, name);
            }
            CHKiRet(rsReloadActionSyntaxFingerprintV1(actionSyntax, &fingerprint));
            CHKiRet(rsReloadNormalizedGraphBuilderV1Add(builder, RS_RELOAD_OBJ_ACTION, identity, fingerprint));
            free(identity);
            identity = NULL;
            free(fingerprint);
            fingerprint = NULL;
        }
        if (stmt->nodetype == S_IF) {
            CHKiRet(addActionNodes(builder, rulesetIdentity, stmt->d.s_if.t_then, ordinal));
            CHKiRet(addActionNodes(builder, rulesetIdentity, stmt->d.s_if.t_else, ordinal));
        } else if (stmt->nodetype == S_FOREACH) {
            CHKiRet(addActionNodes(builder, rulesetIdentity, stmt->d.s_foreach.body, ordinal));
        } else if (stmt->nodetype == S_RELOAD_PRIFILT || stmt->nodetype == S_PRIFILT) {
            CHKiRet(addActionNodes(builder, rulesetIdentity, stmt->d.s_prifilt.t_then, ordinal));
            CHKiRet(addActionNodes(builder, rulesetIdentity, stmt->d.s_prifilt.t_else, ordinal));
        } else if (stmt->nodetype == S_RELOAD_PROPFILT || stmt->nodetype == S_PROPFILT) {
            CHKiRet(addActionNodes(builder, rulesetIdentity, stmt->d.s_propfilt.t_then, ordinal));
            CHKiRet(addActionNodes(builder, rulesetIdentity, stmt->d.s_propfilt.t_else, ordinal));
        }
    }

finalize_it:
    free(identity);
    free(fingerprint);
    if (serialized != NULL) es_deleteStr(serialized);
    RETiRet;
}

rsRetVal rsReloadActionSyntaxFingerprintV1(const struct nvlst *syntax, char **ownedFingerprint) {
    es_str_t *serialized = NULL;
    DEFiRet;
    if (syntax == NULL || ownedFingerprint == NULL || *ownedFingerprint != NULL) return RS_RET_PARAM_ERROR;
    CHKmalloc(serialized = es_newStr(128));
    CHKiRet(appendToken(&serialized, "rsyslog.reload.action.v1;"));
    CHKiRet(appendNvlst(&serialized, syntax));
    CHKiRet(sha256Fingerprint(serialized, ownedFingerprint));
finalize_it:
    if (serialized != NULL) es_deleteStr(serialized);
    RETiRet;
}

rsRetVal rsReloadActionSyntaxNameV1(const struct nvlst *const syntax,
                                    const char **const borrowedName,
                                    size_t *const nameLength) {
    const char *name;
    size_t length;
    if (syntax == NULL || borrowedName == NULL || nameLength == NULL) return RS_RET_PARAM_ERROR;
    *borrowedName = NULL;
    *nameLength = 0;
    name = nvlstString(syntax, "name", &length);
    if (name == NULL) return RS_RET_NOT_IMPLEMENTED;
    *borrowedName = name;
    *nameLength = length;
    return RS_RET_OK;
}

rsRetVal rsReloadCandidateBuildNormalizedGraphV1(const rsReloadCandidate_t *candidate,
                                                 rsReloadNormalizedGraphBuilderV1_t **ppBuilder) {
    rsReloadNormalizedGraphBuilderV1_t *builder = NULL;
    const rsReloadCandidateObject_t *entry;
    size_t ordinal = 0;
    size_t defaultActionOrdinal = 0;
    int haveDefaultRuleset = 0;
    int haveGlobal = 0;
    int haveMainQueue = 0;
    char *identity = NULL;
    char *fingerprint = NULL;
    DEFiRet;

    if (candidate == NULL || ppBuilder == NULL || *ppBuilder != NULL || candidate->parseError)
        return RS_RET_PARAM_ERROR;
    CHKiRet(rsReloadNormalizedGraphBuilderV1Construct(&builder));
    for (entry = candidate->head; entry != NULL; entry = entry->next) {
        ++ordinal;
        if (entry->object->objType == CNFOBJ_GLOBAL) {
            haveGlobal = 1;
            continue;
        }
        if (entry->object->objType == CNFOBJ_MAINQ) {
            if (haveMainQueue) ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
            haveMainQueue = 1;
        }
        if (entry->object->objType == CNFOBJ_RULESET && entry->object->nvlst == NULL) {
            haveDefaultRuleset = 1;
            CHKiRet(addActionNodes(builder, "ruleset:default", entry->object->script, &defaultActionOrdinal));
            continue;
        }
        CHKiRet(makeIdentity(entry->object,
                             entry->object->objType == CNFOBJ_INPUT ? inputTypeOrdinal(candidate, entry) : ordinal,
                             &identity));
        if (entry->object->objType == CNFOBJ_RULESET) lowerRulesetIdentity(identity);
        CHKiRet(objectFingerprint(entry->object, &fingerprint));
        CHKiRet(rsReloadNormalizedGraphBuilderV1Add(builder, objectKind(entry->object), identity, fingerprint));
        if (entry->object->objType == CNFOBJ_RULESET) {
            size_t actionOrdinal = 0;
            CHKiRet(addActionNodes(builder, identity, entry->object->script, &actionOrdinal));
        }
        free(identity);
        identity = NULL;
        free(fingerprint);
        fingerprint = NULL;
    }
    if (haveDefaultRuleset) {
        CHKiRet(defaultRulesetFingerprint(candidate, &fingerprint));
        CHKiRet(rsReloadNormalizedGraphBuilderV1Add(builder, RS_RELOAD_OBJ_RULESET, "ruleset:default", fingerprint));
        free(fingerprint);
        fingerprint = NULL;
    }
    if (haveGlobal) {
        CHKiRet(globalFingerprint(candidate, &fingerprint));
        CHKiRet(rsReloadNormalizedGraphBuilderV1Add(builder, RS_RELOAD_OBJ_GLOBAL, "global", fingerprint));
        free(fingerprint);
        fingerprint = NULL;
    }
    *ppBuilder = builder;
    builder = NULL;

finalize_it:
    free(identity);
    free(fingerprint);
    rsReloadNormalizedGraphBuilderV1Destruct(&builder);
    RETiRet;
}

rsRetVal rsReloadCandidateCheckRulesetOnlyReportV1(const rsReloadReportV1_t *const report) {
    size_t i;
    if (report == NULL || report->version != RS_RELOAD_REPORT_V1 || report->structSize < sizeof(*report) ||
        report->entryStride < sizeof(rsReloadReportEntryV1_t) ||
        report->entryStride % _Alignof(rsReloadReportEntryV1_t) != 0)
        return RS_RET_PARAM_ERROR;
    for (i = 0; i < report->entryCount; ++i) {
        const uintptr_t address = (uintptr_t)(const void *)report->entries + i * report->entryStride;
        const rsReloadReportEntryV1_t *entry = (const rsReloadReportEntryV1_t *)(const void *)address;
        if (entry->diffKind == RS_RELOAD_DIFF_UNCHANGED) continue;
        /* Added/removed rulesets alter call targets. Any changed action,
         * input, parser, queue, module, template, or global fails closed. */
        if (entry->objectKind != RS_RELOAD_OBJ_RULESET || entry->diffKind != RS_RELOAD_DIFF_MODIFIED)
            return RS_RET_NOT_IMPLEMENTED;
    }
    return RS_RET_OK;
}

rsRetVal rsReloadCandidateSourceBegin(void) {
    rsReloadCandidateSourceAbort();
    sourceOrdinal = 0;
    sourceDefaultActionOrdinal = 0;
    sourceError = 0;
    sourceHaveGlobal = 0;
    sourceHaveMainQueue = 0;
    sourceInputOrdinalsDestruct();
    return rsReloadNormalizedGraphBuilderV1Construct(&sourceBuilder);
}

int rsReloadCandidateSourceActive(void) {
    return sourceBuilder != NULL && !sourceError;
}

void rsReloadCandidateSourceCaptureObject(const struct cnfobj *object) {
    char *identity = NULL;
    char *fingerprint = NULL;
    size_t identityOrdinal;
    rsRetVal ret;

    if (sourceBuilder == NULL || sourceError || object == NULL) return;
    ++sourceOrdinal;
    if (object->objType == CNFOBJ_RULESET && object->nvlst == NULL) {
        if (sourceDefaultSerialized == NULL) sourceDefaultSerialized = es_newStr(256);
        if (sourceDefaultSerialized == NULL ||
            (es_strlen(sourceDefaultSerialized) == 0 &&
             appendToken(&sourceDefaultSerialized, "rsyslog.reload.default-ruleset.v1;") != RS_RET_OK) ||
            appendObject(&sourceDefaultSerialized, object) != RS_RET_OK ||
            addActionNodes(sourceBuilder, "ruleset:default", object->script, &sourceDefaultActionOrdinal) !=
                RS_RET_OK) {
            sourceError = 1;
        }
        return;
    }
    /* Merge sequential global blocks into the same case-insensitive,
     * last-write map used by the candidate graph. */
    if (object->objType == CNFOBJ_GLOBAL) {
        struct nvlst *copy;
        sourceHaveGlobal = 1;
        if (validateNvlst(object->nvlst) != RS_RET_OK) {
            sourceError = 1;
            return;
        }
        copy = rsconfTranslateCloneNvlst(object->nvlst);
        if (object->nvlst != NULL && copy == NULL) {
            sourceError = 1;
            return;
        }
        if (sourceGlobalParamsTail == NULL)
            sourceGlobalParams = copy;
        else
            sourceGlobalParamsTail->next = copy;
        if (copy != NULL) {
            sourceGlobalParamsTail = copy;
            while (sourceGlobalParamsTail->next != NULL) sourceGlobalParamsTail = sourceGlobalParamsTail->next;
        }
        return;
    }
    if (object->objType == CNFOBJ_MAINQ) {
        if (sourceHaveMainQueue) {
            sourceError = 1;
            return;
        }
        sourceHaveMainQueue = 1;
    }
    identityOrdinal = sourceOrdinal;
    if (object->objType == CNFOBJ_INPUT) {
        ret = sourceInputTypeOrdinal(object, &identityOrdinal);
        if (ret != RS_RET_OK) {
            sourceError = 1;
            return;
        }
    }
    ret = makeIdentity(object, identityOrdinal, &identity);
    if (ret == RS_RET_OK && object->objType == CNFOBJ_RULESET) lowerRulesetIdentity(identity);
    if (ret == RS_RET_OK) ret = objectFingerprint(object, &fingerprint);
    if (ret == RS_RET_OK)
        ret = rsReloadNormalizedGraphBuilderV1Add(sourceBuilder, objectKind(object), identity, fingerprint);
    if (ret == RS_RET_OK && object->objType == CNFOBJ_RULESET) {
        size_t actionOrdinal = 0;
        ret = addActionNodes(sourceBuilder, identity, object->script, &actionOrdinal);
    }
    free(identity);
    free(fingerprint);
    if (ret != RS_RET_OK) sourceError = 1;
}

void rsReloadCandidateSourceCaptureDefaultScript(const struct cnfstmt *script) {
    struct cnfobj object = {
        .objType = CNFOBJ_RULESET, .nvlst = NULL, .subobjs = NULL, .script = (struct cnfstmt *)script};

    rsReloadCandidateSourceCaptureObject(&object);
}

rsRetVal rsReloadCandidateSourceFinish(rsReloadNormalizedGraphBuilderV1_t **ppBuilder) {
    char *fingerprint = NULL;
    rsRetVal ret;

    if (ppBuilder == NULL || *ppBuilder != NULL || sourceBuilder == NULL) return RS_RET_PARAM_ERROR;
    if (sourceError) {
        rsReloadCandidateSourceAbort();
        return RS_RET_NOT_IMPLEMENTED;
    }
    if (sourceDefaultSerialized != NULL) {
        ret = sha256Fingerprint(sourceDefaultSerialized, &fingerprint);
        if (ret == RS_RET_OK)
            ret = rsReloadNormalizedGraphBuilderV1Add(sourceBuilder, RS_RELOAD_OBJ_RULESET, "ruleset:default",
                                                      fingerprint);
        free(fingerprint);
        fingerprint = NULL;
        if (ret != RS_RET_OK) {
            rsReloadCandidateSourceAbort();
            return ret;
        }
    }
    if (sourceHaveGlobal) {
        es_str_t *serialized = es_newStr(256);
        if (serialized == NULL)
            ret = RS_RET_OUT_OF_MEMORY;
        else {
            ret = appendToken(&serialized, "rsyslog.reload.global.v1;");
            if (ret == RS_RET_OK) ret = appendGlobalMap(&serialized, sourceGlobalParams);
            if (ret == RS_RET_OK) ret = sha256Fingerprint(serialized, &fingerprint);
            es_deleteStr(serialized);
        }
        if (ret == RS_RET_OK)
            ret = rsReloadNormalizedGraphBuilderV1Add(sourceBuilder, RS_RELOAD_OBJ_GLOBAL, "global", fingerprint);
        free(fingerprint);
        fingerprint = NULL;
        if (ret != RS_RET_OK) {
            rsReloadCandidateSourceAbort();
            return ret;
        }
    }
    *ppBuilder = sourceBuilder;
    sourceBuilder = NULL;
    if (sourceDefaultSerialized != NULL) es_deleteStr(sourceDefaultSerialized);
    sourceDefaultSerialized = NULL;
    nvlstDestruct(sourceGlobalParams);
    sourceGlobalParams = NULL;
    sourceGlobalParamsTail = NULL;
    sourceHaveGlobal = 0;
    sourceHaveMainQueue = 0;
    sourceInputOrdinalsDestruct();
    return RS_RET_OK;
}

void rsReloadCandidateSourceAbort(void) {
    rsReloadNormalizedGraphBuilderV1Destruct(&sourceBuilder);
    if (sourceDefaultSerialized != NULL) es_deleteStr(sourceDefaultSerialized);
    sourceDefaultSerialized = NULL;
    nvlstDestruct(sourceGlobalParams);
    sourceGlobalParams = NULL;
    sourceGlobalParamsTail = NULL;
    sourceHaveGlobal = 0;
    sourceError = 0;
    sourceHaveMainQueue = 0;
    sourceInputOrdinalsDestruct();
}

static void objectDestruct(struct cnfobj *const object) {
    if (object == NULL) return;
    cnfstmtDestructLst(object->script);
    object->script = NULL;
    cnfobjDestruct(object);
}

void rsReloadCandidateDestruct(rsReloadCandidate_t **const ppCandidate) {
    rsReloadCandidateObject_t *entry;
    rsReloadCandidateObject_t *next;

    if (ppCandidate == NULL || *ppCandidate == NULL) return;
    for (entry = (*ppCandidate)->head; entry != NULL; entry = next) {
        next = entry->next;
        objectDestruct(entry->object);
        free(entry);
    }
    free(*ppCandidate);
    *ppCandidate = NULL;
}

int rsReloadCandidateCaptureActive(void) {
    return captureCandidate != NULL;
}

rsRetVal rsReloadCandidateTakeObject(struct cnfobj *const object) {
    rsReloadCandidateObject_t *entry;

    if (captureCandidate == NULL || object == NULL) return RS_RET_PARAM_ERROR;
    entry = calloc(1, sizeof(*entry));
    if (entry == NULL) return RS_RET_OUT_OF_MEMORY;
    entry->object = object;
    if (captureCandidate->tail == NULL) {
        captureCandidate->head = entry;
    } else {
        captureCandidate->tail->next = entry;
    }
    captureCandidate->tail = entry;
    ++captureCandidate->objectCount;
    return RS_RET_OK;
}

rsRetVal rsReloadCandidateTakeDefaultScript(struct cnfstmt *const script) {
    struct cnfobj *object;
    rsRetVal ret;

    if (captureCandidate == NULL || script == NULL) return RS_RET_PARAM_ERROR;
    object = cnfobjNew(CNFOBJ_RULESET, NULL);
    if (object == NULL) return RS_RET_OUT_OF_MEMORY;
    object->script = script;
    ret = rsReloadCandidateTakeObject(object);
    if (ret != RS_RET_OK) {
        object->script = NULL;
        cnfobjDestruct(object);
    }
    return ret;
}

void rsReloadCandidateNoteParseError(void) {
    if (captureCandidate != NULL) captureCandidate->parseError = 1;
}

static int candidateErrorPriority(const rsRetVal error) {
    if (error == RS_RET_OUT_OF_MEMORY) return 3;
    if (error == RS_RET_IO_ERROR || error == RS_RET_FILE_OPEN_ERROR || error == RS_RET_CONF_FILE_NOT_FOUND ||
        error == RS_RET_FILE_NOT_FOUND || error == RS_RET_FILENAME_INVALID) {
        return 2;
    }
    return error == RS_RET_OK ? 0 : 1;
}

void rsReloadCandidateNoteError(const rsRetVal error) {
    if (captureCandidate != NULL &&
        candidateErrorPriority(error) > candidateErrorPriority(captureCandidate->captureError)) {
        captureCandidate->captureError = error;
    }
}

size_t rsReloadCandidateObjectCount(const rsReloadCandidate_t *const candidate) {
    return candidate == NULL ? 0 : candidate->objectCount;
}

typedef struct visitedRulesetIdentity_s {
    char *identity;
    int syntheticDefault;
    struct visitedRulesetIdentity_s *next;
} visitedRulesetIdentity_t;

static void visitedRulesetIdentitiesDestruct(visitedRulesetIdentity_t *entry) {
    while (entry != NULL) {
        visitedRulesetIdentity_t *const next = entry->next;
        free(entry->identity);
        free(entry);
        entry = next;
    }
}

rsRetVal rsReloadCandidateVisitRulesetFragmentsV1(const rsReloadCandidate_t *candidate,
                                                  rsReloadCandidateRulesetFragmentVisitorV1_t visitor,
                                                  void *context) {
    const rsReloadCandidateObject_t *entry;
    visitedRulesetIdentity_t *visited = NULL;
    char *identity = NULL;
    DEFiRet;
    if (candidate == NULL || visitor == NULL) return RS_RET_PARAM_ERROR;
    for (entry = candidate->head; entry != NULL; entry = entry->next) {
        const struct cnfobj *object = entry->object;
        visitedRulesetIdentity_t *seen;
        int first = 1;
        const int isSyntheticDefault = object != NULL && object->objType == CNFOBJ_RULESET && object->nvlst == NULL;
        if (object == NULL || object->objType != CNFOBJ_RULESET) continue;
        CHKiRet(makeIdentity(object, 0, &identity));
        lowerRulesetIdentity(identity);
        for (seen = visited; seen != NULL; seen = seen->next) {
            if (strcmp(seen->identity, identity)) continue;
            if (!isSyntheticDefault || !seen->syntheticDefault) {
                ABORT_FINALIZE(RS_RET_DUP_PARAM);
            }
            first = 0;
            break;
        }
        if (seen == NULL) {
            CHKmalloc(seen = calloc(1, sizeof(*seen)));
            seen->identity = identity;
            seen->syntheticDefault = isSyntheticDefault;
            identity = NULL;
            seen->next = visited;
            visited = seen;
        }
        free(identity);
        identity = NULL;
        CHKiRet(visitor(seen->identity, object->script, first, isSyntheticDefault, context));
    }
finalize_it:
    free(identity);
    visitedRulesetIdentitiesDestruct(visited);
    RETiRet;
}

rsRetVal rsReloadCandidateParse(const char *const path, rsReloadCandidate_t **const ppCandidate) {
    const char *ext;
    int parseResult;
    rsRetVal frontendResult = RS_RET_OK;
    struct stat pathStat;
    rsReloadCandidate_t *candidate = NULL;
    DEFiRet;

    if (path == NULL || ppCandidate == NULL || *ppCandidate != NULL || captureCandidate != NULL) {
        return RS_RET_PARAM_ERROR;
    }
    if (stat(path, &pathStat) != 0) {
        return errno == ENOENT || errno == ENOTDIR ? RS_RET_CONF_FILE_NOT_FOUND : RS_RET_FILE_OPEN_ERROR;
    }
    if (!S_ISREG(pathStat.st_mode)) return RS_RET_FILENAME_INVALID;
    CHKmalloc(candidate = calloc(1, sizeof(*candidate)));
    captureCandidate = candidate;
    cnfClearFatalParseError();
    ext = strrchr(path, '.');
#ifdef HAVE_LIBYAML
    if (ext != NULL && (!strcmp(ext, ".yaml") || !strcmp(ext, ".yml"))) {
        frontendResult = yamlconf_load(path);
        parseResult = frontendResult == RS_RET_OK ? 0 : 1;
        if (parseResult == 0 && cnfHasPendingBuffers()) {
            parseResult = yyparse();
            if (parseResult == 2)
                frontendResult = RS_RET_OUT_OF_MEMORY;
            else if (parseResult != 0)
                frontendResult = RS_RET_CONF_PARSE_ERROR;
        }
    } else {
#else
    if (ext != NULL && (!strcmp(ext, ".yaml") || !strcmp(ext, ".yml"))) {
        parseResult = 1;
    } else {
#endif
        parseResult = cnfSetLexFile(path);
        if (parseResult == 2) {
            frontendResult = errno == ENOENT || errno == ENOTDIR ? RS_RET_CONF_FILE_NOT_FOUND : RS_RET_FILE_OPEN_ERROR;
        } else if (parseResult == 3) {
            frontendResult = RS_RET_OUT_OF_MEMORY;
        } else if (parseResult == 4) {
            frontendResult = RS_RET_IO_ERROR;
        }
        if (parseResult == 0) {
            parseResult = yyparse();
            if (parseResult == 2) frontendResult = RS_RET_OUT_OF_MEMORY;
        }
    }
    {
        const rsRetVal fatalParseRet = cnfTakeFatalParseError();
        if ((frontendResult == RS_RET_OK || frontendResult == RS_RET_CONF_PARSE_ERROR) && fatalParseRet != RS_RET_OK) {
            frontendResult = fatalParseRet;
        }
    }
    cnfResetParser();
    captureCandidate = NULL;
    if (candidate->captureError != RS_RET_OK) ABORT_FINALIZE(candidate->captureError);
    if (frontendResult != RS_RET_OK) ABORT_FINALIZE(frontendResult);
    if (parseResult != 0 || candidate->parseError) ABORT_FINALIZE(RS_RET_CONF_PARSE_ERROR);
    *ppCandidate = candidate;
    candidate = NULL;

finalize_it:
    captureCandidate = NULL;
    rsReloadCandidateDestruct(&candidate);
    RETiRet;
}
