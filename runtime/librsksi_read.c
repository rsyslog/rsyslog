/* librsksi_read.c - rsyslog's guardtime support library
 * This includes functions used for reading signature (and
 * other related) files. Well, actually it also contains
 * some writing functionality, but only as far as rsyslog
 * itself is not concerned, but "just" the utility programs.
 *
 * This part of the library uses C stdio and expects that the
 * caller will open and close the file to be read itself.
 *
 * Copyright 2013-2016 Adiscon GmbH.
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
#include <stdio.h>
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
#include <ksi/ksi.h>

#include "rsyslog.h"
#include "librsgt_common.h"
#include "librsksi.h"


#ifndef VERSION
    #define VERSION "no-version"
#endif

static int rsksi_read_debug = 0;
const char *rsksi_read_puburl = ""; /* old default http://verify.guardtime.com/gt-controlpublications.bin";*/
const char *rsksi_extend_puburl = ""; /* old default "http://verifier.guardtime.net/gt-extendingservice";*/
const char *rsksi_userid = "";
const char *rsksi_userkey = "";
uint8_t rsksi_read_showVerified = 0;

/* macro to obtain next char from file including error tracking */
#define NEXTC                                \
    if ((c = fgetc(fp)) == EOF) {            \
        r = feof(fp) ? RSGTE_EOF : RSGTE_IO; \
        goto done;                           \
    }

/* if verbose==0, only the first and last two octets are shown,
 * otherwise everything.
 */
static void outputHexBlob(FILE *fp, const uint8_t *blob, const uint16_t len, const uint8_t verbose) {
    unsigned i;
    if (verbose || len <= 8) {
        for (i = 0; i < len; ++i) fprintf(fp, "%2.2x", blob[i]);
    } else {
        fprintf(fp, "%2.2x%2.2x%2.2x[...]%2.2x%2.2x%2.2x", blob[0], blob[1], blob[2], blob[len - 3], blob[len - 2],
                blob[len - 1]);
    }
}

void outputKSIHash(FILE *fp, const char *hdr, const KSI_DataHash *const __restrict__ hash, const uint8_t verbose) {
    const unsigned char *digest;
    size_t digest_len;

    fprintf(fp, "%s", hdr);
    if (KSI_DataHash_extract(hash, NULL, &digest, &digest_len) != KSI_OK) {
        fprintf(fp, "<error: extraction failed>\n");
        return;
    }
    outputHexBlob(fp, digest, digest_len, verbose);
    fputc('\n', fp);
}

void outputHash(FILE *fp, const char *hdr, const uint8_t *data, const uint16_t len, const uint8_t verbose) {
    fprintf(fp, "%s", hdr);
    outputHexBlob(fp, data, len, verbose);
    fputc('\n', fp);
}

void rsksi_errctxInit(ksierrctx_t *ectx) {
    ectx->fp = NULL;
    ectx->filename = NULL;
    ectx->recNum = 0;
    ectx->ksistate = 0;
    ectx->recNumInFile = 0;
    ectx->blkNum = 0;
    ectx->verbose = 0;
    ectx->errRec = NULL;
    ectx->frstRecInBlk = NULL;
    ectx->fileHash = NULL;
    ectx->lefthash = ectx->righthash = ectx->computedHash = NULL;
}
void rsksi_errctxExit(ksierrctx_t *ectx) {
    free(ectx->errRec);
    free(ectx->filename);
    free(ectx->frstRecInBlk);
}

/* note: we do not copy the record, so the caller MUST not destruct
 * it before processing of the record is completed. To remove the
 * current record without setting a new one, call this function
 * with rec==NULL.
 */
void rsksi_errctxSetErrRec(ksierrctx_t *ectx, char *rec) {
    free(ectx->errRec);
    ectx->errRec = strdup(rec);
}
/* This stores the block's first record. Here we copy the data,
 * as the caller will usually not preserve it long enough.
 */
void rsksi_errctxFrstRecInBlk(ksierrctx_t *ectx, char *rec) {
    free(ectx->frstRecInBlk);
    ectx->frstRecInBlk = strdup(rec);
}

static void reportError(const int errcode, ksierrctx_t *ectx) {
    if (ectx->fp != NULL) {
        fprintf(ectx->fp, "%s[%llu:%llu:%llu]: error[%u]: %s\n", ectx->filename, (long long unsigned)ectx->blkNum,
                (long long unsigned)ectx->recNum, (long long unsigned)ectx->recNumInFile, errcode,
                RSKSIE2String(errcode));
        if (ectx->frstRecInBlk != NULL) fprintf(ectx->fp, "\tBlock Start Record.: '%s'\n", ectx->frstRecInBlk);
        if (ectx->errRec != NULL) fprintf(ectx->fp, "\tRecord in Question.: '%s'\n", ectx->errRec);
        if (ectx->computedHash != NULL) {
            outputKSIHash(ectx->fp, "\tComputed Hash......: ", ectx->computedHash, ectx->verbose);
        }
        if (ectx->fileHash != NULL) {
            outputHash(ectx->fp, "\tSignature File Hash: ", ectx->fileHash->data, ectx->fileHash->len, ectx->verbose);
        }
        if (errcode == RSGTE_INVLD_TREE_HASH || errcode == RSGTE_INVLD_TREE_HASHID) {
            fprintf(ectx->fp, "\tTree Level.........: %d\n", (int)ectx->treeLevel);
            outputKSIHash(ectx->fp, "\tTree Left Hash.....: ", ectx->lefthash, ectx->verbose);
            outputKSIHash(ectx->fp, "\tTree Right Hash....: ", ectx->righthash, ectx->verbose);
        }
        if (errcode == RSGTE_INVLD_SIGNATURE || errcode == RSGTE_TS_CREATEHASH) {
            fprintf(ectx->fp, "\tPublication Server.: %s\n", rsksi_read_puburl);
            fprintf(ectx->fp, "\tKSI Verify Signature: [%u]%s\n", ectx->ksistate, KSI_getErrorString(ectx->ksistate));
        }
        if (errcode == RSGTE_SIG_EXTEND || errcode == RSGTE_TS_CREATEHASH) {
            fprintf(ectx->fp, "\tExtending Server...: %s\n", rsksi_extend_puburl);
            fprintf(ectx->fp, "\tKSI Extend Signature: [%u]%s\n", ectx->ksistate, KSI_getErrorString(ectx->ksistate));
        }
        if (errcode == RSGTE_TS_DERENCODE) {
            fprintf(ectx->fp, "\tAPI return state...: [%u]%s\n", ectx->ksistate, KSI_getErrorString(ectx->ksistate));
        }
    }
}

/* obviously, this is not an error-reporting function. We still use
 * ectx, as it has most information we need.
 */
static void reportVerifySuccess(ksierrctx_t *ectx) /*OLD CODE , GTVerificationInfo *vrfyInf)*/
{
    fprintf(stdout, "%s[%llu:%llu:%llu]: block signature successfully verified\n", ectx->filename,
            (long long unsigned)ectx->blkNum, (long long unsigned)ectx->recNum, (long long unsigned)ectx->recNumInFile);
    if (ectx->frstRecInBlk != NULL) fprintf(stdout, "\tBlock Start Record.: '%s'\n", ectx->frstRecInBlk);
    if (ectx->errRec != NULL) fprintf(stdout, "\tBlock End Record...: '%s'\n", ectx->errRec);
    fprintf(stdout, "\tKSI Verify Signature: [%u]%s\n", ectx->ksistate, KSI_getErrorString(ectx->ksistate));
}

/* return the actual length in to-be-written octets of an integer */
static uint8_t rsksi_tlvGetInt64OctetSize(uint64_t val) {
    if (val >> 56) return 8;
    if ((val >> 48) & 0xff) return 7;
    if ((val >> 40) & 0xff) return 6;
    if ((val >> 32) & 0xff) return 5;
    if ((val >> 24) & 0xff) return 4;
    if ((val >> 16) & 0xff) return 3;
    if ((val >> 8) & 0xff) return 2;
    return 1;
}

static int rsksi_tlvfileAddOctet(FILE *newsigfp, int8_t octet) {
    /* Directory write into file */
    int r = 0;
    if (fputc(octet, newsigfp) == EOF) r = RSGTE_IO;
    return r;
}
static int rsksi_tlvfileAddOctetString(FILE *newsigfp, uint8_t *octet, int size) {
    int i, r = 0;
    for (i = 0; i < size; ++i) {
        r = rsksi_tlvfileAddOctet(newsigfp, octet[i]);
        if (r != 0) goto done;
    }
done:
    return r;
}
static int rsksi_tlvfileAddInt64(FILE *newsigfp, uint64_t val) {
    uint8_t doWrite = 0;
    int r;
    if (val >> 56) {
        r = rsksi_tlvfileAddOctet(newsigfp, (val >> 56) & 0xff), doWrite = 1;
        if (r != 0) goto done;
    }
    if (doWrite || ((val >> 48) & 0xff)) {
        r = rsksi_tlvfileAddOctet(newsigfp, (val >> 48) & 0xff), doWrite = 1;
        if (r != 0) goto done;
    }
    if (doWrite || ((val >> 40) & 0xff)) {
        r = rsksi_tlvfileAddOctet(newsigfp, (val >> 40) & 0xff), doWrite = 1;
        if (r != 0) goto done;
    }
    if (doWrite || ((val >> 32) & 0xff)) {
        r = rsksi_tlvfileAddOctet(newsigfp, (val >> 32) & 0xff), doWrite = 1;
        if (r != 0) goto done;
    }
    if (doWrite || ((val >> 24) & 0xff)) {
        r = rsksi_tlvfileAddOctet(newsigfp, (val >> 24) & 0xff), doWrite = 1;
        if (r != 0) goto done;
    }
    if (doWrite || ((val >> 16) & 0xff)) {
        r = rsksi_tlvfileAddOctet(newsigfp, (val >> 16) & 0xff), doWrite = 1;
        if (r != 0) goto done;
    }
    if (doWrite || ((val >> 8) & 0xff)) {
        r = rsksi_tlvfileAddOctet(newsigfp, (val >> 8) & 0xff), doWrite = 1;
        if (r != 0) goto done;
    }
    r = rsksi_tlvfileAddOctet(newsigfp, val & 0xff);
done:
    return r;
}

static int rsksi_tlv8Write(FILE *newsigfp, int flags, int tlvtype, int len) {
    int r;
    assert((flags & RSGT_TYPE_MASK) == 0);
    assert((tlvtype & RSGT_TYPE_MASK) == tlvtype);
    r = rsksi_tlvfileAddOctet(newsigfp, (flags & ~RSKSI_FLAG_TLV16_RUNTIME) | tlvtype);
    if (r != 0) goto done;
    r = rsksi_tlvfileAddOctet(newsigfp, len & 0xff);
done:
    return r;
}

static int rsksi_tlv16Write(FILE *newsigfp, int flags, int tlvtype, uint16_t len) {
    uint16_t typ;
    int r;
    assert((flags & RSGT_TYPE_MASK) == 0);
    assert((tlvtype >> 8 & RSGT_TYPE_MASK) == (tlvtype >> 8));
    typ = ((flags | RSKSI_FLAG_TLV16_RUNTIME) << 8) | tlvtype;
    r = rsksi_tlvfileAddOctet(newsigfp, typ >> 8);
    if (r != 0) goto done;
    r = rsksi_tlvfileAddOctet(newsigfp, typ & 0xff);
    if (r != 0) goto done;
    r = rsksi_tlvfileAddOctet(newsigfp, (len >> 8) & 0xff);
    if (r != 0) goto done;
    r = rsksi_tlvfileAddOctet(newsigfp, len & 0xff);
done:
    return r;
}

/**
 * Write the provided record to the current file position.
 *
 * @param[in] fp file pointer for writing
 * @param[out] rec tlvrecord to write
 *
 * @returns 0 if ok, something else otherwise
 */
int rsksi_tlvwrite(FILE *fp, tlvrecord_t *rec) {
    int r = RSGTE_IO;
    if (fwrite(rec->hdr, (size_t)rec->lenHdr, 1, fp) != 1) goto done;
    if (fwrite(rec->data, (size_t)rec->tlvlen, 1, fp) != 1) goto done;
    r = 0;
done:
    return r;
}
/*
int
rsksi_tlvWriteHashKSI(FILE *fp, ksifile ksi, uint16_t tlvtype, KSI_DataHash *rec)
{
    unsigned tlvlen;
    int r;
    const unsigned char *digest;
    size_t digest_len;
    r = KSI_DataHash_extract(rec, NULL, &digest, &digest_len);
    if (r != KSI_OK){
        reportKSIAPIErr(ksi->ctx, ksi, "KSI_DataHash_extract", r);
        goto done;
    }
    tlvlen = 1 + digest_len;
    r = rsksi_tlv16Write(fp, 0x00, tlvtype, tlvlen);
    if(r != 0) goto done;
    r = rsksi_tlvfileAddOctet(fp, hashIdentifierKSI(ksi->hashAlg));
    if(r != 0) goto done;
    r = rsksi_tlvfileAddOctetString(fp, (unsigned char*)digest, digest_len);
done:	return r;
}
*/

/**
 * Read a header from a binary file.
 * @param[in] fp file pointer for processing
 * @param[in] hdr buffer for the header. Must be 9 bytes
 * 		  (8 for header + NUL byte)
 * @returns 0 if ok, something else otherwise
 */
int rsksi_tlvrdHeader(FILE *fp, uchar *hdr) {
    int r;
    if (fread(hdr, 8, 1, fp) != 1) {
        r = RSGTE_IO;
        goto done;
    }
    hdr[8] = '\0';
    r = 0;
done:
    return r;
}

/* read type a complete tlv record
 */
