/* lib_ksils12.c - rsyslog's KSI-LS12 support library
 *
 * Regarding the online algorithm for Merkle tree signing. Expected
 * calling sequence is:
 *
 * sigblkConstruct
 * for each signature block:
 *    sigblkInitKSI
 *    for each record:
 *       sigblkAddRecordKSI
 *    sigblkFinishKSI
 * sigblkDestruct
 *
 * Obviously, the next call after sigblkFinsh must either be to
 * sigblkInitKSI or sigblkDestruct (if no more signature blocks are
 * to be emitted, e.g. on file close). sigblkDestruct saves state
 * information (most importantly last block hash) and sigblkConstruct
 * reads (or initilizes if not present) it.
 *
 * Copyright 2013-2018 Adiscon GmbH and Guardtime, Inc.
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <limits.h>

#include <ksi/ksi.h>
#include <ksi/tlv_element.h>
#include <ksi/hash.h>
#include <ksi/net_async.h>
#include <ksi/net_ha.h>
#include <ksi/net_uri.h>
#include <ksi/signature_builder.h>
#include "rsyslog.h"
#include "errmsg.h"
#include "lib_ksils12.h"
#include "lib_ksi_queue.h"

#ifndef VERSION
#define VERSION "no-version"
#endif

#define KSI_BUF_SIZE 4096

static const char *blockFileSuffix = ".logsig.parts/blocks.dat";
static const char *sigFileSuffix = ".logsig.parts/block-signatures.dat";
static const char *ls12FileSuffix = ".logsig";
static const char *blockCloseReason = "com.guardtime.blockCloseReason";


#define LS12_FILE_HEADER "LOGSIG12"
#define LS12_BLOCKFILE_HEADER "LOG12BLK"
#define LS12_SIGFILE_HEADER "LOG12SIG"
#define LS12_SIGNATURE_TIMEOUT 60

/* Worker queue item type identifier */
typedef enum QITEM_type_en {
    QITEM_SIGNATURE_REQUEST = 0x00,
    QITEM_CLOSE_FILE,
    QITEM_NEW_FILE,
    QITEM_QUIT
} QITEM_type;

/* Worker queue item status identifier */
typedef enum QITEM_status_en {
    /* State assigned to any item added to queue (initial state). */
    QITEM_WAITING = 0x00,

    /* State assigned to #QITEM_SIGNATURE_REQUEST item when it is sent out. */
    QITEM_SENT,

    /* State assigned to #QITEM_SIGNATURE_REQUEST item when request failed or succeeded. */
    QITEM_DONE
} QITEM_status;


/* Worker queue job item */
typedef struct QueueItem_st {
    QITEM_type type;
    QITEM_status status;
    KSI_DataHash *root;
    FILE *file;             /* To keep track of the target signature file. */
    uint64_t intarg1;       /* Block time limit or record count or not used. */
    uint64_t intarg2;       /* Level of the sign request or not used. */
    KSI_AsyncHandle *respHandle;
    int ksi_status;
    time_t request_time;
} QueueItem;

static bool queueAddCloseFile(rsksictx ctx, ksifile kf);
static bool queueAddNewFile(rsksictx ctx, ksifile kf);
static bool queueAddQuit(rsksictx ctx);
static bool queueAddSignRequest(rsksictx ctx, ksifile kf, KSI_DataHash *root, unsigned level);
static int sigblkFinishKSINoSignature(ksifile ksi, const char *reason);

void *signer_thread(void *arg);

static void __attribute__((format(printf, 2, 3)))
report(rsksictx ctx, const char *errmsg, ...) {
    char buf[1024];
    int r;
    va_list args;
    va_start(args, errmsg);

    r = vsnprintf(buf, sizeof (buf), errmsg, args);
    buf[sizeof(buf)-1] = '\0';
    va_end(args);

    if(ctx->logFunc == NULL)
        return;

    if(r>0 && r<(int)sizeof(buf))
        ctx->logFunc(ctx->usrptr, (uchar*)buf);
    else
        ctx->logFunc(ctx->usrptr, (uchar*)errmsg);
}

static void
reportErr(rsksictx ctx, const char *const errmsg)
{
    if(ctx->errFunc == NULL)
        goto done;
    ctx->errFunc(ctx->usrptr, (uchar*)errmsg);
done:   return;
}

static const char *
level2str(int level) {
    switch (level) {
    case KSI_LOG_DEBUG: return "DEBUG";
    case KSI_LOG_INFO: return "INFO";
    case KSI_LOG_NOTICE: return "NOTICE";
    case KSI_LOG_WARN: return "WARN";
    case KSI_LOG_ERROR: return "ERROR";
    default: return "UNKNOWN LOG LEVEL";
    }
}

void
reportKSIAPIErr(rsksictx ctx, ksifile ksi, const char *apiname, int ecode)
{
    char errbuf[4096];
    char ksi_errbuf[4096];
    KSI_ERR_getBaseErrorMessage(ctx->ksi_ctx, ksi_errbuf, sizeof(ksi_errbuf), NULL, NULL);
    snprintf(errbuf, sizeof(errbuf), "%s[%s:%d]: %s (%s)",
        (ksi == NULL) ? (uchar*) "" : ksi->blockfilename,
        apiname, ecode, KSI_getErrorString(ecode), ksi_errbuf);

    errbuf[sizeof(errbuf)-1] = '\0';
    reportErr(ctx, errbuf);
}

void
rsksisetErrFunc(rsksictx ctx, void (*func)(void*, uchar *), void *usrptr)
{
    ctx->usrptr = usrptr;
    ctx->errFunc = func;
}

void
rsksisetLogFunc(rsksictx ctx, void (*func)(void*, uchar *), void *usrptr)
{
    ctx->usrptr = usrptr;
    ctx->logFunc = func;
}

static ksifile
rsksifileConstruct(rsksictx ctx) {
    ksifile ksi = NULL;
    if ((ksi = calloc(1, sizeof (struct ksifile_s))) == NULL)
        goto done;
    ksi->ctx = ctx;
    ksi->hashAlg = ctx->hashAlg;
    ksi->blockTimeLimit = ctx->blockTimeLimit;
    ksi->blockSizeLimit = 1 << (ctx->effectiveBlockLevelLimit - 1);
    ksi->bKeepRecordHashes = ctx->bKeepRecordHashes;
    ksi->bKeepTreeHashes = ctx->bKeepTreeHashes;
    ksi->lastLeaf[0] = ctx->hashAlg;

done:
    return ksi;
}

/* return the actual length in to-be-written octets of an integer */
static uint8_t
tlvGetIntSize(uint64_t val) {
    uint8_t n = 0;
    while (val != 0) {
        val >>= 8;
        n++;
    }
    return n;
}

static int
tlvWriteOctetString(FILE *f, const uint8_t *data, uint16_t len) {
    if (fwrite(data, len, 1, f) != 1)
        return RSGTE_IO;
    return 0;
}

static int
tlvWriteHeader8(FILE *f, int flags, uint8_t tlvtype, int len) {
    unsigned char buf[2];
    assert((flags & RSGT_TYPE_MASK) == 0);
    assert((tlvtype & RSGT_TYPE_MASK) == tlvtype);
    buf[0] = (flags & ~RSGT_FLAG_TLV16) | tlvtype;
    buf[1] = len & 0xff;

    return tlvWriteOctetString(f, buf, 2);
}

static int
tlvWriteHeader16(FILE *f, int flags, uint16_t tlvtype, uint16_t len)
{
    uint16_t typ;
    unsigned char buf[4];
    assert((flags & RSGT_TYPE_MASK) == 0);
    assert((tlvtype >> 8 & RSGT_TYPE_MASK) == (tlvtype >> 8));
    typ = ((flags | RSGT_FLAG_TLV16) << 8) | tlvtype;

    buf[0] = typ >> 8;
    buf[1] = typ & 0xff;
    buf[2] = (len >> 8) & 0xff;
    buf[3] = len & 0xff;

    return tlvWriteOctetString(f, buf, 4);
}

static int
tlvGetHeaderSize(uint16_t tag, size_t size) {
    if (tag <= RSGT_TYPE_MASK && size <= 0xff)
        return 2;
    if ((tag >> 8) <= RSGT_TYPE_MASK && size <= 0xffff)
        return 4;
    return 0;
}

static int
tlvWriteHeader(FILE *f, int flags, uint16_t tlvtype, uint16_t len) {
    int headersize = tlvGetHeaderSize(tlvtype, flags);
    if (headersize == 2)
        return tlvWriteHeader8(f, flags, tlvtype, len);
    else if (headersize == 4)
        return tlvWriteHeader16(f, flags, tlvtype, len);
    else
        return 0;
}

static int
tlvWriteOctetStringTLV(FILE *f, int flags, uint16_t tlvtype, const uint8_t *data, uint16_t len) {
    if (tlvWriteHeader(f, flags, tlvtype, len) != 0)
        return RSGTE_IO;

    if (fwrite(data, len, 1, f) != 1)
        return RSGTE_IO;

    return 0;
}

static int
tlvWriteInt64TLV(FILE *f, int flags, uint16_t tlvtype, uint64_t val) {
    unsigned char buf[8];
    uint8_t count = tlvGetIntSize(val);
    uint64_t nTmp;

    if (tlvWriteHeader(f, flags, tlvtype, count) != 0)
        return RSGTE_IO;

    nTmp = val;
    for (int i = count - 1; i >= 0; i--) {
        buf[i] = 0xFF & nTmp;
        nTmp = nTmp >> 8;
    }

    if (fwrite(buf, count, 1, f) != 1)
        return RSGTE_IO;

    return 0;
}