static int rsksi_tlvRecRead(FILE *fp, tlvrecord_t *rec) {
    int r = 1;
    int c;
    /* Init record variables */
    rec->tlvtype = 0;
    rec->tlvlen = 0;

    NEXTC;
    rec->hdr[0] = c;
    rec->tlvtype = c & 0x1f;
    if (c & RSKSI_FLAG_TLV16_RUNTIME) { /* tlv16? */
        rec->lenHdr = 4;
        NEXTC;
        rec->hdr[1] = c;
        rec->tlvtype = (rec->tlvtype << 8) | c;
        NEXTC;
        rec->hdr[2] = c;
        rec->tlvlen = c << 8;
        NEXTC;
        rec->hdr[3] = c;
        rec->tlvlen |= c;
    } else {
        NEXTC;
        rec->lenHdr = 2;
        rec->hdr[1] = c;
        rec->tlvlen = c;
    }
    if ((r = fread(rec->data, (size_t)rec->tlvlen, 1, fp)) != 1) {
        r = feof(fp) ? RSGTE_EOF : RSGTE_IO;
        goto done;
    }

    r = 0;
done:
    /* Only show debug if no fail */
    if (rsksi_read_debug && r != 0 && r != RSGTE_EOF)
        printf("debug: rsksi_tlvRecRead:\t read tlvtype %4.4x, len %u r=%d\n", (unsigned)rec->tlvtype,
               (unsigned)rec->tlvlen, r);
    return r;
}

/* decode a sub-tlv record from an existing record's memory buffer
 */
static int rsksi_tlvDecodeSUBREC(tlvrecord_t *rec, uint16_t *stridx, tlvrecord_t *newrec) {
    int r = 1;
    int c;

    if (rec->tlvlen == *stridx) {
        r = RSGTE_LEN;
        if (rsksi_read_debug)
            printf(
                "debug: rsksi_tlvDecodeSUBREC:\t\t "
                "break #1\n");
        goto done;
    }
    c = rec->data[(*stridx)++];
    newrec->hdr[0] = c;
    newrec->tlvtype = c & 0x1f;
    if (c & RSKSI_FLAG_TLV16_RUNTIME) { /* tlv16? */
        newrec->lenHdr = 4;
        if (rec->tlvlen == *stridx) {
            r = RSGTE_LEN;
            if (rsksi_read_debug)
                printf(
                    "debug: rsksi_tlvDecodeSUBREC:"
                    "\t\t break #2\n");
            goto done;
        }
        c = rec->data[(*stridx)++];
        newrec->hdr[1] = c;
        newrec->tlvtype = (newrec->tlvtype << 8) | c;
        if (rec->tlvlen == *stridx) {
            r = RSGTE_LEN;
            if (rsksi_read_debug)
                printf(
                    "debug: rsksi_tlvDecodeSUBREC:"
                    "\t\t break #3\n");
            goto done;
        }
        c = rec->data[(*stridx)++];
        newrec->hdr[2] = c;
        newrec->tlvlen = c << 8;
        if (rec->tlvlen == *stridx) {
            r = RSGTE_LEN;
            if (rsksi_read_debug)
                printf(
                    "debug: rsksi_tlvDecodeSUBREC:"
                    "\t\t break #4\n");
            goto done;
        }
        c = rec->data[(*stridx)++];
        newrec->hdr[3] = c;
        newrec->tlvlen |= c;
    } else {
        if (rec->tlvlen == *stridx) {
            r = RSGTE_LEN;
            if (rsksi_read_debug)
                printf(
                    "debug: rsksi_tlvDecodeSUBREC:"
                    "\t\t break #5\n");
            goto done;
        }
        c = rec->data[(*stridx)++];
        newrec->lenHdr = 2;
        newrec->hdr[1] = c;
        newrec->tlvlen = c;
    }
    if (rec->tlvlen < *stridx + newrec->tlvlen) {
        r = RSGTE_LEN;
        if (rsksi_read_debug)
            printf(
                "debug: "
                "rsksi_tlvDecodeSUBREC:\t\t break rec->tlvlen=%d newrec->tlvlen=%d stridx=%d #6\n",
                rec->tlvlen, newrec->tlvlen, *stridx);
        goto done;
    }
    memcpy(newrec->data, (rec->data) + (*stridx), newrec->tlvlen);
    *stridx += newrec->tlvlen;

    if (rsksi_read_debug)
        printf("debug: rsksi_tlvDecodeSUBREC:\t\t Read subtlv: tlvtype %4.4x, len %u\n", (unsigned)newrec->tlvtype,
               (unsigned)newrec->tlvlen);
    r = 0;
done:
    if (r != 0) /* Only on FAIL! */
        printf("debug: rsksi_tlvDecodeSUBREC:\t\t Failed, tlv record %4.4x with error %d\n", rec->tlvtype, r);
    return r;
}

int rsksi_tlvDecodeIMPRINT(tlvrecord_t *rec, imprint_t **imprint) {
    int r = 1;
    imprint_t *imp = NULL;

    if ((imp = calloc(1, sizeof(imprint_t))) == NULL) {
        r = RSGTE_OOM;
        goto done;
    }

    imp->hashID = rec->data[0];
    if (rec->tlvlen != 1 + hashOutputLengthOctetsKSI(imp->hashID)) {
        r = RSGTE_LEN;
        goto done;
    }
    imp->len = rec->tlvlen - 1;
    if ((imp->data = (uint8_t *)malloc(imp->len)) == NULL) {
        r = RSGTE_OOM;
        goto done;
    }
    memcpy(imp->data, rec->data + 1, imp->len);
    *imprint = imp;
    r = 0;
done:
    if (r == 0) {
        if (rsksi_read_debug)
            printf(
                "debug: rsksi_tlvDecodeIMPRINT:\t\t returned %d TLVType=%4.4x, "
                "TLVLen=%d, HashID=%d\n",
                r, rec->tlvtype, rec->tlvlen, imp->hashID);
        if (rsksi_read_debug) outputHash(stdout, "debug: rsksi_tlvDecodeIMPRINT:\t\t hash: ", imp->data, imp->len, 1);
    } else {
        /* Free memory on FAIL!*/
        printf("debug: rsksi_tlvDecodeIMPRINT:\t\t Failed, tlv record %4.4x with error %d\n", rec->tlvtype, r);
        if (imp != NULL) rsksi_objfree(rec->tlvtype, imp);
    }
    return r;
}
static int rsksi_tlvDecodeSIB_HASH(tlvrecord_t *rec, uint16_t *strtidx, imprint_t *imp) {
    int r = 1;
    tlvrecord_t subrec;

    CHKr(rsksi_tlvDecodeSUBREC(rec, strtidx, &subrec));
    if (!(subrec.tlvtype == 0x02)) {
        r = RSGTE_INVLTYP;
        goto done;
    }
    imp->hashID = subrec.data[0];
    if (subrec.tlvlen != 1 + hashOutputLengthOctetsKSI(imp->hashID)) {
        r = RSGTE_LEN;
        goto done;
    }
    imp->len = subrec.tlvlen - 1;
    if ((imp->data = (uint8_t *)malloc(imp->len)) == NULL) {
        r = RSGTE_OOM;
        goto done;
    }
    memcpy(imp->data, subrec.data + 1, subrec.tlvlen - 1);
    r = 0;
done:
    return r;
}
static int rsksi_tlvDecodeREC_HASH(tlvrecord_t *rec, uint16_t *strtidx, imprint_t *imp) {
    int r = 1;
    tlvrecord_t subrec;
    CHKr(rsksi_tlvDecodeSUBREC(rec, strtidx, &subrec));
    if (!(subrec.tlvtype == 0x01)) {
        r = RSGTE_INVLTYP;
        goto done;
    }
    imp->hashID = subrec.data[0];

    if (subrec.tlvlen != 1 + hashOutputLengthOctetsKSI(imp->hashID)) {
        if (rsksi_read_debug)
            printf(
                "debug: rsksi_tlvDecodeREC_HASH:\t\t FAIL on subrec.tlvtype %4.4x "
                "subrec.tlvlen = %d\n",
                subrec.tlvtype, subrec.tlvlen);
        r = RSGTE_LEN;
        goto done;
    }
    imp->len = subrec.tlvlen - 1;
    if ((imp->data = (uint8_t *)malloc(imp->len)) == NULL) {
        r = RSGTE_OOM;
        goto done;
    }
    memcpy(imp->data, subrec.data + 1, subrec.tlvlen - 1);
    r = 0;
done:
    if (r == 0) {
        if (rsksi_read_debug)
            printf(
                "debug: rsksi_tlvDecodeREC_HASH:\t\t returned %d TLVType=%4.4x, "
                "TLVLen=%d\n",
                r, rec->tlvtype, rec->tlvlen);
    } else
        printf("debug: rsksi_tlvDecodeREC_HASH:\t\t Failed, TLVType=%4.4x, TLVLen=%d with error %d\n", rec->tlvtype,
               rec->tlvlen, r);

    return r;
}
static int rsksi_tlvDecodeLEVEL_CORR(tlvrecord_t *rec, uint16_t *strtidx, uint8_t *levelcorr) {
    int r = 1;
    tlvrecord_t subrec;

    CHKr(rsksi_tlvDecodeSUBREC(rec, strtidx, &subrec));
    if (!(subrec.tlvtype == 0x01 && subrec.tlvlen == 1)) {
        if (rsksi_read_debug)
            printf(
                "debug: rsksi_tlvDecodeLEVEL_CORR:\t FAIL on subrec.tlvtype "
                "%4.4x subrec.tlvlen = %d\n",
                subrec.tlvtype, subrec.tlvlen);
        r = RSGTE_FMT;
        goto done;
    }
    *levelcorr = subrec.data[0];
    r = 0;
done:
    if (r == 0) {
        if (rsksi_read_debug)
            printf(
                "debug: rsksi_tlvDecodeLEVEL_CORR:\t returned %d TLVType=%4.4x, "
                "TLVLen=%d\n",
                r, rec->tlvtype, rec->tlvlen);
    } else
        printf("debug: rsksi_tlvDecodeLEVEL_CORR:\t Failed, tlv record %4.4x with error %d\n", rec->tlvtype, r);
    return r;
}

static int rsksi_tlvDecodeHASH_STEP(tlvrecord_t *rec, uint16_t *pstrtidx, block_hashstep_t **blhashstep) {
    int r = 1;
    uint16_t strtidx = 0;
    tlvrecord_t subrec;
    *blhashstep = NULL; /* Set to NULL by default first */

    /* Init HashStep */
    block_hashstep_t *hashstep = NULL;
    if ((hashstep = calloc(1, sizeof(block_hashstep_t))) == NULL) {
        r = RSGTE_OOM;
        goto done;
    }
    hashstep->sib_hash.data = NULL;

    /* Get Haststep Subrecord now */
    CHKr(rsksi_tlvDecodeSUBREC(rec, pstrtidx, &subrec)); /* Add to external counter */
    hashstep->direction = subrec.tlvtype; /* TLVType is also the DIRECTION! */

    /* Extract HASH and LEVEL Correction!*/
    CHKr(rsksi_tlvDecodeLEVEL_CORR(&subrec, &strtidx, &(hashstep->level_corr)));
    CHKr(rsksi_tlvDecodeSIB_HASH(&subrec, &strtidx, &(hashstep->sib_hash)));

    if (strtidx != subrec.tlvlen) {
        r = RSGTE_LEN;
        goto done;
    }

    *blhashstep = hashstep;
    r = 0;
done:
    if (r == 0) {
        if (rsksi_read_debug)
            printf(
                "debug: rsksi_tlvDecodeHASH_STEP:\t returned %d, tlvtype "
                "%4.4x\n",
                r, (unsigned)rec->tlvtype);
    } else {
        /* Free memory on FAIL!*/
        printf("debug: rsksi_tlvDecodeHASH_STEP:\t Failed, tlv record %4.4x with error %d\n", rec->tlvtype, r);
        if (hashstep != NULL) {
            if (hashstep->sib_hash.data != NULL) free(hashstep->sib_hash.data);
            free(hashstep);
        }
    }
    return r;
}
static int rsksi_tlvDecodeHASH_CHAIN(tlvrecord_t *rec, block_hashchain_t **blhashchain) {
    int r = 1;
    uint16_t strtidx = 0;
    /* Init HashChain Object */
    block_hashchain_t *hashchain = NULL;
    if ((hashchain = calloc(1, sizeof(block_hashchain_t))) == NULL) {
        r = RSGTE_OOM;
        goto done;
    }
    hashchain->rec_hash.data = NULL;
    hashchain->stepCount = 0;
    hashchain->level = 0;

    /* Extract hash chain */
    CHKr(rsksi_tlvDecodeREC_HASH(rec, &strtidx, &(hashchain->rec_hash)));

    /* Loop until all Steps have been processed */
    while (rec->tlvlen > strtidx) {
        CHKr(rsksi_tlvDecodeHASH_STEP(rec, &strtidx, &(hashchain->hashsteps[hashchain->stepCount++])));
        if (rsksi_read_debug)
            printf("debug: rsksi_tlvDecodeHASH_CHAIN:\t tlvlen=%d strtidx=%d\n", rec->tlvlen, strtidx);
    }

    *blhashchain = hashchain;
    r = 0;
done:
    if (r == 0) {
        if (rsksi_read_debug)
            printf(
                "debug: rsksi_tlvDecodeHASH_CHAIN:\t returned %d TLVType=%4.4x, "
                "TLVLen=%d\n",
                r, rec->tlvtype, rec->tlvlen);
    } else {
        /* Free memory on FAIL!*/
        printf("debug: rsksi_tlvDecodeHASH_CHAIN:\t Failed, TLVType=%4.4x, TLVLen=%d with error %d\n", rec->tlvtype,
               rec->tlvlen, r);
        if (hashchain != NULL) rsksi_objfree(rec->tlvtype, hashchain);
    }
    return r;
}