static int
tlvWriteHashKSI(ksifile ksi, uint16_t tlvtype, KSI_DataHash *rec) {
    int r;
    const unsigned char *imprint;
    size_t imprint_len;
    r = KSI_DataHash_getImprint(rec, &imprint, &imprint_len);
    if (r != KSI_OK) {
        reportKSIAPIErr(ksi->ctx, ksi, "KSI_DataHash_getImprint", r);
        return r;
    }

    return tlvWriteOctetStringTLV(ksi->blockFile, 0, tlvtype, imprint, imprint_len);
}

static int
tlvWriteBlockHdrKSI(ksifile ksi) {
    unsigned tlvlen;
    uint8_t hash_algo = ksi->hashAlg;
    int r;

    tlvlen  = 2 + 1 /* hash algo TLV */ +
        2 + KSI_getHashLength(ksi->hashAlg) /* iv */ +
        2 + KSI_getHashLength(ksi->lastLeaf[0]) + 1;
    /* last hash */;

    /* write top-level TLV object block-hdr */
    CHKr(tlvWriteHeader(ksi->blockFile, 0x00, 0x0901, tlvlen));

    /* hash-algo */
    CHKr(tlvWriteOctetStringTLV(ksi->blockFile, 0x00, 0x01, &hash_algo, 1));

    /* block-iv */
    CHKr(tlvWriteOctetStringTLV(ksi->blockFile, 0x00, 0x02,
        ksi->IV, KSI_getHashLength(ksi->hashAlg)));

    /* last-hash */
    CHKr(tlvWriteOctetStringTLV(ksi->blockFile, 0x00, 0x03,
        ksi->lastLeaf, KSI_getHashLength(ksi->lastLeaf[0]) + 1));
done:
    return r;
}

static int
tlvWriteKSISigLS12(FILE *outfile, size_t record_count, uchar *der, uint16_t lenDer) {
    int r = 0;
    int totalSize = 2 + tlvGetIntSize(record_count) + 4 + lenDer;

    CHKr(tlvWriteHeader(outfile, 0x00, 0x0904, totalSize));
    CHKr(tlvWriteInt64TLV(outfile, 0x00, 0x01, record_count));
    CHKr(tlvWriteOctetStringTLV(outfile, 0x00, 0x0905, der, lenDer));
done:
    return r;
}

static int
tlvWriteNoSigLS12(FILE *outfile, size_t record_count, const KSI_DataHash *hash, const char *errorText) {
    int r = 0;
    int totalSize = 0;
    int noSigSize = 0;
    const unsigned char *imprint = NULL;
    size_t imprintLen = 0;

    KSI_DataHash_getImprint(hash, &imprint, &imprintLen);

    noSigSize = 2 + imprintLen + (errorText ? (2 + strlen(errorText) + 1) : 0);
    totalSize = 2 + tlvGetIntSize(record_count) + 2 + noSigSize;

    CHKr(tlvWriteHeader(outfile, 0x00, 0x0904, totalSize));
    CHKr(tlvWriteInt64TLV(outfile, 0x00, 0x01, record_count));
    CHKr(tlvWriteHeader(outfile, 0x00, 0x02, noSigSize));
    CHKr(tlvWriteOctetStringTLV(outfile, 0x00, 0x01, imprint, imprintLen));
    if (errorText)
        CHKr(tlvWriteOctetStringTLV(outfile, 0x00, 0x02, (uint8_t*) errorText, strlen(errorText) + 1));
done:
    return r;
}

static int
tlvCreateMetadata(ksifile ksi, uint64_t record_index, const char *key,
        const char *value, unsigned char *buffer, size_t *len) {
    int r = 0;
    KSI_TlvElement *metadata = NULL, *attrib_tlv = NULL;
    KSI_Utf8String *key_tlv = NULL, *value_tlv = NULL;
    KSI_Integer *index_tlv = NULL;

    CHKr(KSI_TlvElement_new(&metadata));
    metadata->ftlv.tag = 0x0911;

    CHKr(KSI_Integer_new(ksi->ctx->ksi_ctx, record_index, &index_tlv));
    CHKr(KSI_TlvElement_setInteger(metadata, 0x01, index_tlv));

    CHKr(KSI_TlvElement_new(&attrib_tlv));
    attrib_tlv->ftlv.tag = 0x02;

    CHKr(KSI_Utf8String_new(ksi->ctx->ksi_ctx, key, strlen(key) + 1, &key_tlv));
    CHKr(KSI_TlvElement_setUtf8String(attrib_tlv, 0x01, key_tlv));

    CHKr(KSI_Utf8String_new(ksi->ctx->ksi_ctx, value, strlen(value) + 1, &value_tlv));
    CHKr(KSI_TlvElement_setUtf8String(attrib_tlv, 0x02, value_tlv));

    CHKr(KSI_TlvElement_setElement(metadata, attrib_tlv));

    CHKr(KSI_TlvElement_serialize(metadata, buffer, 0xFFFF, len, 0));

done:
    if (metadata) KSI_TlvElement_free(metadata);
    if (attrib_tlv) KSI_TlvElement_free(attrib_tlv);
    if (key_tlv) KSI_Utf8String_free(key_tlv);
    if (value_tlv) KSI_Utf8String_free(value_tlv);
    if (index_tlv) KSI_Integer_free(index_tlv);

    return r;
}

#define KSI_FILE_AMOUNT_INC 32

static int
rsksiExpandRegisterIfNeeded(rsksictx ctx, size_t inc) {
    int ret = RSGTE_INTERNAL;
    ksifile *tmp = NULL;

    if (ctx == NULL || inc == 0) {
        return RSGTE_INTERNAL;
    }

    if (ctx->ksiCount < ctx->ksiCapacity) {
        return RSGTE_SUCCESS;
    }

    /* If needed allocate memory for the buffer. */
    tmp = (ksifile*)realloc(ctx->ksi,  sizeof(ksifile) * (ctx->ksiCapacity + inc));
    if (tmp == NULL) {
        ret = RSGTE_OOM;
        goto done;
    }

    /* Make sure that allocated pointers are all set to NULL. */
    memset(tmp + ctx->ksiCapacity, 0, sizeof(ksifile) * inc);

    /* Update buffer capacity. */
    ctx->ksiCapacity += inc;
    ctx->ksi = tmp;
    tmp = NULL;
    ret = RSGTE_SUCCESS;

done:
    free(tmp);
    return ret;
}

static int
rsksiRegisterKsiFile(rsksictx ctx, ksifile ksi) {
    int ret = RSGTE_INTERNAL;

    if (ctx == NULL || ksi == NULL) {
        return RSGTE_INTERNAL;
    }

    /* To be extra sure that ksifile buffer is initialized correctly, clear variables. */
    if (ctx->ksi == NULL) {
        ctx->ksiCount = 0;
        ctx->ksiCapacity = 0;
    }

    ret = rsksiExpandRegisterIfNeeded(ctx, KSI_FILE_AMOUNT_INC);
    if (ret != RSGTE_SUCCESS) goto done;

    ctx->ksi[ctx->ksiCount] = ksi;
    ctx->ksiCount++;
    ret = RSGTE_SUCCESS;

done:
    return ret;
}

static int
rsksiDeregisterKsiFile(rsksictx ctx, ksifile ksi) {
    int ret = RSGTE_INTERNAL;
    size_t i = 0;

    if (ctx == NULL || ksi == NULL) {
        return RSGTE_INTERNAL;
    }


    for (i = 0; i < ctx->ksiCount; i++) {
        if (ctx->ksi[i] != NULL && ctx->ksi[i] == ksi) {
            size_t lastElement = ctx->ksiCount - 1;

            if (i != lastElement) {
                ctx->ksi[i] = ctx->ksi[lastElement];
            }

            ctx->ksi[lastElement] = NULL;

            ctx->ksiCount--;
            ret = RSGTE_SUCCESS;
            goto done;
        }
    }

done:
    return ret;
}

/* support for old platforms - graceful degrade */
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
/* read rsyslog log state file; if we cannot access it or the
 * contents looks invalid, we flag it as non-present (and thus
 * begin a new hash chain).
 * The context is initialized accordingly.
 */
static bool
ksiReadStateFile(ksifile ksi) {
    int fd = -1;
    struct rsksistatefile sf;
    bool ret = false;

    fd = open((char*)ksi->statefilename, O_RDONLY|O_NOCTTY|O_CLOEXEC, 0600);
    if (fd == -1)
        goto done;

    if (read(fd, &sf, sizeof (sf)) != sizeof (sf)) goto done;
    if (strncmp(sf.hdr, "KSISTAT10", 9)) goto done;

    if (KSI_getHashLength(sf.hashID) != sf.lenHash ||
        KSI_getHashLength(sf.hashID) > KSI_MAX_IMPRINT_LEN - 1)
        goto done;

    if (read(fd, ksi->lastLeaf + 1, sf.lenHash) != sf.lenHash)
        goto done;

    ksi->lastLeaf[0] = sf.hashID;
    ret = true;

done:
    if (!ret) {
        memset(ksi->lastLeaf, 0, sizeof (ksi->lastLeaf));
        ksi->lastLeaf[0] = ksi->hashAlg;
    }

    if (fd != -1)
        close(fd);
    return ret;
}

/* persist all information that we need to re-open and append
 * to a log signature file.
 */