static int rsksi_tlvDecodeHASH_ALGO(tlvrecord_t *rec, uint16_t *strtidx, uint8_t *hashAlg) {
    int r = 1;
    tlvrecord_t subrec;

    CHKr(rsksi_tlvDecodeSUBREC(rec, strtidx, &subrec));
    if (!(subrec.tlvtype == 0x01 && subrec.tlvlen == 1)) {
        r = RSGTE_FMT;
        goto done;
    }
    *hashAlg = subrec.data[0];
    r = 0;
done:
    return r;
}
static int rsksi_tlvDecodeBLOCK_IV(tlvrecord_t *rec, uint16_t *strtidx, uint8_t **iv) {
    int r = 1;
    tlvrecord_t subrec;

    CHKr(rsksi_tlvDecodeSUBREC(rec, strtidx, &subrec));
    if (!(subrec.tlvtype == 0x02)) {
        r = RSGTE_INVLTYP;
        goto done;
    }
    if ((*iv = (uint8_t *)malloc(subrec.tlvlen)) == NULL) {
        r = RSGTE_OOM;
        goto done;
    }
    memcpy(*iv, subrec.data, subrec.tlvlen);
    r = 0;
done:
    return r;
}
static int rsksi_tlvDecodeLAST_HASH(tlvrecord_t *rec, uint16_t *strtidx, imprint_t *imp) {
    int r = 1;
    tlvrecord_t subrec;

    CHKr(rsksi_tlvDecodeSUBREC(rec, strtidx, &subrec));
    if (!(subrec.tlvtype == 0x03)) {
        r = RSGTE_INVLTYP;
        goto done;
    }
    imp->hashID = subrec.data[0];
    if (subrec.tlvlen != 1 + hashOutputLengthOctetsKSI(imp->hashID)) {
        r = RSGTE_LEN;
        goto done;
    }
    imp->len = subrec.tlvlen - 1;
    if ((imp->data = (uint8_t *)malloc(imp->len)) == NULL) {
        r = RSGTE_OOM;
        goto done;
    }
    memcpy(imp->data, subrec.data + 1, subrec.tlvlen - 1);
    r = 0;
done:
    return r;
}
static int rsksi_tlvDecodeREC_COUNT(tlvrecord_t *rec, uint16_t *strtidx, uint64_t *cnt) {
    int r = 1;
    int i;
    uint64_t val;
    tlvrecord_t subrec;

    CHKr(rsksi_tlvDecodeSUBREC(rec, strtidx, &subrec));
    if (!(subrec.tlvtype == 0x01 && subrec.tlvlen <= 8)) {
        r = RSGTE_INVLTYP;
        goto done;
    }
    val = 0;
    for (i = 0; i < subrec.tlvlen; ++i) {
        val = (val << 8) + subrec.data[i];
    }
    *cnt = val;
    r = 0;
done:
    return r;
}
static int rsksi_tlvDecodeSIG(tlvrecord_t *rec, uint16_t *strtidx, block_sig_t *bs) {
    int r = 1;
    tlvrecord_t subrec;

    CHKr(rsksi_tlvDecodeSUBREC(rec, strtidx, &subrec));
    if (!(subrec.tlvtype == 0x0905)) {
        r = RSGTE_INVLTYP;
        goto done;
    }
    bs->sig.der.len = subrec.tlvlen;
    bs->sigID = SIGID_RFC3161;
    if ((bs->sig.der.data = (uint8_t *)malloc(bs->sig.der.len)) == NULL) {
        r = RSGTE_OOM;
        goto done;
    }
    memcpy(bs->sig.der.data, subrec.data, bs->sig.der.len);
    r = 0;
done:
    if (rsksi_read_debug)
        printf("debug: rsksi_tlvDecodeSIG:\t\t returned %d, tlvtype %4.4x\n", r, (unsigned)rec->tlvtype);
    return r;
}

static int rsksi_tlvDecodeBLOCK_HDR(tlvrecord_t *rec, block_hdr_t **blockhdr) {
    int r = 1;
    uint16_t strtidx = 0;
    block_hdr_t *bh = NULL;
    if ((bh = calloc(1, sizeof(block_hdr_t))) == NULL) {
        r = RSGTE_OOM;
        goto done;
    }
    CHKr(rsksi_tlvDecodeHASH_ALGO(rec, &strtidx, &(bh->hashID)));
    CHKr(rsksi_tlvDecodeBLOCK_IV(rec, &strtidx, &(bh->iv)));
    CHKr(rsksi_tlvDecodeLAST_HASH(rec, &strtidx, &(bh->lastHash)));
    if (strtidx != rec->tlvlen) {
        r = RSGTE_LEN;
        goto done;
    }
    *blockhdr = bh;
    r = 0;
done:
    if (r == 0) {
        if (rsksi_read_debug)
            printf("debug: tlvDecodeBLOCK_HDR:\t\t returned %d, tlvtype %4.4x\n", r, (unsigned)rec->tlvtype);
    } else {
        /* Free memory on FAIL!*/
        if (bh != NULL) rsksi_objfree(rec->tlvtype, bh);
    }
    return r;
}

static int rsksi_tlvDecodeEXCERPT_SIG(tlvrecord_t *rec, block_sig_t **blocksig) {
    int r = 1;
    block_sig_t *bs = NULL;
    if ((bs = calloc(1, sizeof(block_sig_t))) == NULL) {
        r = RSGTE_OOM;
        goto done;
    }

    /* Read signature now */
    if (!(rec->tlvtype == 0x0905)) {
        r = RSGTE_INVLTYP;
        goto done;
    }
    bs->recCount = 0;
    bs->sig.der.len = rec->tlvlen;
    bs->sigID = SIGID_RFC3161;
    if ((bs->sig.der.data = (uint8_t *)malloc(bs->sig.der.len)) == NULL) {
        r = RSGTE_OOM;
        goto done;
    }
    memcpy(bs->sig.der.data, rec->data, bs->sig.der.len);

    *blocksig = bs;
    r = 0;
done:
    if (r == 0) {
        if (rsksi_read_debug)
            printf("debug: tlvDecodeEXCERPT_SIG:\t returned %d, tlvtype %4.4x\n", r, (unsigned)rec->tlvtype);
    } else {
        /* Free memory on FAIL!*/
        if (bs != NULL) rsksi_objfree(rec->tlvtype, bs);
    }
    return r;
}
static int rsksi_tlvDecodeBLOCK_SIG(tlvrecord_t *rec, block_sig_t **blocksig) {
    int r = 1;
    uint16_t strtidx = 0;
    block_sig_t *bs = NULL;
    if ((bs = calloc(1, sizeof(block_sig_t))) == NULL) {
        r = RSGTE_OOM;
        goto done;
    }
    CHKr(rsksi_tlvDecodeREC_COUNT(rec, &strtidx, &(bs->recCount)));
    if (strtidx < rec->tlvlen) CHKr(rsksi_tlvDecodeSIG(rec, &strtidx, bs));
    if (strtidx != rec->tlvlen) {
        r = RSGTE_LEN;
        goto done;
    }
    *blocksig = bs;
    r = 0;
done:
    if (r == 0) {
        if (rsksi_read_debug)
            printf(
                "debug: tlvDecodeBLOCK_SIG:\t\t returned %d, tlvtype %4.4x, recCount "
                "%ju\n",
                r, (unsigned)rec->tlvtype, bs->recCount);
    } else {
        /* Free memory on FAIL!*/
        if (bs != NULL) rsksi_objfree(rec->tlvtype, bs);
    }
    return r;
}
int rsksi_tlvRecDecode(tlvrecord_t *rec, void *obj) {
    int r = 1;
    switch (rec->tlvtype) {
        case 0x0901:
            r = rsksi_tlvDecodeBLOCK_HDR(rec, obj);
            if (r != 0) goto done;
            break;
        case 0x0902:
        case 0x0903:
            r = rsksi_tlvDecodeIMPRINT(rec, obj);
            if (r != 0) goto done;
            break;
        case 0x0904:
            r = rsksi_tlvDecodeBLOCK_SIG(rec, obj);
            if (r != 0) goto done;
            break;
        case 0x0905:
            r = rsksi_tlvDecodeEXCERPT_SIG(rec, obj);
            if (r != 0) goto done;
            break;
        case 0x0907:
            r = rsksi_tlvDecodeHASH_CHAIN(rec, obj);
            if (r != 0) goto done;
            break;
    }
done:
    if (rsksi_read_debug)
        printf("debug: rsksi_tlvRecDecode:\t\t returned %d, tlvtype %4.4x\n", r, (unsigned)rec->tlvtype);
    return r;
}

static int rsksi_tlvrdRecHash(FILE *fp, FILE *outfp, imprint_t **imp) {
    int r;
    tlvrecord_t rec;

    if ((r = rsksi_tlvrd(fp, &rec, imp)) != 0) goto done;
    if (rec.tlvtype != 0x0902) {
        if (rsksi_read_debug)
            printf(
                "debug: rsksi_tlvrdRecHash:\t\t\t expected tlvtype 0x0902, "
                "but was %4.4x\n",
                rec.tlvtype);
        r = RSGTE_MISS_REC_HASH;
        rsksi_objfree(rec.tlvtype, *imp);
        *imp = NULL;
        goto done;
    }
    if (outfp != NULL) {
        if ((r = rsksi_tlvwrite(outfp, &rec)) != 0) goto done;
    }
    r = 0;
done:
    if (r == 0 && rsksi_read_debug)
        printf("debug: tlvrdRecHash:\t\t\t returned %d, rec->tlvtype %4.4x\n", r, (unsigned)rec.tlvtype);
    return r;
}

static int rsksi_tlvrdTreeHash(FILE *fp, FILE *outfp, imprint_t **imp) {
    int r;
    tlvrecord_t rec;

    if ((r = rsksi_tlvrd(fp, &rec, imp)) != 0) goto done;
    if (rec.tlvtype != 0x0903) {
        if (rsksi_read_debug)
            printf(
                "debug: rsksi_tlvrdTreeHash:\t\t expected tlvtype 0x0903, "
                "but was %4.4x\n",
                rec.tlvtype);
        r = RSGTE_MISS_TREE_HASH;
        rsksi_objfree(rec.tlvtype, *imp);
        *imp = NULL;
        goto done;
    }
    if (outfp != NULL) {
        if ((r = rsksi_tlvwrite(outfp, &rec)) != 0) goto done;
    }
    r = 0;
done:
    if (r == 0 && rsksi_read_debug)
        printf("debug: rsksi_tlvrdTreeHash:\t\t returned %d, rec->tlvtype %4.4x\n", r, (unsigned)rec.tlvtype);
    return r;
}

/* read BLOCK_SIG during verification phase */
static int rsksi_tlvrdVrfyBlockSig(FILE *fp, block_sig_t **bs, tlvrecord_t *rec) {
    int r;

    if ((r = rsksi_tlvrd(fp, rec, bs)) != 0) goto done;
    if (rec->tlvtype != 0x0904) {
        if (rsksi_read_debug)
            printf(
                "debug: rsksi_tlvrdVrfyBlockSig:\t expected tlvtype 0x0904, but "
                "was %4.4x\n",
                rec->tlvtype);
        r = RSGTE_MISS_BLOCKSIG;
        /* NOT HERE, done above ! rsksi_objfree(rec->tlvtype, *bs); */
        goto done;
    }
    r = 0;
done:
    return r;
}

/**
 * Read the next "object" from file. This usually is
 * a single TLV, but may be something larger, for
 * example in case of a block-sig TLV record.
 * Unknown type records are ignored (or run aborted
 * if we are not permitted to skip).
 *
 * @param[in] fp file pointer for processing
 * @param[out] tlvtype type of tlv record (top-level for
 * 		    structured objects.
 * @param[out] tlvlen length of the tlv record value
 * @param[out] obj pointer to object; This is a proper
 * 		   tlv record structure, which must be casted
 * 		   by the caller according to the reported type.
 * 		   The object must be freed by the caller (TODO: better way?)
 *
 * @returns 0 if ok, something else otherwise
 */
int rsksi_tlvrd(FILE *fp, tlvrecord_t *rec, void *obj) {
    int r;
    if ((r = rsksi_tlvRecRead(fp, rec)) != 0) goto done;
    r = rsksi_tlvRecDecode(rec, obj);
done:
    if (rsksi_read_debug && r != RSGTE_SUCCESS && r != RSGTE_EOF)
        printf(
            "debug: rsksi_tlvrd:\t failed with "
            "error %d\n",
            r);
    return r;
}


/* return if a blob is all zero */
static int blobIsZero(uint8_t *blob, uint16_t len) {
    int i;
    for (i = 0; i < len; ++i)
        if (blob[i] != 0) return 0;
    return 1;
}

static void rsksi_printIMPRINT(FILE *fp, const char *name, imprint_t *imp, uint8_t verbose) {
    fprintf(fp, "%s", name);
    outputHexBlob(fp, imp->data, imp->len, verbose);
    fputc('\n', fp);
}

static void rsksi_printREC_HASH(FILE *fp, imprint_t *imp, uint8_t verbose) {
    rsksi_printIMPRINT(fp, "[0x0902]Record hash: ", imp, verbose);
}

static void rsksi_printINT_HASH(FILE *fp, imprint_t *imp, uint8_t verbose) {
    rsksi_printIMPRINT(fp, "[0x0903]Tree hash..: ", imp, verbose);
}

/**
 * Output a human-readable representation of a block_hdr_t
 * to proviced file pointer. This function is mainly inteded for
 * debugging purposes or dumping tlv files.
 *
 * @param[in] fp file pointer to send output to
 * @param[in] bsig ponter to block_hdr_t to output
 * @param[in] verbose if 0, abbreviate blob hexdump, else complete
 */
void rsksi_printBLOCK_HDR(FILE *fp, block_hdr_t *bh, uint8_t verbose) {
    fprintf(fp, "[0x0901]Block Header Record:\n");
    fprintf(fp, "\tPrevious Block Hash:\n");
    fprintf(fp, "\t   Algorithm..: %s\n", hashAlgNameKSI(bh->lastHash.hashID));
    fprintf(fp, "\t   Hash.......: ");
    outputHexBlob(fp, bh->lastHash.data, bh->lastHash.len, verbose);
    fputc('\n', fp);
    if (blobIsZero(bh->lastHash.data, bh->lastHash.len)) fprintf(fp, "\t   NOTE: New Hash Chain Start!\n");
    fprintf(fp, "\tHash Algorithm: %s\n", hashAlgNameKSI(bh->hashID));
    fprintf(fp, "\tIV............: ");
    outputHexBlob(fp, bh->iv, getIVLenKSI(bh), verbose);
    fputc('\n', fp);
}


/**
 * Output a human-readable representation of a block_sig_t
 * to proviced file pointer. This function is mainly inteded for
 * debugging purposes or dumping tlv files.
 *
 * @param[in] fp file pointer to send output to
 * @param[in] bsig ponter to block_sig_t to output
 * @param[in] verbose if 0, abbreviate blob hexdump, else complete
 */
void rsksi_printBLOCK_SIG(FILE *fp, block_sig_t *bs, uint8_t verbose) {
    fprintf(fp, "[0x0904]Block Signature Record:\n");
    fprintf(fp, "\tRecord Count..: %llu\n", (long long unsigned)bs->recCount);
    fprintf(fp, "\tSignature Type: %s\n", sigTypeName(bs->sigID));
    fprintf(fp, "\tSignature Len.: %u\n", (unsigned)bs->sig.der.len);
    fprintf(fp, "\tSignature.....: ");
    outputHexBlob(fp, bs->sig.der.data, bs->sig.der.len, verbose);
    fputc('\n', fp);
}

/**
 * Output a human-readable representation of a block_hashchain_t
 * to proviced file pointer. This function is mainly inteded for
 * debugging purposes or dumping tlv files.
 *
 * @param[in] fp file pointer to send output to
 * @param[in] bsig ponter to block_sig_t to output
 * @param[in] verbose if 0, abbreviate blob hexdump, else complete
 */
static void rsksi_printHASHCHAIN(FILE *fp, block_sig_t *bs, uint8_t verbose) {
    fprintf(fp, "[0x0905]HashChain Start Record:\n");
    fprintf(fp, "\tSignature Type: %s\n", sigTypeName(bs->sigID));
    fprintf(fp, "\tSignature Len.: %u\n", (unsigned)bs->sig.der.len);
    outputHash(fp, "\tSignature.....: ", bs->sig.der.data, bs->sig.der.len, verbose);
}

/**
 * Output a human-readable representation of a block_hashchain_t
 * to proviced file pointer. This function is mainly inteded for
 * debugging purposes or dumping tlv files.
 *
 * @param[in] fp file pointer to send output to
 * @param[in] hashchain step ponter to block_hashstep_s to output
 * @param[in] verbose if 0, abbreviate blob hexdump, else complete
 */
static void rsksi_printHASHCHAINSTEP(FILE *fp, block_hashchain_t *hschain, uint8_t verbose) {
    uint8_t j;

    fprintf(fp, "[0x0907]HashChain Step:\n");
    fprintf(fp, "\tChain Count ....: %llu\n", (long long unsigned)hschain->stepCount);
    fprintf(fp, "\tRecord Hash Len.: %zd\n", hschain->rec_hash.len);
    outputHash(fp, "\tRecord Hash.....: ", hschain->rec_hash.data, hschain->rec_hash.len, verbose);

    for (j = 0; j < hschain->stepCount; ++j) {
        fprintf(fp, "\tDirection.....: %s\n", (hschain->hashsteps[j]->direction == 0x02) ? "LEFT" : "RIGHT");
        fprintf(fp, "\tLevel Correction.: %llu\n", (long long unsigned)hschain->hashsteps[j]->level_corr);
        fprintf(fp, "\tChain Hash Len.: %llu\n", (long long unsigned)hschain->hashsteps[j]->sib_hash.len);
        outputHash(fp, "\tRecord Hash.....: ", hschain->hashsteps[j]->sib_hash.data,
                   hschain->hashsteps[j]->sib_hash.len, verbose);
    }
}

/**
 * Output a human-readable representation of a tlv object.
 *
 * @param[in] fp file pointer to send output to
 * @param[in] tlvtype type of tlv object (record)
 * @param[in] verbose if 0, abbreviate blob hexdump, else complete
 */
void rsksi_tlvprint(FILE *fp, uint16_t tlvtype, void *obj, uint8_t verbose) {
    switch (tlvtype) {
        case 0x0901:
            rsksi_printBLOCK_HDR(fp, obj, verbose);
            break;
        case 0x0902:
            rsksi_printREC_HASH(fp, obj, verbose);
            break;
        case 0x0903:
            rsksi_printINT_HASH(fp, obj, verbose);
            break;
        case 0x0904:
            rsksi_printBLOCK_SIG(fp, obj, verbose);
            break;
        case 0x0905:
            rsksi_printHASHCHAIN(fp, obj, verbose);
            break;
        case 0x0907:
            rsksi_printHASHCHAINSTEP(fp, obj, verbose);
            break;
        default:
            fprintf(fp, "rsksi_tlvprint :\t unknown tlv record %4.4x\n", tlvtype);
            break;
    }
}

/**
 * Free the provided object.
 *
 * @param[in] tlvtype type of tlv object (record)
 * @param[in] obj the object to be destructed
 */
void rsksi_objfree(uint16_t tlvtype, void *obj) {
    unsigned j;
    // check if obj is valid
    if (obj == NULL) return;

    switch (tlvtype) {
        case 0x0901:
            if (((block_hdr_t *)obj)->iv != NULL) free(((block_hdr_t *)obj)->iv);
            if (((block_hdr_t *)obj)->lastHash.data != NULL) free(((block_hdr_t *)obj)->lastHash.data);
            break;
        case 0x0902:
        case 0x0903:
            free(((imprint_t *)obj)->data);
            break;
        case 0x0904: /* signature data for a log block */
        case 0x0905: /* signature data for a log block */
            if (((block_sig_t *)obj)->sig.der.data != NULL) {
                free(((block_sig_t *)obj)->sig.der.data);
            }
            break;
        case 0x0907: /* Free Hash Chain */
            if (((block_hashchain_t *)obj)->rec_hash.data != NULL) {
                free(((block_hashchain_t *)obj)->rec_hash.data);
            }
            /* Loop through Step Objects and delete mem */
            if (((block_hashchain_t *)obj)->stepCount > 0) {
                for (j = 0; j < ((block_hashchain_t *)obj)->stepCount; ++j) {
                    if (((block_hashchain_t *)obj)->hashsteps[j] != NULL &&
                        ((block_hashchain_t *)obj)->hashsteps[j]->sib_hash.data != NULL) {
                        free(((block_hashchain_t *)obj)->hashsteps[j]->sib_hash.data);
                    }
                    free((block_hashstep_t *)((block_hashchain_t *)obj)->hashsteps[j]);
                }
            }
            break;
        default:
            fprintf(stderr, "rsksi_objfree:\t unknown tlv record %4.4x\n", tlvtype);
            break;
    }
    free(obj);
}

static block_hashstep_t *rsksiHashstepFromKSI_DataHash(ksifile ksi, KSI_DataHash *hash) {
    int r;
    const unsigned char *digest;
    size_t digest_len;
    block_hashstep_t *hashstep;

    if ((hashstep = calloc(1, sizeof(block_hashstep_t))) == NULL) {
        goto done;
    }

    /* Get imprint from KSI_Hash */
    KSI_HashAlgorithm hashID;
    r = KSI_DataHash_extract(hash, &hashID, &digest, &digest_len);
    if (r != KSI_OK) {
        reportKSIAPIErr(ksi->ctx, ksi, "KSI_DataHash_extract", r);
        free(hashstep);
        hashstep = NULL;
        goto done;
    }

    /* Fill Hashstep object */
    hashstep->sib_hash.hashID = hashID;
    hashstep->sib_hash.len = digest_len;
    if ((hashstep->sib_hash.data = (uint8_t *)malloc(hashstep->sib_hash.len)) == NULL) {
        free(hashstep);
        hashstep = NULL;
        goto done;
    }
    memcpy(hashstep->sib_hash.data, digest, digest_len);
done:
    return hashstep;
}

/**
 * Read block parameters. This detects if the block contains the
 * individual log hashes, the intermediate hashes and the overall
 * block parameters (from the signature block). As we do not have any
 * begin of block record, we do not know e.g. the hash algorithm or IV
 * until reading the block signature record. And because the file is
 * purely sequential and variable size, we need to read all records up to
 * the next signature record.
 * If a caller intends to verify a log file based on the parameters,
 * he must re-read the file from the begining (we could keep things
 * in memory, but this is impractical for large blocks). In order
 * to facitate this, the function permits to rewind to the original
 * read location when it is done.
 *
 * @param[in] fp file pointer of tlv file
 * @param[in] bRewind 0 - do not rewind at end of procesing, 1 - do so
 * @param[out] bs block signature record
 * @param[out] bHasRecHashes 0 if record hashes are present, 1 otherwise
 * @param[out] bHasIntermedHashes 0 if intermediate hashes are present,
 *                1 otherwise
 *
 * @returns 0 if ok, something else otherwise
 */
int rsksi_getBlockParams(FILE *fp,
                         uint8_t bRewind,
                         block_sig_t **bs,
                         block_hdr_t **bh,
                         uint8_t *bHasRecHashes,
                         uint8_t *bHasIntermedHashes) {
    int r = RSGTE_SUCCESS;
    uint64_t nRecs = 0;
    uint8_t bDone = 0;
    uint8_t bHdr = 0;
    off_t rewindPos = 0;
    void *obj;
    tlvrecord_t rec;

    if (bRewind) rewindPos = ftello(fp);
    *bHasRecHashes = 0;
    *bHasIntermedHashes = 0;
    *bs = NULL;
    *bh = NULL;

    while (!bDone) { /* we will err out on EOF */
        if ((r = rsksi_tlvrd(fp, &rec, &obj)) != 0) goto done;
        bHdr = 0;
        switch (rec.tlvtype) {
            case 0x0901:
                *bh = (block_hdr_t *)obj;
                bHdr = 1;
                break;
            case 0x0902:
                ++nRecs;
                *bHasRecHashes = 1;
                break;
            case 0x0903:
                *bHasIntermedHashes = 1;
                break;
            case 0x0904:
                *bs = (block_sig_t *)obj;
                bDone = 1;
                break;
            default:
                fprintf(fp, "unknown tlv record %4.4x\n", rec.tlvtype);
                break;
        }
        if (!bDone && !bHdr) rsksi_objfree(rec.tlvtype, obj);
    }

    if (*bHasRecHashes && (nRecs != (*bs)->recCount)) {
        r = RSGTE_INVLD_RECCNT;
        goto done;
    }

    if (bRewind) {
        if (fseeko(fp, rewindPos, SEEK_SET) != 0) {
            r = RSGTE_IO;
            goto done;
        }
    }
done:
    if (rsksi_read_debug && r != RSGTE_EOF && r != RSGTE_SUCCESS)
        printf("debug: rsksi_getBlockParams:\t returned %d\n", r);
    return r;
}

/**
 * Read Excerpt block parameters. This detects if the block contains
 * hash chains for log records.
 * If a caller intends to verify a log file based on the parameters,
 * he must re-read the file from the begining (we could keep things
 * in memory, but this is impractical for large blocks). In order
 * to facitate this, the function permits to rewind to the original
 * read location when it is done.
 *
 * @param[in] fp file pointer of tlv file
 * @param[in] bRewind 0 - do not rewind at end of procesing, 1 - do so
 * @param[out] bs block signature record
 *
 * @returns 0 if ok, something else otherwise
 */
int rsksi_getExcerptBlockParams(FILE *fp, uint8_t bRewind, block_sig_t **bs, block_hdr_t **bh) {
    int r = RSGTE_SUCCESS;
    uint64_t nRecs = 0;
    uint8_t bSig = 0;
    off_t rewindPos = 0;
    void *obj;
    tlvrecord_t rec;

    /* Initial RewindPos */
    if (bRewind) rewindPos = ftello(fp);
    *bs = NULL;

    /* Init Blockheader */
    if ((*bh = calloc(1, sizeof(block_hdr_t))) == NULL) {
        r = RSGTE_OOM;
        goto done;
    }
    (*bh)->iv = NULL;
    (*bh)->lastHash.data = NULL;

    while (r == RSGTE_SUCCESS && bSig == 0) { /* we will err out on EOF */
        if ((r = rsksi_tlvrd(fp, &rec, &obj)) != 0) goto done;
        switch (rec.tlvtype) {
            case 0x0905: /* OpenKSI signature | Excerpt File */
                if (*bs == NULL) {
                    *bs = (block_sig_t *)obj;

                    /* Save NEW RewindPos */
                    if (bRewind) rewindPos = ftello(fp);
                } else {
                    /* Previous Block finished */
                    bSig = 1;
                }
                break;
            case 0x0907: /* hash chain for one log record | Excerpt File */
                if (*bs != NULL) {
                    if (nRecs == 0) /* Copy HASHID from record hash */
                        (*bh)->hashID = ((block_hashchain_t *)obj)->rec_hash.hashID;
                    /* Increment hash chain count */
                    nRecs++;
                }

                /* Free MEM, hashchain obj not needed here*/
                rsksi_objfree(rec.tlvtype, obj);
                break;
            default:
                fprintf(fp, "unknown tlv record %4.4x\n", rec.tlvtype);
                break;
        }

        /* Free second Signatur object if set! */
        if (bSig == 1 && obj != NULL) rsksi_objfree(rec.tlvtype, obj);
    }
done:
    if (*bs != NULL) {
        if (r == RSGTE_EOF) {
            if (rsksi_read_debug) printf("debug: rsksi_getExcerptBlockParams:\t Reached END of FILE\n");
            r = RSGTE_SUCCESS;
        }
    } else {
        goto done2;
    }

    /* Copy Count back! */
    (*bs)->recCount = nRecs;

    /* Rewind file back */
    if (bRewind) {
        if (fseeko(fp, rewindPos, SEEK_SET) != 0) {
            r = RSGTE_IO;
            goto done2;
        }
    }
done2:
    if (rsksi_read_debug)
        printf("debug: rsksi_getExcerptBlockParams:\t Found %lld records, returned %d\n", (long long unsigned)nRecs, r);
    return r;
}