static void
ksiWwriteStateFile(ksifile ksi)
{
    int fd;
    struct rsksistatefile sf;

    fd = open((char*)ksi->statefilename,
               O_WRONLY|O_CREAT|O_TRUNC|O_NOCTTY|O_CLOEXEC, ksi->ctx->fCreateMode);
    if(fd == -1)
        goto done;
    if (ksi->ctx->fileUID != (uid_t) - 1 || ksi->ctx->fileGID != (gid_t) - 1) {
        /* we need to set owner/group */
        if (fchown(fd, ksi->ctx->fileUID, ksi->ctx->fileGID) != 0) {
            report(ksi->ctx, "lmsig_ksi: chown for file '%s' failed: %s",
                ksi->statefilename, strerror(errno));
        }
    }

    memcpy(sf.hdr, "KSISTAT10", 9);
    sf.hashID = ksi->hashAlg;
    sf.lenHash = KSI_getHashLength(ksi->lastLeaf[0]);
    /* if the write fails, we cannot do anything against that. We check
     * the condition just to keep the compiler happy.
     */
    if(write(fd, &sf, sizeof(sf))){};
    if (write(fd, ksi->lastLeaf + 1, sf.lenHash)) {
    };
    close(fd);
done:
    return;
}


static int
ksiCloseSigFile(ksifile ksi) {
    fclose(ksi->blockFile);
    ksi->blockFile = NULL;
    if (ksi->ctx->syncMode == LOGSIG_ASYNCHRONOUS)
        queueAddCloseFile(ksi->ctx, ksi);

    ksiWwriteStateFile(ksi);
    return 0;
}

static int mkpath(char* path, mode_t mode, uid_t uid, gid_t gid) {

    if(path == NULL)
        return 1;

    for (char *p = strchr(path + 1, '/'); p; p = strchr(p + 1, '/')) {
        *p = '\0';
        if (mkdir(path, mode) == 0) {
            if (uid != (uid_t) -1 || gid != (uid_t) -1) {
                if (chown(path, uid, gid)) {
                    LogError(errno, RS_RET_IO_ERROR,
                        "ksils12 signatures: could not change to "
                        "configured owner - files may be unaccessible");
                }
            }
        }
        else if (errno != EEXIST) {
            *p = '/';
            return -1;
        }

        *p = '/';
    }
    return 0;
}

static FILE*
ksiCreateFile(rsksictx ctx, const char *path, uid_t uid, gid_t gid, int mode, bool lockit, const char* header) {
    int fd = -1;
    struct stat stat_st;
    FILE *f = NULL;
    struct flock lock = {F_WRLCK, SEEK_SET, 0, 0, 0};

    if(path ==NULL)
        return NULL;

    if (mkpath((char*) path, ctx->fDirCreateMode, ctx->dirUID, ctx->dirGID) != 0) {
        report(ctx, "ksiCreateFile: mkpath failed for %s", path);
        goto done;
    }

    fd = open(path, O_RDWR | O_APPEND | O_NOCTTY | O_CLOEXEC, 0600);
    if (fd == -1) {
        fd = open(path, O_RDWR | O_CREAT | O_NOCTTY | O_CLOEXEC, mode);
        if (fd == -1) {
            report(ctx, "creating file '%s' failed: %s", path, strerror(errno));
            goto done;
        }

        if (uid != (uid_t) - 1 || gid != (gid_t) - 1) {
            if (fchown(fd, uid, gid) != 0) {
                report(ctx, "lmsig_ksi: chown for file '%s' failed: %s",
                    path, strerror(errno));
            }
        }
    }

    if (lockit && fcntl(fd, F_SETLK, &lock) != 0)
        report(ctx, "fcntl error: %s", strerror(errno));

    f = fdopen(fd, "a");
    if (f == NULL) {
        report(ctx, "fdopen for '%s' failed: %s", path, strerror(errno));
        goto done;
    }

    setvbuf(f, NULL, _IOFBF, KSI_BUF_SIZE);

    if (fstat(fd, &stat_st) == -1) {
        reportErr(ctx, "ksiOpenSigFile: can not stat file");
        goto done;
    }

    if (stat_st.st_size == 0 && header != NULL) {
        if(fwrite(header, strlen(header), 1, f) != 1) {
            report(ctx, "ksiOpenSigFile: fwrite for file %s failed: %s",
                path, strerror(errno));
            goto done;
        }
    }
    /* Write header immediately as when using dynafile it is possible that the same
     * file is opened 2x in sequence (caused by small dynafile cache where files are
     * frequently closed and reopened). If the header already exists double header is
     * not written. The content of the file is ordered by signer thread.
     */
    fflush(f);
done:
    return f;
}

static void handle_ksi_config(rsksictx ctx, KSI_AsyncService *as, KSI_Config *config) {
    int res = KSI_UNKNOWN_ERROR;
    KSI_Integer *intValue = NULL;

    if (KSI_Config_getMaxRequests(config, &intValue) == KSI_OK && intValue != NULL) {
        ctx->max_requests = KSI_Integer_getUInt64(intValue);
        report(ctx, "KSI gateway has reported a max requests value of %llu",
            (long long unsigned) ctx->max_requests);

        if(as) {
            /* libksi expects size_t. */
            size_t optValue = 0;

            optValue = ctx->max_requests;
            res = KSI_AsyncService_setOption(as, KSI_ASYNC_OPT_MAX_REQUEST_COUNT,
                (void*)optValue);
            if(res != KSI_OK)
                reportKSIAPIErr(ctx, NULL, "KSI_AsyncService_setOption(max_request)", res);

            optValue = 3 * ctx->max_requests * ctx->blockSigTimeout;
            KSI_AsyncService_setOption(as, KSI_ASYNC_OPT_REQUEST_CACHE_SIZE,
                (void*)optValue);
        }
    }

    intValue = NULL;
    if(KSI_Config_getMaxLevel(config, &intValue) == KSI_OK && intValue != NULL) {
        uint64_t newLevel = 0;
        newLevel = KSI_Integer_getUInt64(intValue);
        report(ctx, "KSI gateway has reported a max level value of %llu",
            (long long unsigned) newLevel);
        newLevel=MIN(newLevel, ctx->blockLevelLimit);
        if(ctx->effectiveBlockLevelLimit != newLevel) {
            report(ctx, "Changing the configured block level limit from %llu to %llu",
            (long long unsigned) ctx->effectiveBlockLevelLimit,
            (long long unsigned) newLevel);
            ctx->effectiveBlockLevelLimit = newLevel;
        }
        else if(newLevel < 2) {
            report(ctx, "KSI gateway has reported an invalid level limit value (%llu), "
                "plugin disabled", (long long unsigned) newLevel);
            ctx->disabled = true;
        }
    }

    intValue = NULL;
    if (KSI_Config_getAggrPeriod(config, &intValue) == KSI_OK && intValue != NULL) {
        uint64_t newThreadSleep = 0;
        newThreadSleep = KSI_Integer_getUInt64(intValue);
        report(ctx, "KSI gateway has reported an aggregation period value of %llu",
            (long long unsigned) newThreadSleep);

        newThreadSleep = MIN(newThreadSleep, ctx->threadSleepms);
        if(ctx->threadSleepms != newThreadSleep) {
            report(ctx, "Changing async signer thread sleep from %llu to %llu",
            (long long unsigned) ctx->threadSleepms,
            (long long unsigned) newThreadSleep);
            ctx->threadSleepms = newThreadSleep;
        }
    }
}

static int
isAggrConfNeeded(rsksictx ctx) {
    time_t now = 0;

    now = time(NULL);

    if ((uint64_t)ctx->tConfRequested + ctx->confInterval <= (uint64_t)now || ctx->tConfRequested == 0) {
        ctx->tConfRequested = now;
        return 1;
    }

    return 0;
}

/* note: if file exists, the last hash for chaining must
 * be read from file.
 */
static int
ksiOpenSigFile(ksifile ksi) {
    int r = 0, tmpRes = 0;
    const char *header;
    FILE* signatureFile = NULL;

    if (ksi->ctx->syncMode == LOGSIG_ASYNCHRONOUS)
        header = LS12_BLOCKFILE_HEADER;
    else
        header = LS12_FILE_HEADER;

    ksi->blockFile = ksiCreateFile(ksi->ctx, (char*) ksi->blockfilename, ksi->ctx->fileUID,
        ksi->ctx->fileGID, ksi->ctx->fCreateMode, true, header);

    if (ksi->blockFile == NULL) {
        r = RSGTE_IO;
        goto done;
    }

    /* create the file for ksi signatures if needed */
    if (ksi->ctx->syncMode == LOGSIG_ASYNCHRONOUS) {
        signatureFile = ksiCreateFile(ksi->ctx, (char*) ksi->ksifilename, ksi->ctx->fileUID,
            ksi->ctx->fileGID, ksi->ctx->fCreateMode, true, LS12_SIGFILE_HEADER);

        if (signatureFile == NULL) {
            r = RSGTE_IO;
            goto done;
        }

        ksi->sigFile = signatureFile;
        queueAddNewFile(ksi->ctx, ksi);
    }

    /* we now need to obtain the last previous hash, so that
     * we can continue the hash chain. We do not check for error
     * as a state file error can be recovered by graceful degredation.
     */
    ksiReadStateFile(ksi);

    if (ksi->ctx->syncMode == LOGSIG_SYNCHRONOUS) {
        if (isAggrConfNeeded(ksi->ctx)) {
            KSI_Config *config = NULL;

            tmpRes = KSI_receiveAggregatorConfig(ksi->ctx->ksi_ctx, &config);
            if (tmpRes == KSI_OK) {
                handle_ksi_config(ksi->ctx, NULL, config);
            } else {
                reportKSIAPIErr(ksi->ctx, NULL, "KSI_receiveAggregatorConfig", tmpRes);
            }
            KSI_Config_free(config);
        }
    }

done:   return r;
}

/*
 * As of some Linux and security expert I spoke to, /dev/urandom
 * provides very strong random numbers, even if it runs out of
 * entropy. As far as he knew, this is save for all applications
 * (and he had good proof that I currently am not permitted to
 * reproduce). -- rgerhards, 2013-03-04
 */
static void
seedIVKSI(ksifile ksi)
{
    int hashlen;
    int fd;
    const char *rnd_device = ksi->ctx->random_source ? ksi->ctx->random_source : "/dev/urandom";

    hashlen = KSI_getHashLength(ksi->hashAlg);
    ksi->IV = malloc(hashlen); /* do NOT zero-out! */
    /*
     * If /dev/urandom is unavailable, the current memory content is used as a
     * weak fallback for random data. This is a theoretical case, as this
     * library only supports Linux, where /dev/urandom is expected to be
     * reliably available.
     */
    if ((fd = open(rnd_device, O_RDONLY)) >= 0) {
        if(read(fd, ksi->IV, hashlen) == hashlen) {}; /* keep compiler happy */
        close(fd);
    }
}

static int
create_signer_thread(rsksictx ctx) {
    int r;
    if (ctx->signer_state != SIGNER_STARTED) {
        if ((r = pthread_mutex_init(&ctx->module_lock, 0)))
            report(ctx, "pthread_mutex_init: %s", strerror(r));
        ctx->signer_queue = ProtectedQueue_new(10);

        ctx->signer_state = SIGNER_INIT;
        if ((r = pthread_create(&ctx->signer_thread, NULL, signer_thread, ctx))) {
            report(ctx, "pthread_create: %s", strerror(r));
            ctx->signer_state = SIGNER_IDLE;
            return RSGTE_INTERNAL;
        }

        /* Lock until init. */
        while(*((volatile int*)&ctx->signer_state) & SIGNER_INIT);

        if (ctx->signer_state != SIGNER_STARTED) {
            return RSGTE_INTERNAL;
        }
    }

    return RSGTE_SUCCESS;
}

rsksictx
rsksiCtxNew(void) {
    rsksictx ctx;
    ctx = calloc(1, sizeof (struct rsksictx_s));
    KSI_CTX_new(&ctx->ksi_ctx); // TODO: error check (probably via a generic macro?)
    ctx->hasher = NULL;
    ctx->hashAlg = KSI_getHashAlgorithmByName("default");
    ctx->blockTimeLimit = 0;
    ctx->bKeepTreeHashes = false;
    ctx->bKeepRecordHashes = true;
    ctx->max_requests = (1 << 8);
    ctx->blockSigTimeout = 10;
    ctx->confInterval = 3600;
    ctx->tConfRequested = 0;
    ctx->threadSleepms = 1000;
    ctx->errFunc = NULL;
    ctx->usrptr = NULL;
    ctx->fileUID = -1;
    ctx->fileGID = -1;
    ctx->dirUID = -1;
    ctx->dirGID = -1;
    ctx->fCreateMode = 0644;
    ctx->fDirCreateMode = 0700;
#if KSI_SDK_VER_MAJOR == 3 && KSI_SDK_VER_MINOR < 22
    ctx->roundCount = 0;
    ctx->bRoundLock = 0;
#endif
    ctx->syncMode = LOGSIG_SYNCHRONOUS;
    ctx->signer_state = SIGNER_IDLE;
    ctx->disabled = false;
    ctx->ksi = NULL;

    /*if (pthread_mutex_init(&ctx->module_lock, 0))
        report(ctx, "pthread_mutex_init: %s", strerror(errno));
    ctx->signer_queue = ProtectedQueue_new(10);*/

    /* Creating a thread this way works only in daemon mode but not when being run
     interactively when not forked */
    /*ret = pthread_atfork(NULL, NULL, create_signer_thread);
    if (ret != 0)
        report(ctx, "pthread_atfork error: %s", strerror(ret));*/

    return ctx;
}

static
int rsksiStreamLogger(void *logCtx, int logLevel, const char *message)
{
    char time_buf[32];
    struct tm *tm_info;
    time_t timer;
    FILE *f = (FILE *)logCtx;

    timer = time(NULL);

    tm_info = localtime(&timer);
    if (tm_info == NULL) {
        return KSI_UNKNOWN_ERROR;
    }

    if (f != NULL) {
        flockfile(f);  /* for thread safety */
        if (strftime(time_buf, sizeof(time_buf), "%d.%m.%Y %H:%M:%S", tm_info)) {
            if (fprintf(f, "%s [%s] %lu - %s\n", level2str(logLevel),
                        time_buf, pthread_self(), message) > 0) { }
        }
        funlockfile(f);
    }

    return KSI_OK;
}

int
rsksiInitModule(rsksictx ctx) {
    int res = 0;

    if(ctx->debugFileName != NULL) {
        ctx->debugFile = fopen(ctx->debugFileName, "w");
        if(ctx->debugFile) {
            res = KSI_CTX_setLoggerCallback(ctx->ksi_ctx, rsksiStreamLogger, ctx->debugFile);
            if (res != KSI_OK)
                reportKSIAPIErr(ctx, NULL, "Unable to set logger callback", res);
            res = KSI_CTX_setLogLevel(ctx->ksi_ctx, ctx->debugLevel);
            if (res != KSI_OK)
                reportKSIAPIErr(ctx, NULL, "Unable to set log level", res);
        }
        else {
            report(ctx, "Could not open logfile %s: %s", ctx->debugFileName, strerror(errno));
        }
    }

    KSI_CTX_setOption(ctx->ksi_ctx, KSI_OPT_AGGR_HMAC_ALGORITHM, (void*)((size_t)ctx->hmacAlg));

    return create_signer_thread(ctx);
}

/* either returns ksifile object or NULL if something went wrong */
ksifile
rsksiCtxOpenFile(rsksictx ctx, unsigned char *logfn)
{
    int ret = RSGTE_INTERNAL;
    ksifile ksi;
    char fn[MAXFNAME+1];

    if (ctx->disabled)
        return NULL;

    pthread_mutex_lock(&ctx->module_lock);

    /* The thread cannot be be created in rsksiCtxNew because in daemon mode the
     process forks after rsksiCtxNew and the thread disappears */
    if (ctx->signer_state != SIGNER_STARTED) {
        ret = rsksiInitModule(ctx);
        if (ret != RSGTE_SUCCESS) {
            report(ctx, "Unable to init. KSI module, signing service disabled");
            ctx->disabled = true;
            pthread_mutex_unlock(&ctx->module_lock);
            return NULL;
        }
    }

    if ((ksi = rsksifileConstruct(ctx)) == NULL)
        goto done;

    snprintf(fn, sizeof (fn), "%s.ksistate", logfn);
    fn[MAXFNAME] = '\0'; /* be on safe side */
    ksi->statefilename = (uchar*) strdup(fn);

    if (ctx->syncMode == LOGSIG_ASYNCHRONOUS) {
        /* filename for blocks of hashes*/
        snprintf(fn, sizeof (fn), "%s%s", logfn, blockFileSuffix);
        fn[MAXFNAME] = '\0'; /* be on safe side */
        ksi->blockfilename = (uchar*) strdup(fn);

        /* filename for KSI signatures*/
        snprintf(fn, sizeof (fn), "%s%s", logfn, sigFileSuffix);
        fn[MAXFNAME] = '\0'; /* be on safe side */
        ksi->ksifilename = (uchar*) strdup(fn);
    } else if (ctx->syncMode == LOGSIG_SYNCHRONOUS) {
        snprintf(fn, sizeof (fn), "%s%s", logfn, ls12FileSuffix);
        fn[MAXFNAME] = '\0'; /* be on safe side */
        ksi->blockfilename = (uchar*) strdup(fn);
    }

    if (ksiOpenSigFile(ksi) != 0) {
        reportErr(ctx, "signature file open failed");
        /* Free memory */
        free(ksi);
        ksi = NULL;
    }

done:
    /* Register ksi file in rsksictx for keeping track of block timeouts. */
    rsksiRegisterKsiFile(ctx, ksi);
    pthread_mutex_unlock(&ctx->module_lock);
    return ksi;
}


/* Returns RSGTE_SUCCESS on success, error code otherwise. If algo is unknown or
 * is not trusted, default hash function is used.
 */
int
rsksiSetHashFunction(rsksictx ctx, char *algName) {
    if (ctx == NULL || algName == NULL) {
        return RSGTE_INTERNAL;
    }

    int r, id = KSI_getHashAlgorithmByName(algName);
    if (!KSI_isHashAlgorithmSupported(id)) {
        report(ctx, "Hash function '%s' is not supported - using default", algName);
        ctx->hashAlg = KSI_getHashAlgorithmByName("default");
    } else {
        if(!KSI_isHashAlgorithmTrusted(id)) {
            report(ctx, "Hash function '%s' is not trusted - using default", algName);
            ctx->hashAlg = KSI_getHashAlgorithmByName("default");
        }
        else
            ctx->hashAlg = id;
    }

    if ((r = KSI_DataHasher_open(ctx->ksi_ctx, ctx->hashAlg, &ctx->hasher)) != KSI_OK) {
        reportKSIAPIErr(ctx, NULL, "KSI_DataHasher_open", r);
        ctx->disabled = true;
        return r;
    }

    return RSGTE_SUCCESS;
}

int
rsksiSetHmacFunction(rsksictx ctx, char *algName) {
    int id = KSI_getHashAlgorithmByName(algName);
    if (!KSI_isHashAlgorithmSupported(id)) {
        report(ctx, "HMAC function '%s' is not supported - using default", algName);
        ctx->hmacAlg = KSI_getHashAlgorithmByName("default");
    } else {
        if(!KSI_isHashAlgorithmTrusted(id)) {
            report(ctx, "HMAC function '%s' is not trusted - using default", algName);
            ctx->hmacAlg = KSI_getHashAlgorithmByName("default");
        }
        else
            ctx->hmacAlg = id;
    }
    return 0;
}