/**
 *	Set Default Constrain parameters
 */
int rsksi_setDefaultConstraint(ksifile ksi, char *stroid, char *strvalue) {
    int ksistate;
    int r = RSGTE_SUCCESS;

    /* Create and set default CertConstraint */
    const KSI_CertConstraint pubFileCertConstr[] = {{stroid, strvalue}, {NULL, NULL}};

    if (rsksi_read_debug) {
        printf("rsksi_setDefaultConstraint:\t\t Setting OID='%s' to '%s' \n", stroid, strvalue);
    }

    ksistate = KSI_CTX_setDefaultPubFileCertConstraints(ksi->ctx->ksi_ctx, pubFileCertConstr);
    if (ksistate != KSI_OK) {
        fprintf(stderr,
                "rsksi_setDefaultConstraint:\t\t\t Unable to configure publications file cert "
                "constraints %s=%s.\n",
                stroid, strvalue);
        r = RSGTE_IO;
        goto done;
    }
done:
    return r;
}


/**
 * Read the file header and compare it to the expected value.
 * The file pointer is placed right after the header.
 * @param[in] fp file pointer of tlv file
 * @param[in] excpect expected header (e.g. "LOGSIG10")
 * @returns 0 if ok, something else otherwise
 */
int rsksi_chkFileHdr(FILE *fp, char *expect, uint8_t verbose) {
    int r;
    char hdr[9];
    off_t rewindPos = ftello(fp);

    if ((r = rsksi_tlvrdHeader(fp, (uchar *)hdr)) != 0) goto done;
    if (strcmp(hdr, expect)) {
        r = RSGTE_INVLHDR;
        fseeko(fp, rewindPos, SEEK_SET); /* Reset Filepointer on failure for additional checks*/
    } else
        r = 0;
done:
    if (r != RSGTE_SUCCESS && verbose) printf("rsksi_chkFileHdr:\t\t failed expected '%s' but was '%s'\n", expect, hdr);
    return r;
}

ksifile rsksi_vrfyConstruct_gf(void) {
    int ksistate;
    ksifile ksi;
    if ((ksi = calloc(1, sizeof(struct ksifile_s))) == NULL) goto done;
    ksi->x_prev = NULL;

    /* Create new KSI Context! */
    rsksictx ctx = rsksiCtxNew();
    ksi->ctx = ctx; /* assign context to ksifile */

    /* Setting KSI Publication URL ! */
    ksistate = KSI_CTX_setPublicationUrl(ksi->ctx->ksi_ctx, rsksi_read_puburl);
    if (ksistate != KSI_OK) {
        fprintf(stderr, "Failed setting KSI Publication URL '%s' with error (%d): %s\n", rsksi_read_puburl, ksistate,
                KSI_getErrorString(ksistate));
        free(ksi);
        return NULL;
    }
    if (rsksi_read_debug) fprintf(stdout, "PublicationUrl set to: '%s'\n", rsksi_read_puburl);

    /* Setting KSI Extender! */
    ksistate = KSI_CTX_setExtender(ksi->ctx->ksi_ctx, rsksi_extend_puburl, rsksi_userid, rsksi_userkey);
    if (ksistate != KSI_OK) {
        fprintf(stderr, "Failed setting KSIExtender URL '%s' with error (%d): %s\n", rsksi_extend_puburl, ksistate,
                KSI_getErrorString(ksistate));
        free(ksi);
        return NULL;
    }
    if (rsksi_read_debug) fprintf(stdout, "ExtenderUrl set to: '%s'\n", rsksi_extend_puburl);

done:
    return ksi;
}

void rsksi_vrfyBlkInit(ksifile ksi, block_hdr_t *bh, uint8_t bHasRecHashes, uint8_t bHasIntermedHashes) {
    ksi->hashAlg = hashID2AlgKSI(bh->hashID);
    ksi->bKeepRecordHashes = bHasRecHashes;
    ksi->bKeepTreeHashes = bHasIntermedHashes;
    if (ksi->IV != NULL) {
        free(ksi->IV);
        ksi->IV = NULL;
    }
    if (bh->iv != NULL) {
        ksi->IV = malloc(getIVLenKSI(bh));
        memcpy(ksi->IV, bh->iv, getIVLenKSI(bh));
    }
    if (bh->lastHash.data != NULL) {
        rsksiimprintDel(ksi->x_prev); /* Delete first incase isset */
        ksi->x_prev = malloc(sizeof(imprint_t));
        ksi->x_prev->len = bh->lastHash.len;
        ksi->x_prev->hashID = bh->lastHash.hashID;
        ksi->x_prev->data = malloc(ksi->x_prev->len);
        memcpy(ksi->x_prev->data, bh->lastHash.data, ksi->x_prev->len);
    } else {
        ksi->x_prev = NULL;
    }
}

static int rsksi_vrfy_chkRecHash(ksifile ksi, FILE *sigfp, FILE *nsigfp, KSI_DataHash *hash, ksierrctx_t *ectx) {
    int r = 0;
    imprint_t *imp = NULL;

    const unsigned char *digest;
    if (KSI_DataHash_extract(hash, NULL, &digest, NULL) != KSI_OK) {
        r = RSGTE_EXTRACT_HASH;
        reportError(r, ectx);
        goto done;
    }

    if ((r = rsksi_tlvrdRecHash(sigfp, nsigfp, &imp)) != 0) reportError(r, ectx);
    goto done;
    if (imp->hashID != hashIdentifierKSI(ksi->hashAlg)) {
        reportError(r, ectx);
        r = RSGTE_INVLD_REC_HASHID;
        goto done;
    }
    if (memcmp(imp->data, digest, hashOutputLengthOctetsKSI(imp->hashID))) {
        r = RSGTE_INVLD_REC_HASH;
        ectx->computedHash = hash;
        ectx->fileHash = imp;
        reportError(r, ectx);
        ectx->computedHash = NULL, ectx->fileHash = NULL;
        goto done;
    }
    r = 0;
done:
    if (imp != NULL) rsksi_objfree(0x0902, imp);
    return r;
}

static int rsksi_vrfy_chkTreeHash(ksifile ksi, FILE *sigfp, FILE *nsigfp, KSI_DataHash *hash, ksierrctx_t *ectx) {
    int r = 0;
    imprint_t *imp = NULL;
    const unsigned char *digest;
    if (KSI_DataHash_extract(hash, NULL, &digest, NULL) != KSI_OK) {
        r = RSGTE_EXTRACT_HASH;
        reportError(r, ectx);
        goto done;
    }


    if ((r = rsksi_tlvrdTreeHash(sigfp, nsigfp, &imp)) != 0) {
        reportError(r, ectx);
        goto done;
    }
    if (imp->hashID != hashIdentifierKSI(ksi->hashAlg)) {
        reportError(r, ectx);
        r = RSGTE_INVLD_TREE_HASHID;
        goto done;
    }
    if (memcmp(imp->data, digest, hashOutputLengthOctetsKSI(imp->hashID))) {
        r = RSGTE_INVLD_TREE_HASH;
        ectx->computedHash = hash;
        ectx->fileHash = imp;
        reportError(r, ectx);
        ectx->computedHash = NULL, ectx->fileHash = NULL;
        goto done;
    }
    r = 0;
done:
    if (imp != NULL) {
        if (rsksi_read_debug)
            printf("debug: rsksi_vrfy_chkTreeHash:\t\t returned %d, hashID=%d, Length=%d\n", r, imp->hashID,
                   hashOutputLengthOctetsKSI(imp->hashID));
        /* Free memory */
        rsksi_objfree(0x0903, imp);
    }
    return r;
}

/* Helper function to verifiy the next record in the signature file */
int rsksi_vrfy_nextRec(ksifile ksi, FILE *sigfp, FILE *nsigfp, unsigned char *rec, size_t len, ksierrctx_t *ectx) {
    int r = 0;
    KSI_DataHash *x; /* current hash */
    KSI_DataHash *m, *recHash = NULL, *t, *t_del;
    uint8_t j;

    hash_m_ksi(ksi, &m);
    hash_r_ksi(ksi, &recHash, rec, len);

    if (ksi->bKeepRecordHashes) {
        r = rsksi_vrfy_chkRecHash(ksi, sigfp, nsigfp, recHash, ectx);
        if (r != 0) goto done;
    }
    hash_node_ksi(ksi, &x, m, recHash, 1); /* hash leaf */
    if (ksi->bKeepTreeHashes) {
        ectx->treeLevel = 0;
        ectx->lefthash = m;
        ectx->righthash = recHash;
        r = rsksi_vrfy_chkTreeHash(ksi, sigfp, nsigfp, x, ectx);
        if (r != 0) goto done;
    }

    /* Store Current Hash for later use */
    rsksiimprintDel(ksi->x_prev);
    ksi->x_prev = rsksiImprintFromKSI_DataHash(ksi, x);

    /* add x to the forest as new leaf, update roots list */
    t = x;
    for (j = 0; j < ksi->nRoots; ++j) {
        if (ksi->roots_valid[j] == 0) {
            ksi->roots_hash[j] = t;
            ksi->roots_valid[j] = 1;
            t = NULL;
            break;
        } else if (t != NULL) {
            /* hash interim node */
            ectx->treeLevel = j + 1;
            ectx->righthash = t;
            t_del = t;
            hash_node_ksi(ksi, &t, ksi->roots_hash[j], t_del, j + 2);
            ksi->roots_valid[j] = 0;
            if (ksi->bKeepTreeHashes) {
                ectx->lefthash = ksi->roots_hash[j];
                r = rsksi_vrfy_chkTreeHash(ksi, sigfp, nsigfp, t, ectx);
                if (r != 0) goto done; /* mem leak ok, we terminate! */
            }
            KSI_DataHash_free(ksi->roots_hash[j]);
            KSI_DataHash_free(t_del);
        }
    }
    if (t != NULL) {
        /* new level, append "at the top" */
        ksi->roots_hash[ksi->nRoots] = t;
        ksi->roots_valid[ksi->nRoots] = 1;
        ++ksi->nRoots;
        assert(ksi->nRoots < MAX_ROOTS);
        t = NULL;
    }
    ++ksi->nRecords;

    /* cleanup */
    KSI_DataHash_free(m);
done:
    if (recHash != NULL) KSI_DataHash_free(recHash);
    if (rsksi_read_debug) printf("debug: rsksi_vrfy_nextRec:\t\t returned %d\n", r);
    return r;
}