int
rsksifileDestruct(ksifile ksi) {
    int r = 0;
    rsksictx ctx = NULL;
    if (ksi == NULL)
        return RSGTE_INTERNAL;

    pthread_mutex_lock(&ksi->ctx->module_lock);

    ctx = ksi->ctx;

    /* Deregister ksifile so it is not used by signer thread anymore. Note that files are not closed yet! */
    rsksiDeregisterKsiFile(ctx, ksi);

    if (!ksi->disabled && ksi->bInBlk) {
        sigblkAddMetadata(ksi, blockCloseReason, "Block closed due to file closure.");
        r = sigblkFinishKSI(ksi);
    }
    /* Note that block file is closed immediately but signature file will be closed
     * by the signer thread scheduled by signer thread work queue.
     */
    if(!ksi->disabled)
        r = ksiCloseSigFile(ksi);
    free(ksi->blockfilename);
    free(ksi->statefilename);
    free(ksi->ksifilename);

    free(ksi);

    pthread_mutex_unlock(&ctx->module_lock);
    return r;
}

/* This can only be used when signer thread has terminated or within the thread. */
static void
rsksifileForceFree(ksifile ksi) {
    if (ksi == NULL) return;

    if (ksi->sigFile != NULL) fclose(ksi->sigFile);
    if (ksi->blockFile != NULL) fclose(ksi->blockFile);
    free(ksi->blockfilename);
    free(ksi->statefilename);
    free(ksi->ksifilename);
    free(ksi);
    return;
}

/* This can only be used when signer thread has terminated or within the thread. */
static void
rsksictxForceFreeSignatures(rsksictx ctx) {
    size_t i = 0;

    if (ctx == NULL || ctx->ksi == NULL) return;

    for (i = 0; i < ctx->ksiCount; i++) {
        if (ctx->ksi[i] != NULL) {
            rsksifileForceFree(ctx->ksi[i]);
            ctx->ksi[i] = NULL;
        }
    }

    ctx->ksiCount = 0;
    return;
}

/* This can only be used when signer thread has terminated or within the thread. */
static int
rsksictxForceCloseWithoutSig(rsksictx ctx, const char *reason) {
    size_t i = 0;
    if (ctx == NULL || ctx->ksi == NULL) return RSGTE_INTERNAL;
    for (i = 0; i < ctx->ksiCount; i++) {
        if (ctx->ksi[i] != NULL) {
            int ret = RSGTE_INTERNAL;

            /* Only if block contains records, create metadata, close the block and add
             * no signature marker. Closing block without record will produce redundant
             * blocks that needs to be signed afterward.
             */
            if (ctx->ksi[i]->nRecords > 0) {
                ret = sigblkFinishKSINoSignature(ctx->ksi[i], reason);
                if (ret != RSGTE_SUCCESS) return ret;
            }

            /* Free files and remove object from the list. */
            rsksifileForceFree(ctx->ksi[i]);
            ctx->ksi[i] = NULL;
        }
    }

    ctx->ksiCount = 0;
    return RSGTE_SUCCESS;
}

void
rsksiCtxDel(rsksictx ctx) {
    if (ctx == NULL)
        return;

    /* Note that even in sync. mode signer thread is created and needs to be closed
     * correctly.
     */
    if (ctx->signer_state == SIGNER_STARTED) {
        queueAddQuit(ctx);
        /* Wait until thread closes to be able to safely free the resources. */
        pthread_join(ctx->signer_thread, NULL);
        ProtectedQueue_free(ctx->signer_queue);
        pthread_mutex_destroy(&ctx->module_lock);
    }

    free(ctx->aggregatorUri);
    free(ctx->aggregatorId);
    free(ctx->aggregatorKey);
    free(ctx->debugFileName);

    if (ctx->random_source)
        free(ctx->random_source);

    KSI_DataHasher_free(ctx->hasher);
    KSI_CTX_free(ctx->ksi_ctx);

    if(ctx->debugFile!=NULL)
        fclose(ctx->debugFile);

    /* After signer thread is terminated there should be no open signature files,
     * but to be extra sure that all files are closed, recheck the list of opened
     * signature files.
     */
    rsksictxForceFreeSignatures(ctx);
    free(ctx->ksi);

    free(ctx);
}

/* new sigblk is initialized, but maybe in existing ctx */
void
sigblkInitKSI(ksifile ksi)
{
    if(ksi == NULL) goto done;
    seedIVKSI(ksi);
    memset(ksi->roots, 0, sizeof (ksi->roots));
    ksi->nRoots = 0;
    ksi->nRecords = 0;
    ksi->bInBlk = 1;
    ksi->blockStarted = time(NULL);
    ksi->blockSizeLimit = 1 << (ksi->ctx->effectiveBlockLevelLimit - 1);

    /* flush the optional debug file when starting a new block */
    if(ksi->ctx->debugFile != NULL)
        fflush(ksi->ctx->debugFile);

done:
    return;
}

int
sigblkCreateMask(ksifile ksi, KSI_DataHash **m) {
    int r = 0;

    CHKr(KSI_DataHasher_reset(ksi->ctx->hasher));
    CHKr(KSI_DataHasher_add(ksi->ctx->hasher, ksi->lastLeaf, KSI_getHashLength(ksi->lastLeaf[0]) + 1));
    CHKr(KSI_DataHasher_add(ksi->ctx->hasher, ksi->IV, KSI_getHashLength(ksi->hashAlg)));
    CHKr(KSI_DataHasher_close(ksi->ctx->hasher, m));

done:
    if (r != KSI_OK) {
        reportKSIAPIErr(ksi->ctx, ksi, "KSI_DataHasher", r);
        r = RSGTE_HASH_CREATE;
    }
    return r;
}
int
sigblkCreateHash(ksifile ksi, KSI_DataHash **out, const uchar *rec, const size_t len) {
    int r = 0;

    CHKr(KSI_DataHasher_reset(ksi->ctx->hasher));
    CHKr(KSI_DataHasher_add(ksi->ctx->hasher, rec, len));
    CHKr(KSI_DataHasher_close(ksi->ctx->hasher, out));

done:
    if (r != KSI_OK) {
        reportKSIAPIErr(ksi->ctx, ksi, "KSI_DataHasher", r);
        r = RSGTE_HASH_CREATE;
    }

    return r;
}


int
sigblkHashTwoNodes(ksifile ksi, KSI_DataHash **out, KSI_DataHash *left, KSI_DataHash *right,
        uint8_t level) {
    int r = 0;

    CHKr(KSI_DataHasher_reset(ksi->ctx->hasher));
    CHKr(KSI_DataHasher_addImprint(ksi->ctx->hasher, left));
    CHKr(KSI_DataHasher_addImprint(ksi->ctx->hasher, right));
    CHKr(KSI_DataHasher_add(ksi->ctx->hasher, &level, 1));
    CHKr(KSI_DataHasher_close(ksi->ctx->hasher, out));

done:
    if (r != KSI_OK) {
        reportKSIAPIErr(ksi->ctx, ksi, "KSI_DataHash_create", r);
        r = RSGTE_HASH_CREATE;
    }

    return r;
}
int
sigblkAddMetadata(ksifile ksi, const char *key, const char *value) {
    unsigned char buffer[0xFFFF];
    size_t encoded_size = 0;
    int ret = 0;

    tlvCreateMetadata(ksi, ksi->nRecords, key, value, buffer, &encoded_size);
    sigblkAddLeaf(ksi, buffer, encoded_size, true);

    return ret;
}

int
sigblkAddRecordKSI(ksifile ksi, const uchar *rec, const size_t len) {
    int ret = 0;
    if (ksi == NULL || ksi->disabled)
        return 0;

    pthread_mutex_lock(&ksi->ctx->module_lock);

    if ((ret = sigblkAddLeaf(ksi, rec, len, false)) != 0)
        goto done;

    if (ksi->nRecords == ksi->blockSizeLimit) {
        sigblkFinishKSI(ksi);
        sigblkInitKSI(ksi);
    }

done:
    pthread_mutex_unlock(&ksi->ctx->module_lock);
    return ret;
}


int
sigblkAddLeaf(ksifile ksi, const uchar *leafData, const size_t leafLength, bool metadata) {

    KSI_DataHash *mask, *leafHash, *treeNode, *tmpTreeNode;
    uint8_t j;
    const unsigned char *pTmp;
    size_t len;

    int r = 0;

    if (ksi == NULL || ksi->disabled) goto done;
    CHKr(sigblkCreateMask(ksi, &mask));
    CHKr(sigblkCreateHash(ksi, &leafHash, leafData, leafLength));

    if(ksi->nRecords == 0)
        tlvWriteBlockHdrKSI(ksi);

    /* metadata record has to be written into the block file too*/
    if (metadata)
        tlvWriteOctetString(ksi->blockFile, leafData, leafLength);

    if (ksi->bKeepRecordHashes)
        tlvWriteHashKSI(ksi, 0x0902, leafHash);

    /* normal leaf and metadata record are hashed in different order */
    if (!metadata) { /* hash leaf */
        if ((r = sigblkHashTwoNodes(ksi, &treeNode, mask, leafHash, 1)) != 0) goto done;
    } else {
        if ((r = sigblkHashTwoNodes(ksi, &treeNode, leafHash, mask, 1)) != 0) goto done;
    }

    /* persists x here if Merkle tree needs to be persisted! */
    if(ksi->bKeepTreeHashes)
        tlvWriteHashKSI(ksi, 0x0903, treeNode);

    KSI_DataHash_getImprint(treeNode, &pTmp, &len);
    memcpy(ksi->lastLeaf, pTmp, len);

    for(j = 0 ; j < ksi->nRoots ; ++j) {
        if (ksi->roots[j] == NULL) {
            ksi->roots[j] = treeNode;
            treeNode = NULL;
            break;
        } else if (treeNode != NULL) {
            /* hash interim node */
            tmpTreeNode = treeNode;
            r = sigblkHashTwoNodes(ksi, &treeNode, ksi->roots[j], tmpTreeNode, j + 2);
            KSI_DataHash_free(ksi->roots[j]);
            ksi->roots[j] = NULL;
            KSI_DataHash_free(tmpTreeNode);
            if (r != 0) goto done;
            if(ksi->bKeepTreeHashes)
                tlvWriteHashKSI(ksi, 0x0903, treeNode);
        }
    }
    if (treeNode != NULL) {
        /* new level, append "at the top" */
        ksi->roots[ksi->nRoots] = treeNode;
        ++ksi->nRoots;
        assert(ksi->nRoots < MAX_ROOTS);
        treeNode = NULL;
    }
    ++ksi->nRecords;

    /* cleanup (x is cleared as part of the roots array) */
    KSI_DataHash_free(mask);
    KSI_DataHash_free(leafHash);

done:
    return r;
}