/* Helper function to verifiy the next record in the signature file */
int rsksi_vrfy_nextRecExtract(ksifile ksi,
                              FILE *sigfp,
                              FILE *nsigfp,
                              unsigned char *rec,
                              size_t len,
                              ksierrctx_t *ectx,
                              block_hashchain_t *hashchain,
                              int storehashchain) {
    int r = 0;
    KSI_DataHash *x; /* current hash */
    KSI_DataHash *m, *recHash = NULL, *t, *t_del;
    uint8_t j;
    block_hashstep_t *hashstep = NULL;

    hash_m_ksi(ksi, &m);
    hash_r_ksi(ksi, &recHash, rec, len);

    if (ksi->bKeepRecordHashes) {
        r = rsksi_vrfy_chkRecHash(ksi, sigfp, nsigfp, recHash, ectx);
        if (r != 0) goto done;
    }
    hash_node_ksi(ksi, &x, m, recHash, 1); /* hash leaf */
    if (ksi->bKeepTreeHashes) {
        ectx->treeLevel = 0;
        ectx->lefthash = m;
        ectx->righthash = recHash;
        r = rsksi_vrfy_chkTreeHash(ksi, sigfp, nsigfp, x, ectx);
        if (r != 0) goto done;
    }
    /* EXTRA DEBUG !!!! */
    if (rsksi_read_debug) {
        outputKSIHash(stdout, "debug: rsksi_vrfy_nextRecExtract:\t Tree Left Hash.....: ", m, ectx->verbose);
        outputKSIHash(stdout, "debug: rsksi_vrfy_nextRecExtract:\t Tree Right Hash....: ", recHash, ectx->verbose);
        outputKSIHash(stdout, "debug: rsksi_vrfy_nextRecExtract:\t Tree Current Hash..: ", x, ectx->verbose);
    }
    /* Store Current Hash for later use */
    rsksiimprintDel(ksi->x_prev);
    ksi->x_prev = rsksiImprintFromKSI_DataHash(ksi, x);

    if (storehashchain == 1) {
        /* Store record hash for HashChain */
        if (hashchain->rec_hash.data == NULL) {
            /* Extract and copy record imprint*/
            rsksiIntoImprintFromKSI_DataHash(&(hashchain->rec_hash), ksi, x);
            if (rsksi_read_debug)
                outputHash(stdout,
                           "debug: rsksi_vrfy_nextRecExtract:\t RECORD Hash:"
                           " \t\t",
                           hashchain->rec_hash.data, hashchain->rec_hash.len, ectx->verbose);
            hashchain->direction = 0x03; /* RIGHT */
            hashstep = rsksiHashstepFromKSI_DataHash(ksi, m);
            if (hashstep == NULL) {
                r = RSGTE_IO;
                goto done;
            }
            hashstep->direction = 0x03; /* RIGHT */
            hashstep->level_corr = 0; /* Level Correction 0 */
            if (rsksi_read_debug)
                outputHash(stdout,
                           "debug: rsksi_vrfy_nextRecExtract:\t RIGHT Hash:"
                           " \t\t",
                           hashstep->sib_hash.data, hashstep->sib_hash.len, ectx->verbose);

            /* Attach to HashChain */
            hashchain->hashsteps[hashchain->stepCount] = hashstep;
            hashchain->stepCount++;

            hashchain->direction = 0x03; /* RIGHT */
            hashchain->level = 1;
        }
    }

    /* add x to the forest as new leaf, update roots list */
    t = x;

    for (j = 0; j < ksi->nRoots; ++j) {
        if (ksi->roots_valid[j] == 0) {
            /* NOT SURE ABOUT j+1 ! */
            if ((j + 1) == hashchain->level) {
                hashchain->direction = 0x02; /* LEFT */
            }
            if (rsksi_read_debug) {
                printf(
                    "debug: rsksi_vrfy_nextRecExtract:\t ROOT is NULL, "
                    "%s Direction, Level=%d\n",
                    (hashchain->direction == 0x02) ? "LEFT" : "RIGHT", j);
            }
            ksi->roots_hash[j] = t;
            ksi->roots_valid[j] = 1;
            t = NULL;
            break;
        } else if (t != NULL) {
            if ((j + 1) == hashchain->level) {
                /* NOT SURE ABOUT j+1 ! */
                if (hashchain->direction == 0x03) { /*RIGHT*/
                    hashstep = rsksiHashstepFromKSI_DataHash(ksi, ksi->roots_hash[j]);
                    if (hashstep == NULL) {
                        r = RSGTE_IO;
                        goto done;
                    }
                } else { /*LEFT*/
                    hashstep = rsksiHashstepFromKSI_DataHash(ksi, t);
                    if (hashstep == NULL) {
                        r = RSGTE_IO;
                        goto done;
                    }
                }
                if (rsksi_read_debug) {
                    printf(
                        "debug: rsksi_vrfy_nextRecExtract:\t %s "
                        "DIRECTION, Level %d\n",
                        (hashchain->direction == 0x02) ? "LEFT" : "RIGHT", j);
                    outputHash(stdout,
                               "debug: rsksi_vrfy_nextRecExtract: \t "
                               "RIGHT Hash: \t\t",
                               hashstep->sib_hash.data, hashstep->sib_hash.len, ectx->verbose);
                }
                hashstep->direction = hashchain->direction;
                hashstep->level_corr = 0;
                /* Attach to HashChain */
                hashchain->hashsteps[hashchain->stepCount] = hashstep;
                hashchain->stepCount++;
                /* Set Direction and Chainlevel */
                hashchain->direction = 0x03; /*RIGHT*/
                hashchain->level = j + 1 + 1;
                if (rsksi_read_debug) printf("debug: rsksi_vrfy_nextRecExtract:\t NEXT Level=%d\n", hashchain->level);
            }

            /* hash interim node */
            ectx->treeLevel = j + 1;
            ectx->righthash = t;
            t_del = t;
            hash_node_ksi(ksi, &t, ksi->roots_hash[j], t_del, j + 1 + 1);
            ksi->roots_valid[j] = 0;
            if (ksi->bKeepTreeHashes) {
                ectx->lefthash = ksi->roots_hash[j];
                r = rsksi_vrfy_chkTreeHash(ksi, sigfp, nsigfp, t, ectx);
                if (r != 0) goto done; /* mem leak ok, we terminate! */
            }
            KSI_DataHash_free(ksi->roots_hash[j]);
            KSI_DataHash_free(t_del);
        }
    }

    if (t != NULL) {
        if (ksi->nRoots < hashchain->level) hashchain->direction = 0x02; /*LEFT*/

        if (rsksi_read_debug) {
            printf(
                "debug: rsksi_vrfy_nextRecExtract:\t END Check, %s Direction, Level=%d, "
                "Attachlevel=%d\n",
                (hashchain->direction == 0x02) ? "LEFT" : "RIGHT", j, ksi->nRoots);
            outputKSIHash(stdout, "debug: rsksi_vrfy_nextRecExtract:\t NEW ROOT Hash....: ", t, ectx->verbose);
        }

        /* new level, append "at the top" */
        ksi->roots_hash[ksi->nRoots] = t;
        ksi->roots_valid[ksi->nRoots] = 1;
        ++ksi->nRoots;
        assert(ksi->nRoots < MAX_ROOTS);
        t = NULL;
    }
    ++ksi->nRecords;

    /* cleanup */
    KSI_DataHash_free(m);
done:

#if 0
	printf("\nMerkle TREE:\n");
	for(j = 0 ; j < ksi->nRoots ; ++j) {
		printf("%.2d: ", j);fflush(stdout);
		if(ksi->roots_valid[j] == 0) {
			printf("invalid\n");
		} else {
			rsksi_printIMPRINT(stdout, "valid: ", rsksiImprintFromKSI_DataHash(NULL,
			ksi->roots_hash[j]), 0);
		}
	}
	printf("HASH Chain:\n");
	for(j = 0 ; j < hashchain->stepCount ; ++j) {
		hashstep = hashchain->hashsteps[j];
		outputHash(stdout, "hash: ", hashstep->sib_hash.data, hashstep->sib_hash.len, ectx->verbose);
	}
#endif


    if (recHash != NULL) KSI_DataHash_free(recHash);
    if (rsksi_read_debug) printf("debug: rsksi_vrfy_nextRecExtract:\t returned %d\n", r);
    return r;
}

/* Helper function to verifiy the next hash chain record in the signature file */
int rsksi_vrfy_nextHashChain(
    ksifile ksi, block_sig_t *bs, FILE *sigfp, unsigned char *rec, size_t len, ksierrctx_t *ectx) {
    unsigned int j;
    int r = 0;
    int ksistate;
    KSI_Signature *sig = NULL;
    KSI_DataHash *line_hash = NULL, *root_hash = NULL, *root_tmp = NULL;
    KSI_DataHash *rec_hash = NULL, *sibling_hash = NULL; /* left_hash = NULL, *right_hash = NULL; */
    int bCheckLineHash = 0; /* Line Hash will be checked after first record !*/
    void *obj;
    tlvrecord_t tlvrec;
    block_hashchain_t *hashchain = NULL;
    uint8_t uiLevelCorr = 0;

    /* Check for next valid tlvrecord */
    if ((r = rsksi_tlvrd(sigfp, &tlvrec, &obj)) != 0) goto done;
    if (tlvrec.tlvtype != 0x0907) {
        r = RSGTE_INVLTYP;
        goto done;
    }

    /* Convert Pointer to block_hashchain_t*/
    hashchain = (block_hashchain_t *)obj;

    /* Verify Hash Alg */
    if (hashchain->rec_hash.hashID != hashIdentifierKSI(ksi->hashAlg)) {
        reportError(r, ectx);
        r = RSGTE_INVLD_REC_HASHID;
        goto done;
    }

    /* Create Hash from Line */
    hash_r_ksi(ksi, &line_hash, rec, len);
    if (rsksi_read_debug)
        outputKSIHash(stdout, "debug: rsksi_vrfy_nextHashChain:\t Line Hash.:.............: ", line_hash,
                      ectx->verbose);

    /* Convert Record hash from HashChain into KSI_DataHash */
    KSI_DataHash_fromDigest(ksi->ctx->ksi_ctx, hashchain->rec_hash.hashID, hashchain->rec_hash.data,
                            hashchain->rec_hash.len, &rec_hash);
    if (rsksi_read_debug)
        outputKSIHash(stdout, "debug: rsksi_vrfy_nextHashChain:\t HashChain Record Hash...: ", rec_hash, ectx->verbose);

    /* Clone Line Hash for first time*/
    if (KSI_DataHash_clone(line_hash, &root_tmp) != KSI_OK) {
        r = RSGTE_IO;
        goto done;
    }

    /* Loop through hashchain now */
    for (j = 0; j < hashchain->stepCount; ++j) {
        /* Set Level correction */
        uiLevelCorr += hashchain->hashsteps[j]->level_corr + 1;

        /* Convert Sibling hash from HashChain into KSI_DataHash */
        KSI_DataHash_fromDigest(ksi->ctx->ksi_ctx, hashchain->hashsteps[j]->sib_hash.hashID,
                                hashchain->hashsteps[j]->sib_hash.data, hashchain->hashsteps[j]->sib_hash.len,
                                &sibling_hash);
        if (rsksi_read_debug)
            outputKSIHash(stdout, "debug: rsksi_vrfy_nextHashChain:\t HashChain Sibling Hash..: ", sibling_hash,
                          ectx->verbose);

        if (hashchain->hashsteps[j]->direction == 0x02 /* LEFT */) {
            /* Combine Root Hash with LEFT Sibling */
            hash_node_ksi(ksi, &root_hash, root_tmp, sibling_hash, uiLevelCorr);
        } else /* RIGHT */ {
            /* Combine Root Hash with RIGHT Sibling */
            hash_node_ksi(ksi, &root_hash, sibling_hash, root_tmp, uiLevelCorr);
        }
        KSI_DataHash_free(root_tmp); /* Free tmp hash*/
        if (rsksi_read_debug) {
            printf("debug: rsksi_vrfy_nextHashChain:\t Direction=%s, Step=%d, LevelCorr=%d\n",
                   (hashchain->hashsteps[j]->direction == 0x02) ? "LEFT" : "RIGHT", j, uiLevelCorr);
            outputKSIHash(stdout, "debug: rsksi_vrfy_nextHashChain:\t HashChain New Root Hash.: ", root_hash,
                          ectx->verbose);
        }

        /* First Sibling, check */
        if (bCheckLineHash == 0) {
            /* Compare root_hash vs rec_hash */
            if (KSI_DataHash_equals(root_hash, rec_hash) != 1) {
                r = RSGTE_INVLD_REC_HASH;
                ectx->computedHash = root_hash;
                ectx->fileHash = &(hashchain->rec_hash);
                reportError(r, ectx);
                ectx->computedHash = NULL, ectx->fileHash = NULL;
                goto done;
            } else {
                if (rsksi_read_showVerified)
                    printf(
                        "Successfully compared line hash against "
                        "record hash\n");
            }
            bCheckLineHash = 1;
        }

        /* Store into TMP for next LOOP */
        root_tmp = root_hash;

        /* Free memory */
        if (sibling_hash != NULL) KSI_DataHash_free(sibling_hash);
    }

    /* Parse KSI Signature */
    ksistate = KSI_Signature_parse(ksi->ctx->ksi_ctx, bs->sig.der.data, bs->sig.der.len, &sig);
    if (ksistate != KSI_OK) {
        if (rsksi_read_debug)
            printf("debug: rsksi_vrfy_nextHashChain:\t KSI_Signature_parse failed with error: %s (%d)\n",
                   KSI_getErrorString(ksistate), ksistate);
        r = RSGTE_INVLD_SIGNATURE;
        ectx->ksistate = ksistate;
        goto done;
    } else {
        if (rsksi_read_debug)
            printf(
                "debug: rsksi_vrfy_nextHashChain:\t KSI_Signature_parse "
                "was successfull\n");
    }

    /* Verify KSI Signature */
    ksistate = KSI_Signature_verify(sig, ksi->ctx->ksi_ctx);
    if (ksistate != KSI_OK) {
        if (rsksi_read_debug)
            printf("debug: rsksi_vrfy_nextHashChain:\t KSI_Signature_verify failed with error: %s (%d)\n",
                   KSI_getErrorString(ksistate), ksistate);
        r = RSGTE_INVLD_SIGNATURE;
        ectx->ksistate = ksistate;
        goto done;
    } else {
        if (rsksi_read_debug)
            printf(
                "debug: rsksi_vrfy_nextHashChain:\t KSI_Signature_verify "
                "was successfull\n");
    }

    /* Verify Roothash against Signature */
    ksistate = KSI_Signature_verifyDataHash(sig, ksi->ctx->ksi_ctx, root_hash);
    if (ksistate != KSI_OK) {
        if (rsksi_read_debug)
            printf(
                "debug: rsksi_vrfy_nextHashChain:\t KSI_Signature_verifyDataHash failed with error: "
                "%s (%d)\n",
                KSI_getErrorString(ksistate), ksistate);
        r = RSGTE_INVLD_SIGNATURE;
        ectx->ksistate = ksistate;
        goto done;
    } else {
        if (rsksi_read_debug)
            printf(
                "debug: rsksi_vrfy_nextHashChain:\t KSI_Signature_verifyDataHash "
                "was successfull\n");
        if (rsksi_read_showVerified) reportVerifySuccess(ectx);
    }
done:
    /* Free Memory */
    if (sig != NULL) KSI_Signature_free(sig);
    if (root_hash != NULL) KSI_DataHash_free(root_hash);
    if (line_hash != NULL) KSI_DataHash_free(line_hash);
    if (rec_hash != NULL) KSI_DataHash_free(rec_hash);
    if (hashchain != NULL) rsksi_objfree(0x0907, hashchain);

    if (rsksi_read_debug) printf("debug: rsksi_vrfy_nextHashChain:\t returned %d\n", r);
    return r;
}