static int
sigblkCheckTimeOut(rsksictx ctx) {
    int ret = RSGTE_INTERNAL;
    time_t now;
    char buf[KSI_BUF_SIZE];
    size_t i = 0;

    if (ctx == NULL) {
        return RSGTE_INTERNAL;
    }

    pthread_mutex_lock(&ctx->module_lock);

    if (ctx->ksi == NULL || ctx->disabled || !ctx->blockTimeLimit) {
        ret = RSGTE_SUCCESS;
        goto done;
    }

    now = time(NULL);

    for (i = 0; i < ctx->ksiCount; i++) {
        ksifile ksi = ctx->ksi[i];

        if (ksi == NULL) continue;  /* To avoide unexpected crash. */
        if (!ksi->bInBlk) continue; /* Not inside a block, nothing to close nor sign. */
        if ((time_t) (ksi->blockStarted + ctx->blockTimeLimit) > now) continue;

        snprintf(buf, KSI_BUF_SIZE, "Block closed due to reaching time limit %d", ctx->blockTimeLimit);
        sigblkAddMetadata(ksi, blockCloseReason, buf);
        sigblkFinishKSI(ksi);
        sigblkInitKSI(ksi);
    }

done:
    pthread_mutex_unlock(&ctx->module_lock);
    return ret;
}


static int
sigblkSign(ksifile ksi, KSI_DataHash *hash, int level)
{
    unsigned char *der = NULL;
    size_t lenDer = 0;
    int r = KSI_OK;
    int ret = 0;
    KSI_Signature *sig = NULL;

    /* Sign the root hash. */
    r = KSI_Signature_signAggregated(ksi->ctx->ksi_ctx, hash, level, &sig);
    if (r != KSI_OK) {
        reportKSIAPIErr(ksi->ctx, ksi, "KSI_Signature_createAggregated", r);
        ret = 1;
        goto signing_done;
    }

    /* Serialize Signature. */
    r = KSI_Signature_serialize(sig, &der, &lenDer);
    if (r != KSI_OK) {
        reportKSIAPIErr(ksi->ctx, ksi, "KSI_Signature_serialize", r);
        ret = 1;
        lenDer = 0;
        goto signing_done;
    }

signing_done:
    /* if signing failed the signature will be written as zero size */
    if (r == KSI_OK) {
        r = tlvWriteKSISigLS12(ksi->blockFile, ksi->nRecords, der, lenDer);
        if (r != KSI_OK) {
            reportKSIAPIErr(ksi->ctx, ksi, "tlvWriteKSISigLS12", r);
            ret = 1;
        }
    } else
        r = tlvWriteNoSigLS12(ksi->blockFile, ksi->nRecords, hash, KSI_getErrorString(r));

    if (r != KSI_OK) {
        reportKSIAPIErr(ksi->ctx, ksi, "tlvWriteBlockSigKSI", r);
        ret = 1;
    }

    if (sig != NULL)
        KSI_Signature_free(sig);
    if (der != NULL)
        KSI_free(der);
    return ret;
}

unsigned
sigblkCalcLevel(unsigned leaves) {
    unsigned level = 0;
    unsigned c = leaves;
    while (c > 1) {
        level++;
        c >>= 1;
    }

    if (1 << level < (int)leaves)
        level++;

    return level;
}

static int
sigblkFinishTree(ksifile ksi, KSI_DataHash **hsh) {
    int ret = RSGTE_INTERNAL;
    KSI_DataHash *root = NULL;
    KSI_DataHash *rootDel = NULL;
    int8_t j = 0;

    if (ksi == NULL || hsh == NULL) {
        goto done;
    }

    if (ksi->nRecords == 0) {
        ret = RSGTE_SUCCESS;
        goto done;
    }

    root = NULL;
    for(j = 0 ; j < ksi->nRoots ; ++j) {
        if(root == NULL) {
            root = ksi->roots[j];
            ksi->roots[j] = NULL;
        } else if (ksi->roots[j] != NULL) {
            rootDel = root;
            root = NULL;
            ret = sigblkHashTwoNodes(ksi, &root, ksi->roots[j], rootDel, j + 2);
            KSI_DataHash_free(ksi->roots[j]);
            ksi->roots[j] = NULL;
            KSI_DataHash_free(rootDel);
            rootDel = NULL;
            if(ksi->bKeepTreeHashes) {
                tlvWriteHashKSI(ksi, 0x0903, root);
            }
            if(ret != KSI_OK) goto done; /* checks sigblkHashTwoNodes() result! */
        }
    }

    *hsh = root;
    root = NULL;
    ret = RSGTE_SUCCESS;

done:
    KSI_DataHash_free(root);
    KSI_DataHash_free(rootDel);
    return ret;
}


int
sigblkFinishKSI(ksifile ksi)
{
    KSI_DataHash *root = NULL;
    int ret = RSGTE_INTERNAL;
    unsigned level = 0;

    if (ksi == NULL) {
        goto done;
    }

    if (ksi->nRecords == 0) {
        ret = RSGTE_SUCCESS;
        goto done;
    }

    ret = sigblkFinishTree(ksi, &root);
    if (ret != RSGTE_SUCCESS) goto done;

    //Multiplying leaves count by 2 to account for blinding masks
    level=sigblkCalcLevel(2 * ksi->nRecords);

    //in case of async mode we append the root hash to signer queue
    if (ksi->ctx->syncMode == LOGSIG_ASYNCHRONOUS) {
        ret = tlvWriteNoSigLS12(ksi->blockFile, ksi->nRecords, root, NULL);
        if (ret != KSI_OK) {
            reportKSIAPIErr(ksi->ctx, ksi, "tlvWriteNoSigLS12", ret);
            goto done;
        }

        queueAddSignRequest(ksi->ctx, ksi, root, level);
        root = NULL;
    } else {
        sigblkSign(ksi, root, level);
    }

    ret = RSGTE_SUCCESS;

done:
    KSI_DataHash_free(root);
    free(ksi->IV);
    ksi->IV = NULL;
    ksi->bInBlk = 0;
    return ret;
}

static int
sigblkFinishKSINoSignature(ksifile ksi, const char *reason)
{
    KSI_DataHash *root = NULL;
    int ret = RSGTE_INTERNAL;

    if (ksi == NULL || ksi->ctx == NULL ||
       (ksi->ctx->syncMode == LOGSIG_ASYNCHRONOUS && ksi->sigFile == NULL) ||
        ksi->blockFile == NULL || reason == NULL) {
        goto done;
    }

    ret = sigblkAddMetadata(ksi, blockCloseReason, reason);
    if (ret != RSGTE_SUCCESS) goto done;

    ret = sigblkFinishTree(ksi, &root);
    if (ret != RSGTE_SUCCESS) goto done;

    ret = tlvWriteNoSigLS12(ksi->blockFile, ksi->nRecords, root, reason);
    if (ret != KSI_OK) {
        reportKSIAPIErr(ksi->ctx, ksi, "tlvWriteNoSigLS12", ret);
        goto done;
    }

    if (ksi->ctx->syncMode == LOGSIG_ASYNCHRONOUS) {
        ret = tlvWriteNoSigLS12(ksi->sigFile, ksi->nRecords, root, reason);
        if (ret != KSI_OK) {
            reportKSIAPIErr(ksi->ctx, ksi, "tlvWriteNoSigLS12", ret);
            goto done;
        }
    }

    ret = RSGTE_SUCCESS;

done:
    KSI_DataHash_free(root);
    free(ksi->IV);
    ksi->IV=NULL;
    ksi->bInBlk = 0;
    return ret;
}

int
rsksiSetAggregator(rsksictx ctx, char *uri, char *loginid, char *key) {
    int r;
    char *strTmp, *strTmpUri;

    /* only use the strings if they are not empty */
    ctx->aggregatorUri = (uri != NULL && strlen(uri) != 0) ? strdup(uri) : NULL;
    ctx->aggregatorId = (loginid != NULL && strlen(loginid) != 0) ? strdup(loginid) : NULL;
    ctx->aggregatorKey = (key != NULL && strlen(key) != 0) ? strdup(key) : NULL;

    /* split the URI string up for possible HA endpoints */
    strTmp = ctx->aggregatorUri;
    while((strTmpUri = strsep(&strTmp, "|") ) != NULL) {
        if(ctx->aggregatorEndpointCount >= KSI_CTX_HA_MAX_SUBSERVICES) {
            report(ctx, "Maximum number (%d) of service endoints reached, ignoring endpoint: %s",
                KSI_CTX_HA_MAX_SUBSERVICES, strTmpUri);
        }
        else {
            ctx->aggregatorEndpoints[ctx->aggregatorEndpointCount] = strTmpUri;
            ctx->aggregatorEndpointCount++;
        }
    }

    r = KSI_CTX_setAggregator(ctx->ksi_ctx, ctx->aggregatorUri, ctx->aggregatorId, ctx->aggregatorKey);
    if(r != KSI_OK) {
        ctx->disabled = true;
        reportKSIAPIErr(ctx, NULL, "KSI_CTX_setAggregator", r);
        return KSI_INVALID_ARGUMENT;
    }

    return r;
}


int
rsksiSetDebugFile(rsksictx ctx, char *val) {
    if(!val)
        return KSI_INVALID_ARGUMENT;

    ctx->debugFileName=strdup(val);
    return KSI_OK;
}

static bool
add_queue_item(rsksictx ctx, QITEM_type type, KSI_DataHash *root, FILE *sigFile, uint64_t intarg1, uint64_t intarg2) {
    QueueItem *qi = (QueueItem*) malloc(sizeof (QueueItem));
    if (!qi) {
        ctx->disabled = true;
        return false;
    }

    qi->root = root;
    qi->file = sigFile;

    qi->type = type;
    qi->status = QITEM_WAITING;
    qi->intarg1 = intarg1;
    qi->intarg2 = intarg2;
    qi->respHandle = NULL;
    qi->ksi_status = KSI_UNKNOWN_ERROR;
    qi->request_time = time(NULL);
    if (ProtectedQueue_addItem(ctx->signer_queue, qi) == false) {
        ctx->disabled = true;
        free(qi);
        return false;
    }
    return true;
}

static bool
queueAddCloseFile(rsksictx ctx, ksifile ksi) {
    return add_queue_item(ctx, QITEM_CLOSE_FILE, NULL, ksi->sigFile, 0, 0);
}

static bool
queueAddNewFile(rsksictx ctx, ksifile ksi) {
    return add_queue_item(ctx, QITEM_NEW_FILE, NULL, ksi->sigFile, time(NULL) + ctx->blockTimeLimit, 0);
}

static bool
queueAddQuit(rsksictx ctx) {
    return add_queue_item(ctx, QITEM_QUIT, NULL, NULL, 0, 0);
}

static bool
queueAddSignRequest(rsksictx ctx, ksifile ksi, KSI_DataHash *root, unsigned level) {
    return add_queue_item(ctx, QITEM_SIGNATURE_REQUEST, root, ksi->sigFile, ksi->nRecords, level);
}

static bool
save_response(rsksictx ctx, FILE* outfile, QueueItem *item) {
    bool ret = false;
    KSI_Signature *sig = NULL;
    unsigned char *raw = NULL;
    size_t raw_len;
    int res = KSI_OK;

    if(item->respHandle != NULL && item->ksi_status == KSI_OK) {
        CHECK_KSI_API(KSI_AsyncHandle_getSignature(item->respHandle, &sig), ctx,
            "KSI_AsyncHandle_getSignature");
        CHECK_KSI_API(KSI_Signature_serialize(sig, &raw, &raw_len), ctx,
            "KSI_Signature_serialize");
        tlvWriteKSISigLS12(outfile, item->intarg1, raw, raw_len);
        KSI_free(raw);
    }
    else {
        tlvWriteNoSigLS12(outfile, item->intarg1, item->root, KSI_getErrorString(item->ksi_status));
    }
    ret = true;

cleanup:
    if(res != KSI_OK)
        tlvWriteNoSigLS12(outfile, item->intarg1, item->root, KSI_getErrorString(res));

    KSI_Signature_free(sig);

    return ret;
}

static KSI_DataHash* clone_hash(KSI_CTX *ksi_ctx, const KSI_DataHash* hash) {
    int res = KSI_UNKNOWN_ERROR;
    const unsigned char *imprint = NULL;
    size_t imprint_len = 0;
    KSI_DataHash* tmp = NULL;

    if (hash == NULL) return NULL;
    res = KSI_DataHash_getImprint(hash, &imprint, &imprint_len);
    if (res != KSI_OK) return NULL;
    res = KSI_DataHash_fromImprint(ksi_ctx, imprint, imprint_len, &tmp);
    if (res != KSI_OK) return NULL;

    return tmp;
}

static bool
process_requests_async(rsksictx ctx, KSI_CTX *ksi_ctx, KSI_AsyncService *as) {
    bool ret = false;
    QueueItem *item = NULL;
    int res = KSI_OK, tmpRes;
    KSI_AsyncHandle *reqHandle = NULL;
    KSI_AsyncHandle *respHandle = NULL;
    KSI_DataHash *clonedHash = NULL;
    KSI_AggregationReq *req = NULL;
    KSI_Config *config = NULL;
    KSI_Integer *level;
    long extError;
    KSI_Utf8String *errorMsg;
    int state, ksi_status;
    unsigned i;
    size_t p;

    KSI_AsyncService_getPendingCount(as, &p);

    /* Check if there are pending/available responses and associate them with the request items */
    while(true) {
        respHandle = NULL;
        item = NULL;
        tmpRes=KSI_AsyncService_run(as, &respHandle, &p);
        if(tmpRes!=KSI_OK)
            reportKSIAPIErr(ctx, NULL, "KSI_AsyncService_run", tmpRes);

        if (respHandle == NULL) { /* nothing received */
            break;
        }

#if KSI_SDK_VER_MAJOR == 3 && KSI_SDK_VER_MINOR < 22
        if (p != 0 && ctx->roundCount > 0) {
            ctx->roundCount--;
        } else {
            ctx->bRoundLock = 0;
            ctx->roundCount = 0;
        }
#endif
        state = KSI_ASYNC_STATE_UNDEFINED;

        CHECK_KSI_API(KSI_AsyncHandle_getState(respHandle, &state), ctx, "KSI_AsyncHandle_getState");

        if(state == KSI_ASYNC_STATE_PUSH_CONFIG_RECEIVED) {
            res = KSI_AsyncHandle_getConfig(respHandle, &config);
            if(res == KSI_OK) {
                handle_ksi_config(ctx, as, config);
                KSI_AsyncHandle_free(respHandle);
            } else
                reportKSIAPIErr(ctx, NULL, "KSI_AsyncHandle_getConfig", res);
        }
        else if(state == KSI_ASYNC_STATE_RESPONSE_RECEIVED) {
            CHECK_KSI_API(KSI_AsyncHandle_getRequestCtx(respHandle, (const void**)&item), ctx,
                "KSI_AsyncHandle_getRequestCtx");
            item->respHandle = respHandle;
            item->ksi_status = KSI_OK;
        }
        else if(state == KSI_ASYNC_STATE_ERROR) {
            CHECK_KSI_API(KSI_AsyncHandle_getRequestCtx(respHandle, (const void**)&item), ctx,
                "KSI_AsyncHandle_getRequestCtx");
            errorMsg = NULL;
            KSI_AsyncHandle_getError(respHandle, &ksi_status);
            KSI_AsyncHandle_getExtError(respHandle, &extError);
            KSI_AsyncHandle_getErrorMessage(respHandle, &errorMsg);
            report(ctx, "Asynchronous request returned error %s (%d), %lu %s",
                KSI_getErrorString(ksi_status), ksi_status, extError,
                    errorMsg ? KSI_Utf8String_cstr(errorMsg) : "");
            KSI_AsyncHandle_free(respHandle);

            if(item)
                item->ksi_status = ksi_status;
        }

        if(item)
            item->status = QITEM_DONE;
    }

    KSI_AsyncService_getPendingCount(as, &p);

    /* Send all the new requests in the back of the queue to the server */
    for(i = 0; i < ProtectedQueue_count(ctx->signer_queue); i++) {
        item = NULL;
        if(!ProtectedQueue_getItem(ctx->signer_queue, i, (void**)&item) || !item)
            continue;
        /* ingore non request queue items */
        if(item->type != QITEM_SIGNATURE_REQUEST)
            continue;

        /* stop at first processed item */
        if(item->status != QITEM_WAITING)
            continue;

        /* Due to a bug in libksi it is possible that async signer may send out
         * more signing requests than permitted by the gateway. Workaround is to
         * keep track of signing requests here.
         */
#if KSI_SDK_VER_MAJOR == 3 && KSI_SDK_VER_MINOR < 22
        if (ctx->roundCount >= ctx->max_requests) ctx->bRoundLock = 1;
        if (ctx->bRoundLock) break;
#endif


        /* The data hash is produced in another thread by another KSI_CTX and
         * libksi internal uses KSI_DataHash cache to reduce the amount of
         * memory allocations by recycling old objects. Lets clone the hash
         * value with current KSI_CTX as we can not be sure that this thread is
         * not affecting the data hash cache operated by another thread.
         */
        clonedHash = clone_hash(ksi_ctx, item->root);
        CHECK_KSI_API(KSI_AggregationReq_new(ksi_ctx, &req), ctx, "KSI_AggregationReq_new");
        CHECK_KSI_API(KSI_AggregationReq_setRequestHash((KSI_AggregationReq*)req,
            clonedHash), ctx,
            "KSI_AggregationReq_setRequestHash");
        clonedHash = NULL;
        CHECK_KSI_API(KSI_Integer_new(ksi_ctx, item->intarg2, &level), ctx,
            "KSI_Integer_new");
        CHECK_KSI_API(KSI_AggregationReq_setRequestLevel(req, level), ctx,
            "KSI_AggregationReq_setRequestLevel");
        CHECK_KSI_API(KSI_AsyncAggregationHandle_new(ksi_ctx, req, &reqHandle), ctx,
            "KSI_AsyncAggregationHandle_new");
        CHECK_KSI_API(KSI_AsyncHandle_setRequestCtx(reqHandle, (void*)item, NULL), ctx,
            "KSI_AsyncRequest_setRequestContext");
        res = KSI_AsyncService_addRequest(as, reqHandle); /* this can fail because of throttling */

        if (res == KSI_OK) {
            item->status = QITEM_SENT;
#if KSI_SDK_VER_MAJOR == 3 && KSI_SDK_VER_MINOR < 22
            ctx->roundCount++;
#endif
        } else {
            reportKSIAPIErr(ctx, NULL, "KSI_AsyncService_addRequest", res);
            KSI_AsyncHandle_free(reqHandle);
            item->status = QITEM_DONE;
            item->ksi_status = res;
            break;
        }

        if (i != 0 && i % ctx->max_requests == 0) {
            CHECK_KSI_API(KSI_AsyncService_run(as, NULL, NULL), ctx,
                "KSI_AsyncService_run");
        }
    }
    CHECK_KSI_API(KSI_AsyncService_run(as, NULL, NULL), ctx,
        "KSI_AsyncService_run");

    /* Save all consequent fulfilled responses in the front of the queue to the signature file */
    while(ProtectedQueue_count(ctx->signer_queue)) {
        item = NULL;
        if(!ProtectedQueue_getItem(ctx->signer_queue, 0, (void**)&item))
            break;

        if(!item) {
            ProtectedQueue_popFront(ctx->signer_queue, (void**) &item);
            continue;
        }

        /* stop at first non request queue item (maybe file close/open, quit) */
        if(item->type!=QITEM_SIGNATURE_REQUEST)
            break;

        /* stop at first unfinished queue item because the signatures need to be ordered */
        if(item->status != QITEM_DONE)
            break;

        ProtectedQueue_popFront(ctx->signer_queue, (void**) &item);
        save_response(ctx, item->file, item);
        fflush(item->file);
        /* the main thread has to be locked when the hash is freed to avoid a race condition */
        pthread_mutex_lock(&ctx->module_lock);
        KSI_DataHash_free(item->root);
        KSI_AsyncHandle_free(item->respHandle);
        free(item);
        pthread_mutex_unlock(&ctx->module_lock);
    }

    ret = true;

cleanup:
    KSI_DataHash_free(clonedHash);
    KSI_AsyncService_getPendingCount(as, &p);
    return ret;
}