/* Finish Verify Sigblock with additional HashChain handling */
int verifySigblkFinishChain(ksifile ksi, block_hashchain_t *hashchain, KSI_DataHash **pRoot, ksierrctx_t *ectx) {
    KSI_DataHash *root, *rootDel;
    int8_t j;
    int r = 0;
    root = NULL;
    block_hashstep_t *hashstep = NULL;

    if (ksi->nRecords == 0) {
        if (rsksi_read_debug) printf("debug: verifySigblkFinishChain:\t\t Error, No records found! %d\n", r);
        goto done;
    }

    for (j = 0; j < ksi->nRoots; ++j) {
        if (root == NULL) {
            if (j + 1 == hashchain->level) {
                hashchain->direction = 0x03; /*RIGHT*/
            }

            root = ksi->roots_valid[j] ? ksi->roots_hash[j] : NULL;
            if (rsksi_read_debug)
                printf("debug: verifySigblkFinishChain:\t\t ROOT VALID=%d, %s Direction, Level=%d Roots=%d\n",
                       ksi->roots_valid[j], (hashchain->direction == 0x02) ? "LEFT" : "RIGHT", j, ksi->nRoots);
            ksi->roots_valid[j] = 0;
            /* Sets Root-J to NONE ....guess this is redundant with init, maybe del */
        } else if (ksi->roots_valid[j]) {
            if (rsksi_read_debug) printf("debug: verifySigblkFinishChain:\t\t Level=%d\n", j);
            if (j + 1 >= hashchain->level) {
                if (hashchain->direction == 0x03) { /*RIGHT*/
                    hashstep = rsksiHashstepFromKSI_DataHash(ksi, ksi->roots_hash[j]);
                    if (hashstep == NULL) {
                        r = RSGTE_IO;
                        goto done;
                    }
                    if (rsksi_read_debug)
                        outputHash(stdout,
                                   "debug: verifySigblkFinishChain:\t\t RIGHT "
                                   "Hash: \t\t",
                                   hashstep->sib_hash.data, hashstep->sib_hash.len, ectx->verbose);
                } else { /*LEFT*/
                    hashstep = rsksiHashstepFromKSI_DataHash(ksi, root);
                    if (hashstep == NULL) {
                        r = RSGTE_IO;
                        goto done;
                    }
                    if (rsksi_read_debug)
                        outputHash(stdout,
                                   "debug: verifySigblkFinishChain:\t\t LEFT "
                                   "Hash: \t\t",
                                   hashstep->sib_hash.data, hashstep->sib_hash.len, ectx->verbose);
                }
                hashstep->direction = hashchain->direction;
                hashstep->level_corr = j + 1 - hashchain->level;
                if (rsksi_read_debug)
                    printf("debug: verifySigblkFinishChain:\t\t level_corr=%d\n", hashstep->level_corr);

                /* Attach to HashChain */
                hashchain->hashsteps[hashchain->stepCount] = hashstep;
                hashchain->stepCount++;

                /* Set Direction and Chainlevel */
                hashchain->direction = 0x03; /*RIGHT*/
                hashchain->level = j + 1 + 1;
            }
            rootDel = root;
            hash_node_ksi(ksi, &root, ksi->roots_hash[j], root, j + 2);
            KSI_DataHash_free(rootDel);
            ksi->roots_valid[j - 1] = 0; /* Previous ROOT has been deleted! */
        }
    }

#if 0
printf("FINISH HASH CHAIN:\n");
for(j = 0 ; j < ksi->nRoots ; ++j) {
	printf("%.2d: ", j);fflush(stdout);
	if(ksi->roots_valid[j] == 0) {
		printf("invalid\n");
	} else {
		rsksi_printIMPRINT(stdout, "valid: ", rsksiImprintFromKSI_DataHash(NULL, ksi->roots_hash[j]), 0);
	}
}
#endif

    *pRoot = root;
    r = 0;
done:
    ksi->bInBlk = 0;
    if (rsksi_read_debug && root != NULL)
        outputKSIHash(stdout,
                      "debug: verifySigblkFinishChain:\t\t ROOT "
                      "Hash: \t\t",
                      root, 0);
    return r;
}


/* TODO: think about merging this with the writer. The
 * same applies to the other computation algos.
 */
int verifySigblkFinish(ksifile ksi, KSI_DataHash **pRoot) {
    KSI_DataHash *root, *rootDel;
    int8_t j;
    int r = 0;
    root = NULL;

    if (ksi->nRecords == 0) {
        if (rsksi_read_debug) printf("debug: verifySigblkFinish:\t\t no records!!!%d\n", r);
        goto done;
    }

    for (j = 0; j < ksi->nRoots; ++j) {
        if (root == NULL) {
            root = ksi->roots_valid[j] ? ksi->roots_hash[j] : NULL;
            ksi->roots_valid[j] = 0; /* guess this is redundant with init, maybe del */
        } else if (ksi->roots_valid[j]) {
            rootDel = root;
            hash_node_ksi(ksi, &root, ksi->roots_hash[j], root, j + 2);
            /* DO NOT SET INVALID HERE !			ksi->roots_valid[j] = 0;
                guess this is redundant with init, maybe del */
            KSI_DataHash_free(rootDel);
        }
    }

    *pRoot = root;
    r = 0;
done:
    ksi->bInBlk = 0;
    if (rsksi_read_debug && root != NULL)
        outputKSIHash(stdout, "debug: verifySigblkFinish:\t\t Root hash: \t", root, 1);
    return r;
}

/* helper for rsksi_extendSig: */
#define COPY_SUBREC_TO_NEWREC                              \
    memcpy(newrec.data + iWr, subrec.hdr, subrec.lenHdr);  \
    iWr += subrec.lenHdr;                                  \
    memcpy(newrec.data + iWr, subrec.data, subrec.tlvlen); \
    iWr += subrec.tlvlen;

static int rsksi_extendSig(KSI_Signature *sig, ksifile ksi, tlvrecord_t *rec, ksierrctx_t *ectx) {
    KSI_Signature *extended = NULL;
    uint8_t *der = NULL;
    size_t lenDer = 0;
    int r, rgt;
    tlvrecord_t newrec, subrec;
    uint16_t iRd, iWr;

    if (sig == NULL) goto skip_extention;

    /* Extend Signature now using KSI API*/
    rgt = KSI_extendSignature(ksi->ctx->ksi_ctx, sig, &extended);
    if (rgt != KSI_OK) {
        ectx->ksistate = rgt;
        r = RSGTE_SIG_EXTEND;
        goto done;
    }

    /* Serialize Signature. */
    rgt = KSI_Signature_serialize(extended, &der, &lenDer);
    if (rgt != KSI_OK) {
        ectx->ksistate = rgt;
        r = RSGTE_SIG_EXTEND;
        goto done;
    }

skip_extention:

    /* update block_sig tlv record with new extended timestamp */
    /* we now need to copy all tlv records before the actual der
     * encoded part.
     */
    iRd = iWr = 0;
    CHKr(rsksi_tlvDecodeSUBREC(rec, &iRd, &subrec));
    /* REC_NUM */
    if (subrec.tlvtype != 0x01) {
        r = RSGTE_INVLTYP;
        goto done;
    }
    COPY_SUBREC_TO_NEWREC

    /* actual sig! */
    newrec.data[iWr++] = 0x09 | RSKSI_FLAG_TLV16_RUNTIME;
    newrec.data[iWr++] = 0x05;
    newrec.data[iWr++] = (lenDer >> 8) & 0xff;
    newrec.data[iWr++] = lenDer & 0xff;
    /* now we know how large the new main record is */
    newrec.tlvlen = (uint16_t)iWr + lenDer;
    newrec.tlvtype = rec->tlvtype;
    newrec.hdr[0] = rec->hdr[0];
    newrec.hdr[1] = rec->hdr[1];
    newrec.hdr[2] = (newrec.tlvlen >> 8) & 0xff;
    newrec.hdr[3] = newrec.tlvlen & 0xff;
    newrec.lenHdr = 4;
    if (der != NULL && lenDer != 0) memcpy(newrec.data + iWr, der, lenDer);
    /* and finally copy back new record to existing one */
    memcpy(rec, &newrec, sizeof(newrec) - sizeof(newrec.data) + newrec.tlvlen + 4);
    r = 0;
done:
    if (extended != NULL) KSI_Signature_free(extended);
    if (der != NULL) KSI_free(der);
    return r;
}

/* Verify the existence of the header.
 */
int verifyBLOCK_HDRKSI(FILE *sigfp, FILE *nsigfp, tlvrecord_t *tlvrec) {
    int r;
    block_hdr_t *bh = NULL;
    if ((r = rsksi_tlvrd(sigfp, tlvrec, &bh)) != 0) goto done;
    if (tlvrec->tlvtype != 0x0901) {
        if (rsksi_read_debug)
            printf("debug: verifyBLOCK_HDRKSI:\t\t expected tlvtype 0x0901, but was %4.4x\n", tlvrec->tlvtype);
        r = RSGTE_MISS_BLOCKSIG;
        goto done;
    }
    if (nsigfp != NULL)
        if ((r = rsksi_tlvwrite(nsigfp, tlvrec)) != 0) goto done;
done:
    if (bh != NULL) rsksi_objfree(tlvrec->tlvtype, bh);
    if (rsksi_read_debug) printf("debug: verifyBLOCK_HDRKSI:\t\t returned %d\n", r);
    return r;
}

/* verify the root hash. This also means we need to compute the
 * Merkle tree root for the current block.
 */
int verifyBLOCK_SIGKSI(block_sig_t *bs,
                       ksifile ksi,
                       FILE *sigfp,
                       FILE *nsigfp,
                       uint8_t bExtend,
                       KSI_DataHash *ksiHash,
                       ksierrctx_t *ectx) {
    int r;
    int ksistate;
    block_sig_t *file_bs = NULL;
    KSI_Signature *sig = NULL;
    tlvrecord_t rec;
    if (ksiHash == NULL) {
        if ((r = verifySigblkFinish(ksi, &ksiHash)) != 0) goto done;
    }
    if (rsksi_read_debug)
        outputKSIHash(stdout, "debug: verifyBLOCK_SIGKSI:\t\t SigBlock Finish Hash....: ", ksiHash, ectx->verbose);
    if ((r = rsksi_tlvrdVrfyBlockSig(sigfp, &file_bs, &rec)) != 0) goto done;
    if (ectx->recNum != bs->recCount) {
        r = RSGTE_INVLD_RECCNT;
        goto done;
    }

    /* Process the KSI signature if it is present */
    if (file_bs->sig.der.data == NULL || file_bs->sig.der.len == 0) {
        if (rsksi_read_debug) printf("debug: verifyBLOCK_SIGKSI:\t\t KSI signature missing\n");
        if (bExtend)
            goto skip_ksi;
        else {
            r = RSGTE_MISS_KSISIG;
            goto done;
        }
    }

    /* Parse KSI Signature */
    ksistate = KSI_Signature_parse(ksi->ctx->ksi_ctx, file_bs->sig.der.data, file_bs->sig.der.len, &sig);
    if (ksistate != KSI_OK) {
        if (rsksi_read_debug)
            printf("debug: verifyBLOCK_SIGKSI:\t\t KSI_Signature_parse failed with error: %s (%d)\n",
                   KSI_getErrorString(ksistate), ksistate);
        r = RSGTE_INVLD_SIGNATURE;
        ectx->ksistate = ksistate;
        goto done;
    } else {
        if (rsksi_read_debug) printf("debug: verifyBLOCK_SIGKSI:\t\t KSI_Signature_parse was successfull\n");
    }
    /* Verify KSI Signature */
    ksistate = KSI_Signature_verify(sig, ksi->ctx->ksi_ctx);
    if (ksistate != KSI_OK) {
        if (rsksi_read_debug)
            printf("debug: verifyBLOCK_SIGKSI:\t\t KSI_Signature_verify failed with error: %s (%d)\n",
                   KSI_getErrorString(ksistate), ksistate);
        r = RSGTE_INVLD_SIGNATURE;
        ectx->ksistate = ksistate;
        goto done;
    } else {
        if (rsksi_read_debug) printf("debug: verifyBLOCK_SIGKSI:\t\t KSI_Signature_verify was successfull\n");
    }
    ksistate = KSI_Signature_verifyDataHash(sig, ksi->ctx->ksi_ctx, ksiHash);
    if (ksistate != KSI_OK) {
        if (rsksi_read_debug)
            printf(
                "debug: verifyBLOCK_SIGKSI:\t\t KSI_Signature_verifyDataHash failed with "
                "error: %s (%d)\n",
                KSI_getErrorString(ksistate), ksistate);
        r = RSGTE_INVLD_SIGNATURE;
        ectx->ksistate = ksistate;
        goto done;
    } else {
        if (rsksi_read_debug) printf("debug: verifyBLOCK_SIGKSI:\t\t KSI_Signature_verifyDataHash was successfull\n");
    }

    if (rsksi_read_debug) printf("debug: verifyBLOCK_SIGKSI:\t\t processed without error's\n");
    if (rsksi_read_showVerified) reportVerifySuccess(ectx);

skip_ksi:
    if (bExtend)
        if ((r = rsksi_extendSig(sig, ksi, &rec, ectx)) != 0) goto done;

    if (nsigfp != NULL) {
        if ((r = rsksi_tlvwrite(nsigfp, &rec)) != 0) goto done;
    }
    r = 0;
done:
    if (file_bs != NULL) rsksi_objfree(0x0904, file_bs);
    if (r != 0) reportError(r, ectx);
    if (ksiHash != NULL) KSI_DataHash_free(ksiHash);
    if (sig != NULL) KSI_Signature_free(sig);

    /* Free Top Root Hash as well! */
    uint8_t j;
    for (j = 0; j < ksi->nRoots; ++j) {
        if (ksi->roots_valid[j] == 1) {
            KSI_DataHash_free(ksi->roots_hash[j]);
            ksi->roots_valid[j] = 0;
            if (rsksi_read_debug) printf("debug: verifyBLOCK_SIGKSI:\t\t\t Free ROOTHASH Level %d \n", j);
        }
    }

    return r;
}

/* Helper function to enable debug */
void rsksi_set_debug(int iDebug) {
    rsksi_read_debug = iDebug;
}

/* Helper function to convert an old V10 signature file into V11 */
int rsksi_ConvertSigFile(FILE *oldsigfp, FILE *newsigfp, int verbose) {
    int r = 0, rRead = 0;
    imprint_t *imp = NULL;
    tlvrecord_t rec;
    tlvrecord_t subrec;

    /* For signature convert*/
    int i;
    uint16_t strtidx = 0;
    block_hdr_t *bh = NULL;
    block_sig_t *bs = NULL;
    uint16_t typconv;
    unsigned tlvlen;
    uint8_t tlvlenRecords;

    /* Temporary change flags back to old default */
    RSKSI_FLAG_TLV16_RUNTIME = 0x20;

    /* Start reading Sigblocks from old FILE */
    while (1) { /* we will err out on EOF */
        rRead = rsksi_tlvRecRead(oldsigfp, &rec);
        if (rRead == 0 /*|| rRead == RSGTE_EOF*/) {
            switch (rec.tlvtype) {
                case 0x0900:
                case 0x0901:
                    /* Convert tlvrecord Header */
                    if (rec.tlvtype == 0x0900) {
                        typconv = ((0x00 /*flags*/ | 0x80 /* NEW RSKSI_FLAG_TLV16_RUNTIME
							*/) << 8) | 0x0902;
                        rec.hdr[0] = typconv >> 8;
                        rec.hdr[1] = typconv & 0xff;
                    } else if (rec.tlvtype == 0x0901) {
                        typconv = ((0x00 /*flags*/ | 0x80 /* NEW RSKSI_FLAG_TLV16_RUNTIME
							*/) << 8) | 0x0903;
                        rec.hdr[0] = typconv >> 8;
                        rec.hdr[1] = typconv & 0xff;
                    }

                    /* Debug verification output */
                    r = rsksi_tlvDecodeIMPRINT(&rec, &imp);
                    if (r != 0) goto donedecode;
                    rsksi_printREC_HASH(stdout, imp, verbose);

                    /* Output into new FILE */
                    if ((r = rsksi_tlvwrite(newsigfp, &rec)) != 0) goto done;

                    /* Free mem*/
                    free(imp->data);
                    free(imp);
                    imp = NULL;
                    break;
                case 0x0902:
                    /* Split Data into HEADER and BLOCK */
                    strtidx = 0;

                    /* Create BH and BS*/
                    if ((bh = calloc(1, sizeof(block_hdr_t))) == NULL) {
                        r = RSGTE_OOM;
                        goto donedecode;
                    }
                    if ((bs = calloc(1, sizeof(block_sig_t))) == NULL) {
                        r = RSGTE_OOM;
                        goto donedecode;
                    }

                    /* Check OLD encoded HASH ALGO */
                    CHKrDecode(rsksi_tlvDecodeSUBREC(&rec, &strtidx, &subrec));
                    if (!(subrec.tlvtype == 0x00 && subrec.tlvlen == 1)) {
                        r = RSGTE_FMT;
                        goto donedecode;
                    }
                    bh->hashID = subrec.data[0];

                    /* Check OLD encoded BLOCK_IV */
                    CHKrDecode(rsksi_tlvDecodeSUBREC(&rec, &strtidx, &subrec));
                    if (!(subrec.tlvtype == 0x01)) {
                        r = RSGTE_INVLTYP;
                        goto donedecode;
                    }
                    if ((bh->iv = (uint8_t *)malloc(subrec.tlvlen)) == NULL) {
                        r = RSGTE_OOM;
                        goto donedecode;
                    }
                    memcpy(bh->iv, subrec.data, subrec.tlvlen);

                    /* Check OLD encoded LAST HASH */
                    CHKrDecode(rsksi_tlvDecodeSUBREC(&rec, &strtidx, &subrec));
                    if (!(subrec.tlvtype == 0x02)) {
                        r = RSGTE_INVLTYP;
                        goto donedecode;
                    }
                    bh->lastHash.hashID = subrec.data[0];
                    if (subrec.tlvlen != 1 + hashOutputLengthOctetsKSI(bh->lastHash.hashID)) {
                        r = RSGTE_LEN;
                        goto donedecode;
                    }
                    bh->lastHash.len = subrec.tlvlen - 1;
                    if ((bh->lastHash.data = (uint8_t *)malloc(bh->lastHash.len)) == NULL) {
                        r = RSGTE_OOM;
                        goto donedecode;
                    }
                    memcpy(bh->lastHash.data, subrec.data + 1, subrec.tlvlen - 1);

                    /* Debug verification output */
                    rsksi_printBLOCK_HDR(stdout, bh, verbose);

                    /* Check OLD encoded COUNT */
                    CHKrDecode(rsksi_tlvDecodeSUBREC(&rec, &strtidx, &subrec));
                    if (!(subrec.tlvtype == 0x03 && subrec.tlvlen <= 8)) {
                        r = RSGTE_INVLTYP;
                        goto donedecode;
                    }
                    bs->recCount = 0;
                    for (i = 0; i < subrec.tlvlen; ++i) {
                        bs->recCount = (bs->recCount << 8) + subrec.data[i];
                    }

                    /* Check OLD encoded SIG */
                    CHKrDecode(rsksi_tlvDecodeSUBREC(&rec, &strtidx, &subrec));
                    if (!(subrec.tlvtype == 0x0905)) {
                        r = RSGTE_INVLTYP;
                        goto donedecode;
                    }
                    bs->sig.der.len = subrec.tlvlen;
                    bs->sigID = SIGID_RFC3161;
                    if ((bs->sig.der.data = (uint8_t *)malloc(bs->sig.der.len)) == NULL) {
                        r = RSGTE_OOM;
                        goto donedecode;
                    }
                    memcpy(bs->sig.der.data, subrec.data, bs->sig.der.len);

                    /* Debug output */
                    rsksi_printBLOCK_SIG(stdout, bs, verbose);

                    if (strtidx != rec.tlvlen) {
                        r = RSGTE_LEN;
                        goto donedecode;
                    }

                    /* Set back to NEW default */
                    RSKSI_FLAG_TLV16_RUNTIME = 0x80;

                    /* Create Block Header */
                    tlvlen = 2 + 1 /* hash algo TLV */ + 2 + hashOutputLengthOctetsKSI(bh->hashID) /* iv */ + 2 + 1 +
                             bh->lastHash.len /* last hash */;
                    /* write top-level TLV object block-hdr */
                    CHKrDecode(rsksi_tlv16Write(newsigfp, 0x00, 0x0901, tlvlen));
                    /* and now write the children */
                    /* hash-algo */
                    CHKrDecode(rsksi_tlv8Write(newsigfp, 0x00, 0x01, 1));
                    CHKrDecode(rsksi_tlvfileAddOctet(newsigfp, hashIdentifierKSI(bh->hashID)));
                    /* block-iv */
                    CHKrDecode(rsksi_tlv8Write(newsigfp, 0x00, 0x02, hashOutputLengthOctetsKSI(bh->hashID)));
                    CHKrDecode(rsksi_tlvfileAddOctetString(newsigfp, bh->iv, hashOutputLengthOctetsKSI(bh->hashID)));
                    /* last-hash */
                    CHKrDecode(rsksi_tlv8Write(newsigfp, 0x00, 0x03, bh->lastHash.len + 1));
                    CHKrDecode(rsksi_tlvfileAddOctet(newsigfp, bh->lastHash.hashID));
                    CHKrDecode(rsksi_tlvfileAddOctetString(newsigfp, bh->lastHash.data, bh->lastHash.len));

                    /* Create Block Signature */
                    tlvlenRecords = rsksi_tlvGetInt64OctetSize(bs->recCount);
                    tlvlen = 2 + tlvlenRecords /* rec-count */ + 4 + bs->sig.der.len /* open-ksi */;
                    /* write top-level TLV object (block-sig */
                    CHKrDecode(rsksi_tlv16Write(newsigfp, 0x00, 0x0904, tlvlen));
                    /* and now write the children */
                    /* rec-count */
                    CHKrDecode(rsksi_tlv8Write(newsigfp, 0x00, 0x01, tlvlenRecords));
                    CHKrDecode(rsksi_tlvfileAddInt64(newsigfp, bs->recCount));
                    /* open-ksi */
                    CHKrDecode(rsksi_tlv16Write(newsigfp, 0x00, 0x905, bs->sig.der.len));
                    CHKrDecode(rsksi_tlvfileAddOctetString(newsigfp, bs->sig.der.data, bs->sig.der.len));

                donedecode:
                    /* Set back to OLD default */
                    RSKSI_FLAG_TLV16_RUNTIME = 0x20;

                    /* Free mem*/
                    if (bh != NULL) {
                        free(bh->iv);
                        free(bh->lastHash.data);
                        free(bh);
                        bh = NULL;
                    }
                    if (bs != NULL) {
                        free(bs->sig.der.data);
                        free(bs);
                        bs = NULL;
                    }
                    if (r != 0) goto done;
                    break;
                default:
                    printf("debug: rsksi_ConvertSigFile:\t unknown tlv record %4.4x\n", rec.tlvtype);
                    break;
            }
        } else {
            /*if(feof(oldsigfp))
                break;
            else*/
            r = rRead;
            if (r == RSGTE_EOF)
                r = 0; /* Successfully finished file */
            else if (rsksi_read_debug)
                printf("debug: rsksi_ConvertSigFile:\t failed to read with error %d\n", r);
            goto done;
        }

        /* Abort further processing if EOF */
        if (rRead == RSGTE_EOF) goto done;
    }
done:
    if (rsksi_read_debug) printf("debug: rsksi_ConvertSigFile:\t  returned %d\n", r);
    return r;
}


/* Helper function to write full hashchain! */
int rsksi_WriteHashChain(FILE *newsigfp, block_hashchain_t *hashchain, int verbose) {
    uint8_t j;
    int r = 0;
    unsigned tlvlen;
    uint8_t tlvlenLevelCorr;
    uint8_t uiLevelCorr = 0;

    if (rsksi_read_debug)
        printf("debug: rsksi_WriteHashChain:\t\t NEW HashChain started with %lld Chains\n",
               (long long unsigned)hashchain->stepCount);

    /* Error Check */
    if (hashchain == NULL || hashchain->rec_hash.data == NULL || hashchain->stepCount == 0) {
        r = RSGTE_EXTRACT_HASH;
        goto done;
    }

    /* Total Length of Hash Chain */
    tlvlenLevelCorr = rsksi_tlvGetInt64OctetSize(uiLevelCorr);

    /* Total Length of Hash Chain */
    tlvlen = /*4 +  ???? */
        2 + 1 + hashchain->rec_hash.len /* rec-hash */ +
        ((2 + 2 + tlvlenLevelCorr + 2 + 1 + hashchain->hashsteps[0]->sib_hash.len) * hashchain->stepCount);
    /* Count of all left/right chains*/
    if (rsksi_read_debug) printf("debug: rsksi_WriteHashChain:\t\t tlvlen=%d \n", tlvlen);

    /* Start hash chain for one log record */
    CHKrDecode(rsksi_tlv16Write(newsigfp, 0x00, 0x0907, tlvlen));

    /* rec-hash */
    CHKrDecode(rsksi_tlv8Write(newsigfp, 0x00, 0x01, 1 + hashchain->rec_hash.len));
    CHKrDecode(rsksi_tlvfileAddOctet(newsigfp, hashchain->rec_hash.hashID));
    CHKrDecode(rsksi_tlvfileAddOctetString(newsigfp, hashchain->rec_hash.data, hashchain->rec_hash.len));
    if (rsksi_read_debug) {
        printf("debug: rsksi_WriteHashChain:\t\t Write Record tlvlen=%zu \n", 1 + hashchain->rec_hash.len);
        outputHash(stdout, "debug: rsksi_WriteHashChain:\t\t RECORD Hash: \t\t", hashchain->rec_hash.data,
                   hashchain->rec_hash.len, verbose);
    }

    /* Process Chains */
    for (j = 0; j < hashchain->stepCount; ++j) {
        tlvlen = 2 + tlvlenLevelCorr + 2 + 1 + hashchain->hashsteps[j]->sib_hash.len;

        /* Step in the hash chain*/
        CHKrDecode(rsksi_tlv8Write(newsigfp, 0x00, hashchain->hashsteps[j]->direction, tlvlen));
        /* level correction value */
        CHKrDecode(rsksi_tlv8Write(newsigfp, 0x00, 0x01, tlvlenLevelCorr));
        CHKrDecode(rsksi_tlvfileAddInt64(newsigfp, hashchain->hashsteps[j]->level_corr));
        /* sibling hash value */
        CHKrDecode(rsksi_tlv8Write(newsigfp, 0x00, 0x02, 1 + hashchain->hashsteps[j]->sib_hash.len));
        CHKrDecode(rsksi_tlvfileAddOctet(newsigfp, hashchain->hashsteps[j]->sib_hash.hashID));
        CHKrDecode(rsksi_tlvfileAddOctetString(newsigfp, hashchain->hashsteps[j]->sib_hash.data,
                                               hashchain->hashsteps[j]->sib_hash.len));

        if (rsksi_read_debug) {
            printf(
                "debug: rsksi_WriteHashChain:\t\t WRITE Chain:\t\tTlvlen=%d, %s Direction, "
                "level_corr=%lld\n",
                tlvlen, (hashchain->hashsteps[j]->direction == 0x02) ? "LEFT" : "RIGHT",
                (long long unsigned)hashchain->hashsteps[j]->level_corr);
            outputHash(stdout, "debug: rsksi_WriteHashChain:\t\t Chain Hash: \t\t",
                       hashchain->hashsteps[j]->sib_hash.data, hashchain->hashsteps[j]->sib_hash.len, verbose);
        }
    }
donedecode:
    if (r != 0) printf("debug: rsksi_WriteHashChain:\t\t failed to write with error %d\n", r);
done:
    if (rsksi_read_debug) printf("debug: rsksi_WriteHashChain:\t\t returned %d\n", r);
    return r;
}

/* Helper function to Extract Block Signature */
int rsksi_ExtractBlockSignature(FILE *newsigfp, block_sig_t *bsIn) {
    int r = 0;

    /* WRITE BLOCK Signature */
    /* open-ksi */
    CHKrDecode(rsksi_tlv16Write(newsigfp, 0x00, 0x905, bsIn->sig.der.len));
    CHKrDecode(rsksi_tlvfileAddOctetString(newsigfp, bsIn->sig.der.data, bsIn->sig.der.len));

donedecode:
    if (r != 0) printf("debug: rsksi_ExtractBlockSignature:\t\t failed to write with error %d\n", r);
    if (rsksi_read_debug) printf("debug: ExtractBlockSignature:\t\t returned %d\n", r);
    return r;
}