/* This can only be used when signer thread has terminated or within the thread. */
static bool
rsksictxCloseAllPendingBlocksWithoutSignature(rsksictx ctx, const char *reason) {
    bool ret = false;
    QueueItem *item = NULL;
    int res = KSI_OK;

    /* Save all consequent fulfilled responses in the front of the queue to the signature file */
    while(ProtectedQueue_count(ctx->signer_queue)) {
        item = NULL;
        ProtectedQueue_popFront(ctx->signer_queue, (void**) &item);

        if(item == NULL) {
            continue;
        }

        /* Skip non request queue item. */
        if(item->type == QITEM_SIGNATURE_REQUEST) {
            res = tlvWriteNoSigLS12(item->file, item->intarg1, item->root, reason);
            if (res != KSI_OK) {
                reportKSIAPIErr(ctx, NULL, "tlvWriteNoSigLS12", res);
                ret = false;
                goto cleanup;
            }
            fflush(item->file);
        }

        KSI_DataHash_free(item->root);
        KSI_AsyncHandle_free(item->respHandle);
        free(item);
    }

    ret = true;

cleanup:
    return ret;
}


static void
request_async_config(rsksictx ctx, KSI_CTX *ksi_ctx, KSI_AsyncService *as) {
    KSI_Config *cfg = NULL;
    KSI_AsyncHandle *cfgHandle = NULL;
    KSI_AggregationReq *cfgReq = NULL;
    int res;
    bool bSuccess = false;

    CHECK_KSI_API(KSI_AggregationReq_new(ksi_ctx, &cfgReq), ctx, "KSI_AggregationReq_new");
    CHECK_KSI_API(KSI_Config_new(ksi_ctx, &cfg), ctx, "KSI_Config_new");
    CHECK_KSI_API(KSI_AggregationReq_setConfig(cfgReq, cfg), ctx, "KSI_AggregationReq_setConfig");
    CHECK_KSI_API(KSI_AsyncAggregationHandle_new(ksi_ctx, cfgReq, &cfgHandle), ctx,
            "KSI_AsyncAggregationHandle_new");
    CHECK_KSI_API(KSI_AsyncService_addRequest(as, cfgHandle), ctx, "KSI_AsyncService_addRequest");

    bSuccess = true;

cleanup:
    if(!bSuccess) {
        if(cfgHandle)
            KSI_AsyncHandle_free(cfgHandle);
        else if(cfgReq)
            KSI_AggregationReq_free(cfgReq);
        else if(cfg)
            KSI_Config_free(cfg);
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
void *signer_thread(void *arg) {
    int res = KSI_UNKNOWN_ERROR;
    rsksictx ctx = (rsksictx) arg;
    KSI_CTX *ksi_ctx = NULL;
    KSI_AsyncService *as = NULL;
    size_t size_t_value = 0;
    size_t ksiFileCount = 0;
    int endpoints = 0;
    bool bSleep = true;


    CHECK_KSI_API(KSI_CTX_new(&ksi_ctx), ctx, "KSI_CTX_new");
    CHECK_KSI_API(KSI_CTX_setAggregator(ksi_ctx,
        ctx->aggregatorUri, ctx->aggregatorId, ctx->aggregatorKey),
        ctx, "KSI_CTX_setAggregator");

    if(ctx->debugFile) {
        res = KSI_CTX_setLoggerCallback(ksi_ctx, rsksiStreamLogger, ctx->debugFile);
        if (res != KSI_OK)
            reportKSIAPIErr(ctx, NULL, "Unable to set logger callback", res);
        res = KSI_CTX_setLogLevel(ksi_ctx, ctx->debugLevel);
        if (res != KSI_OK)
            reportKSIAPIErr(ctx, NULL, "Unable to set log level", res);
    }

    CHECK_KSI_API(KSI_CTX_setOption(ksi_ctx, KSI_OPT_AGGR_HMAC_ALGORITHM, (void*)((size_t)ctx->hmacAlg)),
        ctx, "KSI_CTX_setOption");

    res = KSI_SigningHighAvailabilityService_new(ksi_ctx, &as);
    if (res != KSI_OK) {
        reportKSIAPIErr(ctx, NULL, "KSI_SigningAsyncService_new", res);
    }
    else {
        int i = 0;
        for (i = 0; i < ctx->aggregatorEndpointCount; i++) {
            res = KSI_AsyncService_addEndpoint(as,
                    ctx->aggregatorEndpoints[i], ctx->aggregatorId, ctx->aggregatorKey);
            if (res != KSI_OK) {
                //This can fail if the protocol is not supported by async api.
                reportKSIAPIErr(ctx, NULL, "KSI_AsyncService_addEndpoint", res);
                continue;
            }

            endpoints++;
        }
    }

    if(endpoints == 0) { /* no endpoint accepted, deleting the service */
        report(ctx, "No endpoints added, signing service disabled");
        ctx->disabled = true;
        KSI_AsyncService_free(as);
        as=NULL;
        goto cleanup;
    }

    /* Lets use buffer value, as libksi requires size_t. */
    size_t_value = ctx->max_requests;
    KSI_AsyncService_setOption(as, KSI_ASYNC_OPT_REQUEST_CACHE_SIZE,
        (void*)size_t_value);
    size_t_value = ctx->blockSigTimeout;
    KSI_AsyncService_setOption(as, KSI_ASYNC_OPT_SND_TIMEOUT,
        (void*)size_t_value);


    ctx->signer_state = SIGNER_STARTED;
    while (true) {
        QueueItem *item = NULL;

        if (isAggrConfNeeded(ctx)) {
            request_async_config(ctx, ksi_ctx, as);
        }

        /* Wait for a work item or timeout*/
        if (bSleep) {
            ProtectedQueue_waitForItem(ctx->signer_queue, NULL, ctx->threadSleepms);
        }
        bSleep = true;

        /* Check for block time limit. */
        sigblkCheckTimeOut(ctx);

        /* in case there are no items go around*/
        if (ProtectedQueue_count(ctx->signer_queue) == 0) {
            process_requests_async(ctx, ksi_ctx, as);
            continue;
        }

        /* process signing requests only if there is an open signature file */
        if(ksiFileCount > 0) {
            /* check for pending/unsent requests in asynchronous service */
            if(!process_requests_async(ctx, ksi_ctx, as)) {
                // probably fatal error, disable signing, error should be already reported
                ctx->disabled = true;
                goto cleanup;
            }
        }
        /* if there are sig. requests still in the front, then we have to start over*/
        if (ProtectedQueue_peekFront(ctx->signer_queue, (void**) &item)
            && item->type == QITEM_SIGNATURE_REQUEST)
            continue;

        /* Handle other types of work items */
        if (ProtectedQueue_popFront(ctx->signer_queue, (void**) &item) != 0) {
            /* There is no point to sleep after processing non request type item
             * as there is great possibility that next item can already be
             * processed. */
            bSleep = false;

            if (item->type == QITEM_CLOSE_FILE) {
                if (item->file) {
                    fclose(item->file);
                    item->file = NULL;
                }

                if (ksiFileCount > 0) ksiFileCount--;
            } else if (item->type == QITEM_NEW_FILE) {
                ksiFileCount++;
            } else if (item->type == QITEM_QUIT) {
                free(item);

                /* Will look into work queue for pending KSI signatures and will output
                 * unsigned block marker instead of actual KSI signature to finalize this
                 * thread quickly.
                 */
                rsksictxCloseAllPendingBlocksWithoutSignature(ctx,
                    "Signing not finished due to sudden closure of lmsig_ksi-ls12 module.");
                rsksictxForceCloseWithoutSig(ctx,
                    "Block closed due to sudden closure of lmsig_ksi-ls12 module.");

                goto cleanup;
            }

            free(item);
        }
    }

cleanup:

    KSI_AsyncService_free(as);
    KSI_CTX_free(ksi_ctx);
    ctx->signer_state = SIGNER_STOPPED;

    return NULL;
}
#pragma GCC diagnostic push
