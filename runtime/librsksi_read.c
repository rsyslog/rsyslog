/* librsksi_read.c - rsyslog's guardtime support library
 * This includes functions used for reading signature (and 
 * other related) files. Well, actually it also contains
 * some writing functionality, but only as far as rsyslog
 * itself is not concerned, but "just" the utility programs.
 *
 * This part of the library uses C stdio and expects that the
 * caller will open and close the file to be read itself.
 *
 * Copyright 2013-2015 Adiscon GmbH.
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

#include "librsgt_common.h"
#include "librsksi.h"

typedef unsigned char uchar;
#ifndef VERSION
#define VERSION "no-version"
#endif
#define MAXFNAME 1024

static int rsksi_read_debug = 0;
char *rsksi_read_puburl = ""; /* old default http://verify.guardtime.com/gt-controlpublications.bin";*/
char *rsksi_extend_puburl = ""; /* old default "http://verifier.guardtime.net/gt-extendingservice";*/
char *rsksi_userid = "";
char *rsksi_userkey = "";
uint8_t rsksi_read_showVerified = 0;

/* macro to obtain next char from file including error tracking */
#define NEXTC	if((c = fgetc(fp)) == EOF) { \
			r = feof(fp) ? RSGTE_EOF : RSGTE_IO; \
			goto done; \
		}

/* if verbose==0, only the first and last two octets are shown,
 * otherwise everything.
 */
static void
outputHexBlob(FILE *fp, const uint8_t *blob, const uint16_t len, const uint8_t verbose)
{
	unsigned i;
	if(verbose || len <= 8) {
		for(i = 0 ; i < len ; ++i)
			fprintf(fp, "%2.2x", blob[i]);
	} else {
		fprintf(fp, "%2.2x%2.2x%2.2x[...]%2.2x%2.2x%2.2x",
			blob[0], blob[1], blob[2],
			blob[len-3], blob[len-2], blob[len-1]);
	}
}

static inline void
outputKSIHash(FILE *fp, char *hdr, const KSI_DataHash *const __restrict__ hash, const uint8_t verbose)
{
	const unsigned char *digest;
	size_t digest_len;
	KSI_DataHash_extract(hash, NULL, &digest, &digest_len); // TODO: error check

	fprintf(fp, "%s", hdr);
	outputHexBlob(fp, digest, digest_len, verbose);
	fputc('\n', fp);
}

static inline void
outputHash(FILE *fp, const char *hdr, const uint8_t *data,
	const uint16_t len, const uint8_t verbose)
{
	fprintf(fp, "%s", hdr);
	outputHexBlob(fp, data, len, verbose);
	fputc('\n', fp);
}

void
rsksi_errctxInit(ksierrctx_t *ectx)
{
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
void
rsksi_errctxExit(ksierrctx_t *ectx)
{
	free(ectx->filename);
	free(ectx->frstRecInBlk);
}

/* note: we do not copy the record, so the caller MUST not destruct
 * it before processing of the record is completed. To remove the
 * current record without setting a new one, call this function
 * with rec==NULL.
 */
void
rsksi_errctxSetErrRec(ksierrctx_t *ectx, char *rec)
{
	ectx->errRec = strdup(rec);
}
/* This stores the block's first record. Here we copy the data,
 * as the caller will usually not preserve it long enough.
 */
void
rsksi_errctxFrstRecInBlk(ksierrctx_t *ectx, char *rec)
{
	free(ectx->frstRecInBlk);
	ectx->frstRecInBlk = strdup(rec);
}

static void
reportError(const int errcode, ksierrctx_t *ectx)
{
	if(ectx->fp != NULL) {
		fprintf(ectx->fp, "%s[%llu:%llu:%llu]: error[%u]: %s\n",
			ectx->filename,
			(long long unsigned) ectx->blkNum, (long long unsigned) ectx->recNum,
			(long long unsigned) ectx->recNumInFile,
			errcode, RSKSIE2String(errcode));
		if(ectx->frstRecInBlk != NULL)
			fprintf(ectx->fp, "\tBlock Start Record.: '%s'\n", ectx->frstRecInBlk);
		if(ectx->errRec != NULL)
			fprintf(ectx->fp, "\tRecord in Question.: '%s'\n", ectx->errRec);
		if(ectx->computedHash != NULL) {
			outputKSIHash(ectx->fp, "\tComputed Hash......: ", ectx->computedHash,
				ectx->verbose);
		}
		if(ectx->fileHash != NULL) {
			outputHash(ectx->fp, "\tSignature File Hash: ", ectx->fileHash->data,
				ectx->fileHash->len, ectx->verbose);
		}
		if(errcode == RSGTE_INVLD_TREE_HASH ||
		   errcode == RSGTE_INVLD_TREE_HASHID) {
			fprintf(ectx->fp, "\tTree Level.........: %d\n", (int) ectx->treeLevel);
			outputKSIHash(ectx->fp, "\tTree Left Hash.....: ", ectx->lefthash,
				ectx->verbose);
			outputKSIHash(ectx->fp, "\tTree Right Hash....: ", ectx->righthash,
				ectx->verbose);
		}
		if(errcode == RSGTE_INVLD_SIGNATURE ||
		   errcode == RSGTE_TS_CREATEHASH) {
			fprintf(ectx->fp, "\tPublication Server.: %s\n", rsksi_read_puburl);
			fprintf(ectx->fp, "\tKSI Verify Signature: [%u]%s\n",
				ectx->ksistate, KSI_getErrorString(ectx->ksistate));
		}
		if(errcode == RSGTE_SIG_EXTEND ||
		   errcode == RSGTE_TS_CREATEHASH) {
			fprintf(ectx->fp, "\tExtending Server...: %s\n", rsksi_extend_puburl);
			fprintf(ectx->fp, "\tKSI Extend Signature: [%u]%s\n",
				ectx->ksistate, KSI_getErrorString(ectx->ksistate));
		}
		if(errcode == RSGTE_TS_DERENCODE) {
			fprintf(ectx->fp, "\tAPI return state...: [%u]%s\n",
				ectx->ksistate, KSI_getErrorString(ectx->ksistate));
		}
	}
}

/* obviously, this is not an error-reporting function. We still use
 * ectx, as it has most information we need.
 */

static void
reportVerifySuccess(ksierrctx_t *ectx) /*OLD CODE , GTVerificationInfo *vrfyInf)*/
{
	if(ectx->fp != NULL) {
		fprintf(ectx->fp, "%s[%llu:%llu:%llu]: block signature successfully verified\n",
			ectx->filename,
			(long long unsigned) ectx->blkNum, (long long unsigned) ectx->recNum,
			(long long unsigned) ectx->recNumInFile);
		if(ectx->frstRecInBlk != NULL)
			fprintf(ectx->fp, "\tBlock Start Record.: '%s'\n", ectx->frstRecInBlk);
		if(ectx->errRec != NULL)
			fprintf(ectx->fp, "\tBlock End Record...: '%s'\n", ectx->errRec);
		fprintf(ectx->fp, "\tKSI Verify Signature: [%u]%s\n",
			ectx->ksistate, KSI_getErrorString(ectx->ksistate));
		/* OLDCODE: NOT NEEDED ANYMORE GTVerificationInfo_print(ectx->fp, 0, vrfyInf);*/
	}
}

/* return the actual length in to-be-written octets of an integer */
static inline uint8_t rsksi_tlvGetInt64OctetSize(uint64_t val)
{
	if(val >> 56)
		return 8;
	if((val >> 48) & 0xff)
		return 7;
	if((val >> 40) & 0xff)
		return 6;
	if((val >> 32) & 0xff)
		return 5;
	if((val >> 24) & 0xff)
		return 4;
	if((val >> 16) & 0xff)
		return 3;
	if((val >> 8) & 0xff)
		return 2;
	return 1;
}

static inline int rsksi_tlvfileAddOctet(FILE *newsigfp, int8_t octet)
{
	/* Directory write into file */
	int r = 0;
	if ( fputc(octet, newsigfp) == EOF ) 
		r = RSGTE_IO; 
done:	return r;
}
static inline int rsksi_tlvfileAddOctetString(FILE *newsigfp, uint8_t *octet, int size)
{
	int i, r = 0;
	for(i = 0 ; i < size ; ++i) {
		r = rsksi_tlvfileAddOctet(newsigfp, octet[i]);
		if(r != 0) goto done;
	}
done:	return r;
}
static inline int rsksi_tlvfileAddInt64(FILE *newsigfp, uint64_t val)
{
	uint8_t doWrite = 0;
	int r;
	if(val >> 56) {
		r = rsksi_tlvfileAddOctet(newsigfp, (val >> 56) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 48) & 0xff)) {
		r = rsksi_tlvfileAddOctet(newsigfp, (val >> 48) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 40) & 0xff)) {
		r = rsksi_tlvfileAddOctet(newsigfp, (val >> 40) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 32) & 0xff)) {
		r = rsksi_tlvfileAddOctet(newsigfp, (val >> 32) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 24) & 0xff)) {
		r = rsksi_tlvfileAddOctet(newsigfp, (val >> 24) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 16) & 0xff)) {
		r = rsksi_tlvfileAddOctet(newsigfp, (val >> 16) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 8) & 0xff)) {
		r = rsksi_tlvfileAddOctet(newsigfp, (val >>  8) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	r = rsksi_tlvfileAddOctet(newsigfp, val & 0xff);
done:	return r;
}

static int
rsksi_tlv8Write(FILE *newsigfp, int flags, int tlvtype, int len)
{
	int r;
	assert((flags & RSGT_TYPE_MASK) == 0);
	assert((tlvtype & RSGT_TYPE_MASK) == tlvtype);
	r = rsksi_tlvfileAddOctet(newsigfp, (flags & ~RSKSI_FLAG_TLV16_RUNTIME) | tlvtype);
	if(r != 0) goto done;
	r = rsksi_tlvfileAddOctet(newsigfp, len & 0xff);
done:	return r;
} 

static int
rsksi_tlv16Write(FILE *newsigfp, int flags, int tlvtype, uint16_t len)
{
	uint16_t typ;
	int r;
	assert((flags & RSGT_TYPE_MASK) == 0);
	assert((tlvtype >> 8 & RSGT_TYPE_MASK) == (tlvtype >> 8));
	typ = ((flags | RSKSI_FLAG_TLV16_RUNTIME) << 8) | tlvtype;
	r = rsksi_tlvfileAddOctet(newsigfp, typ >> 8);
	if(r != 0) goto done;
	r = rsksi_tlvfileAddOctet(newsigfp, typ & 0xff);
	if(r != 0) goto done;
	r = rsksi_tlvfileAddOctet(newsigfp, (len >> 8) & 0xff);
	if(r != 0) goto done;
	r = rsksi_tlvfileAddOctet(newsigfp, len & 0xff);
done:	return r;
} 

/**
 * Write the provided record to the current file position.
 *
 * @param[in] fp file pointer for writing
 * @param[out] rec tlvrecord to write
 *
 * @returns 0 if ok, something else otherwise
 */
static int
rsksi_tlvwrite(FILE *fp, tlvrecord_t *rec)
{
	int r = RSGTE_IO;
	if(fwrite(rec->hdr, (size_t) rec->lenHdr, 1, fp) != 1) goto done;
	if(fwrite(rec->data, (size_t) rec->tlvlen, 1, fp) != 1) goto done;
	r = 0;
done:	return r;
}

/**
 * Read a header from a binary file.
 * @param[in] fp file pointer for processing
 * @param[in] hdr buffer for the header. Must be 9 bytes 
 * 		  (8 for header + NUL byte)
 * @returns 0 if ok, something else otherwise
 */
int
rsksi_tlvrdHeader(FILE *fp, uchar *hdr)
{
	int r;
	if(fread(hdr, 8, 1, fp) != 1) {
		r = RSGTE_IO;
		goto done;
	}
	hdr[8] = '\0';
	r = 0;
done:	return r;
}

/* read type a complete tlv record 
 */
static int
rsksi_tlvRecRead(FILE *fp, tlvrecord_t *rec)
{
	int r = 1;
	int c;

	NEXTC;
	rec->hdr[0] = c;
	rec->tlvtype = c & 0x1f;
	if(c & RSKSI_FLAG_TLV16_RUNTIME) { /* tlv16? */
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
	if(fread(rec->data, (size_t) rec->tlvlen, 1, fp) != 1) {
		r = feof(fp) ? RSGTE_EOF : RSGTE_IO;
		goto done;
	}

	r = 0;
done:	return r;
	if(r == 0 && rsksi_read_debug)
		/* Only show debug if no fail */
		printf("debug: read tlvtype %4.4x, len %u\n", (unsigned) rec->tlvtype,
			(unsigned) rec->tlvlen);
}

/* decode a sub-tlv record from an existing record's memory buffer
 */
static int
rsksi_tlvDecodeSUBREC(tlvrecord_t *rec, uint16_t *stridx, tlvrecord_t *newrec)
{
	int r = 1;
	int c;

	if(rec->tlvlen == *stridx) {r=RSGTE_LEN; goto done;}
	c = rec->data[(*stridx)++];
	newrec->hdr[0] = c;
	newrec->tlvtype = c & 0x1f;
	if(c & RSKSI_FLAG_TLV16_RUNTIME) { /* tlv16? */
		newrec->lenHdr = 4;
		if(rec->tlvlen == *stridx) {r=RSGTE_LEN; goto done;}
		c = rec->data[(*stridx)++];
		newrec->hdr[1] = c;
		newrec->tlvtype = (newrec->tlvtype << 8) | c;
		if(rec->tlvlen == *stridx) {r=RSGTE_LEN; goto done;}
		c = rec->data[(*stridx)++];
		newrec->hdr[2] = c;
		newrec->tlvlen = c << 8;
		if(rec->tlvlen == *stridx) {r=RSGTE_LEN; goto done;}
		c = rec->data[(*stridx)++];
		newrec->hdr[3] = c;
		newrec->tlvlen |= c;
	} else {
		if(rec->tlvlen == *stridx) {r=RSGTE_LEN; goto done;}
		c = rec->data[(*stridx)++];
		newrec->lenHdr = 2;
		newrec->hdr[1] = c;
		newrec->tlvlen = c;
	}
	if(rec->tlvlen < *stridx + newrec->tlvlen) {r=RSGTE_LEN; goto done;}
	memcpy(newrec->data, (rec->data)+(*stridx), newrec->tlvlen);
	*stridx += newrec->tlvlen;

	if(rsksi_read_debug)
		printf("debug: read sub-tlv: tlvtype %4.4x, len %u\n",
			(unsigned) newrec->tlvtype,
			(unsigned) newrec->tlvlen);
	r = 0;
done:	return r;
}


static int
rsksi_tlvDecodeIMPRINT(tlvrecord_t *rec, imprint_t **imprint)
{
	int r = 1;
	imprint_t *imp = NULL;

	if((imp = calloc(1, sizeof(imprint_t))) == NULL) {
		r = RSGTE_OOM;
		goto done;
	}

	imp->hashID = rec->data[0];
	if(rec->tlvlen != 1 + hashOutputLengthOctetsKSI(imp->hashID)) {
		r = RSGTE_LEN;
		goto done;
	}
	imp->len = rec->tlvlen - 1;
	if((imp->data = (uint8_t*)malloc(imp->len)) == NULL) {r=RSGTE_OOM;goto done;}
	memcpy(imp->data, rec->data+1, imp->len);
	*imprint = imp;
	r = 0;
done:	
	if(r == 0) {
		if (rsksi_read_debug)
			printf("debug: read tlvDecodeIMPRINT returned %d TLVLen=%d, HashID=%d\n", r, rec->tlvlen, imp->hashID);
	} else { 
		/* Free memory on FAIL!*/
		if (imp != NULL)
			rsksi_objfree(rec->tlvtype, imp);
	}
	return r;
}

static int
rsksi_tlvDecodeHASH_ALGO(tlvrecord_t *rec, uint16_t *strtidx, uint8_t *hashAlg)
{
	int r = 1;
	tlvrecord_t subrec;

	CHKr(rsksi_tlvDecodeSUBREC(rec, strtidx, &subrec));
	if(!(subrec.tlvtype == 0x01 && subrec.tlvlen == 1)) {
		r = RSGTE_FMT;
		goto done;
	}
	*hashAlg = subrec.data[0];
	r = 0;
done:	return r;
}
static int
rsksi_tlvDecodeBLOCK_IV(tlvrecord_t *rec, uint16_t *strtidx, uint8_t **iv)
{
	int r = 1;
	tlvrecord_t subrec;

	CHKr(rsksi_tlvDecodeSUBREC(rec, strtidx, &subrec));
	if(!(subrec.tlvtype == 0x02)) {
		r = RSGTE_INVLTYP;
		goto done;
	}
	if((*iv = (uint8_t*)malloc(subrec.tlvlen)) == NULL) {r=RSGTE_OOM;goto done;}
	memcpy(*iv, subrec.data, subrec.tlvlen);
	r = 0;
done:	return r;
}
static int
rsksi_tlvDecodeLAST_HASH(tlvrecord_t *rec, uint16_t *strtidx, imprint_t *imp)
{
	int r = 1;
	tlvrecord_t subrec;

	CHKr(rsksi_tlvDecodeSUBREC(rec, strtidx, &subrec));
	if(!(subrec.tlvtype == 0x03)) { r = RSGTE_INVLTYP; goto done; }
	imp->hashID = subrec.data[0];
	if(subrec.tlvlen != 1 + hashOutputLengthOctetsKSI(imp->hashID)) {
		r = RSGTE_LEN;
		goto done;
	}
	imp->len = subrec.tlvlen - 1;
	if((imp->data = (uint8_t*)malloc(imp->len)) == NULL) {r=RSGTE_OOM;goto done;}
	memcpy(imp->data, subrec.data+1, subrec.tlvlen-1);
	r = 0;
done:	return r;
}
static int
rsksi_tlvDecodeREC_COUNT(tlvrecord_t *rec, uint16_t *strtidx, uint64_t *cnt)
{
	int r = 1;
	int i;
	uint64_t val;
	tlvrecord_t subrec;

	CHKr(rsksi_tlvDecodeSUBREC(rec, strtidx, &subrec));
	if(!(subrec.tlvtype == 0x01 && subrec.tlvlen <= 8)) { r = RSGTE_INVLTYP; goto done; }
	val = 0;
	for(i = 0 ; i < subrec.tlvlen ; ++i) {
		val = (val << 8) + subrec.data[i];
	}
	*cnt = val;
	r = 0;
done:	return r;
}
static int
rsksi_tlvDecodeSIG(tlvrecord_t *rec, uint16_t *strtidx, block_sig_t *bs)
{
	int r = 1;
	tlvrecord_t subrec;

	CHKr(rsksi_tlvDecodeSUBREC(rec, strtidx, &subrec));
	if(!(subrec.tlvtype == 0x0905)) { r = RSGTE_INVLTYP; goto done; }
	bs->sig.der.len = subrec.tlvlen;
	bs->sigID = SIGID_RFC3161;
	if((bs->sig.der.data = (uint8_t*)malloc(bs->sig.der.len)) == NULL) {r=RSGTE_OOM;goto done;}
	memcpy(bs->sig.der.data, subrec.data, bs->sig.der.len);
	r = 0;
done:	return r;
}

static int
rsksi_tlvDecodeBLOCK_HDR(tlvrecord_t *rec, block_hdr_t **blockhdr)
{
	int r = 1;
	uint16_t strtidx = 0;
	block_hdr_t *bh = NULL;
	if((bh = calloc(1, sizeof(block_hdr_t))) == NULL) {
		r = RSGTE_OOM;
		goto done;
	}
	CHKr(rsksi_tlvDecodeHASH_ALGO(rec, &strtidx, &(bh->hashID)));
	CHKr(rsksi_tlvDecodeBLOCK_IV(rec, &strtidx, &(bh->iv)));
	CHKr(rsksi_tlvDecodeLAST_HASH(rec, &strtidx, &(bh->lastHash)));
	if(strtidx != rec->tlvlen) {
		r = RSGTE_LEN;
		goto done;
	}
	*blockhdr = bh;
	r = 0;
done:	
	if (r == 0) {
		if(rsksi_read_debug)
			printf("debug: tlvDecodeBLOCK_HDR returned %d, tlvtype %4.4x\n", r, (unsigned) rec->tlvtype);
	} else { 
		/* Free memory on FAIL!*/
		if (bh != NULL)
			rsksi_objfree(rec->tlvtype, bh);
	}
	return r;
}

static int
rsksi_tlvDecodeBLOCK_SIG(tlvrecord_t *rec, block_sig_t **blocksig)
{
	int r = 1;
	uint16_t strtidx = 0;
	block_sig_t *bs = NULL;
	if((bs = calloc(1, sizeof(block_sig_t))) == NULL) {
		r = RSGTE_OOM;
		goto done;
	}
	CHKr(rsksi_tlvDecodeREC_COUNT(rec, &strtidx, &(bs->recCount)));
	CHKr(rsksi_tlvDecodeSIG(rec, &strtidx, bs));
	if(strtidx != rec->tlvlen) {
		r = RSGTE_LEN;
		goto done;
	}
	*blocksig = bs;
	r = 0;
done:	
	if(r == 0) {
		if (rsksi_read_debug)
			printf("debug: rsksi_tlvDecodeBLOCK_SIG returned %d, tlvtype %4.4x\n", r, (unsigned) rec->tlvtype);
	} else { 
		/* Free memory on FAIL!*/
		if (bs != NULL)
			rsksi_objfree(rec->tlvtype, bs);
	}	
	return r;
}
static int
rsksi_tlvRecDecode(tlvrecord_t *rec, void *obj)
{
	int r = 1;
	switch(rec->tlvtype) {
		case 0x0901:
			r = rsksi_tlvDecodeBLOCK_HDR(rec, obj);
			if(r != 0) goto done;
			break;
		case 0x0902:
		case 0x0903:
			r = rsksi_tlvDecodeIMPRINT(rec, obj);
			if(r != 0) goto done;
			break;
		case 0x0904:
			r = rsksi_tlvDecodeBLOCK_SIG(rec, obj);
			if(r != 0) goto done;
			break;
	}
done:
	if(r == 0 && rsksi_read_debug)
		printf("debug: tlvRecDecode returned %d, tlvtype %4.4x\n", r, (unsigned) rec->tlvtype);
	return r;
}

static int
rsksi_tlvrdRecHash(FILE *fp, FILE *outfp, imprint_t **imp)
{
	int r;
	tlvrecord_t rec;

	if((r = rsksi_tlvrd(fp, &rec, imp)) != 0) goto done;
	if(rec.tlvtype != 0x0902) {
		r = RSGTE_MISS_REC_HASH;
		rsksi_objfree(rec.tlvtype, *imp);
		*imp = NULL; 
		goto done;
	}
	if(outfp != NULL)
		if((r = rsksi_tlvwrite(outfp, &rec)) != 0) goto done;
	r = 0;
done:	
	if(r == 0 && rsksi_read_debug)
		printf("debug: tlvrdRecHash returned %d, rec->tlvtype %4.4x\n", r, (unsigned) rec.tlvtype);
	return r;
}

static int
rsksi_tlvrdTreeHash(FILE *fp, FILE *outfp, imprint_t **imp)
{
	int r;
	tlvrecord_t rec;

	if((r = rsksi_tlvrd(fp, &rec, imp)) != 0) goto done;
	if(rec.tlvtype != 0x0903) {
		r = RSGTE_MISS_TREE_HASH;
		rsksi_objfree(rec.tlvtype, *imp);
		*imp = NULL; 
		goto done;
	}
	if(outfp != NULL)
		if((r = rsksi_tlvwrite(outfp, &rec)) != 0) goto done;
	r = 0;
done:	
	if(r == 0 && rsksi_read_debug)
		printf("debug: tlvrdTreeHash returned %d, rec->tlvtype %4.4x\n", r, (unsigned) rec.tlvtype);
	return r;
}

/* read BLOCK_SIG during verification phase */
static int
rsksi_tlvrdVrfyBlockSig(FILE *fp, block_sig_t **bs, tlvrecord_t *rec)
{
	int r;

	if((r = rsksi_tlvrd(fp, rec, bs)) != 0) goto done;
	if(rec->tlvtype != 0x0904) {
		r = RSGTE_MISS_BLOCKSIG;
		rsksi_objfree(rec->tlvtype, *bs);
		goto done;
	}
	r = 0;
done:	return r;
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
int
rsksi_tlvrd(FILE *fp, tlvrecord_t *rec, void *obj)
{
	int r;
	if((r = rsksi_tlvRecRead(fp, rec)) != 0) goto done;
	r = rsksi_tlvRecDecode(rec, obj);
done:	return r;
}


/* return if a blob is all zero */
static inline int
blobIsZero(uint8_t *blob, uint16_t len)
{
	int i;
	for(i = 0 ; i < len ; ++i)
		if(blob[i] != 0)
			return 0;
	return 1;
}

static void
rsksi_printIMPRINT(FILE *fp, char *name, imprint_t *imp, uint8_t verbose)
{
	fprintf(fp, "%s", name);
		outputHexBlob(fp, imp->data, imp->len, verbose);
		fputc('\n', fp);
}

static void
rsksi_printREC_HASH(FILE *fp, imprint_t *imp, uint8_t verbose)
{
	rsksi_printIMPRINT(fp, "[0x0902]Record hash: ",
		imp, verbose);
}

static void
rsksi_printINT_HASH(FILE *fp, imprint_t *imp, uint8_t verbose)
{
	rsksi_printIMPRINT(fp, "[0x0903]Tree hash..: ",
		imp, verbose);
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
void
rsksi_printBLOCK_HDR(FILE *fp, block_hdr_t *bh, uint8_t verbose)
 {
	fprintf(fp, "[0x0901]Block Header Record:\n");
 	fprintf(fp, "\tPrevious Block Hash:\n");
	fprintf(fp, "\t   Algorithm..: %s\n", hashAlgNameKSI(bh->lastHash.hashID));
 	fprintf(fp, "\t   Hash.......: ");
		outputHexBlob(fp, bh->lastHash.data, bh->lastHash.len, verbose);
 		fputc('\n', fp);
	if(blobIsZero(bh->lastHash.data, bh->lastHash.len))
 		fprintf(fp, "\t   NOTE: New Hash Chain Start!\n");
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
void
rsksi_printBLOCK_SIG(FILE *fp, block_sig_t *bs, uint8_t verbose)
{
	fprintf(fp, "[0x0904]Block Signature Record:\n");
	fprintf(fp, "\tRecord Count..: %llu\n", (long long unsigned) bs->recCount);
	fprintf(fp, "\tSignature Type: %s\n", sigTypeName(bs->sigID));
	fprintf(fp, "\tSignature Len.: %u\n", (unsigned) bs->sig.der.len);
	fprintf(fp, "\tSignature.....: ");
		outputHexBlob(fp, bs->sig.der.data, bs->sig.der.len, verbose);
		fputc('\n', fp);
}


/**
 * Output a human-readable representation of a tlv object.
 *
 * @param[in] fp file pointer to send output to
 * @param[in] tlvtype type of tlv object (record)
 * @param[in] verbose if 0, abbreviate blob hexdump, else complete
 */
void
rsksi_tlvprint(FILE *fp, uint16_t tlvtype, void *obj, uint8_t verbose)
{
	switch(tlvtype) {
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
	default:fprintf(fp, "unknown tlv record %4.4x\n", tlvtype);
		break;
	}
}

/**
 * Free the provided object.
 *
 * @param[in] tlvtype type of tlv object (record)
 * @param[in] obj the object to be destructed
 */
void
rsksi_objfree(uint16_t tlvtype, void *obj)
{
	// check if obj is valid 
	if (obj == NULL )
		return; 

	switch(tlvtype) {
	case 0x0901:
		free(((block_hdr_t*)obj)->iv);
		free(((block_hdr_t*)obj)->lastHash.data);
		break;
	case 0x0902:
	case 0x0903:
		free(((imprint_t*)obj)->data);
		break;
	case 0x0904:
		free(((block_sig_t*)obj)->sig.der.data);
		break;
	default:fprintf(stderr, "rsksi_objfree: unknown tlv record %4.4x\n",
		        tlvtype);
		break;
	}
	free(obj);
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
int
rsksi_getBlockParams(FILE *fp, uint8_t bRewind, block_sig_t **bs, block_hdr_t **bh,
                    uint8_t *bHasRecHashes, uint8_t *bHasIntermedHashes)
{
	int r;
	uint64_t nRecs = 0;
	uint8_t bDone = 0;
	uint8_t bHdr = 0;
	off_t rewindPos = 0;
	void *obj;
	tlvrecord_t rec;

	if(bRewind)
		rewindPos = ftello(fp);
	*bHasRecHashes = 0;
	*bHasIntermedHashes = 0;
	*bs = NULL;
	*bh = NULL;

	while(!bDone) { /* we will err out on EOF */
		if((r = rsksi_tlvrd(fp, &rec, &obj)) != 0) goto done;
		bHdr = 0;
		switch(rec.tlvtype) {
		case 0x0901:
			*bh = (block_hdr_t*) obj;
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
			*bs = (block_sig_t*) obj;
			bDone = 1;
			break;
		default:fprintf(fp, "unknown tlv record %4.4x\n", rec.tlvtype);
			break;
		}
		if(!bDone && !bHdr)
			rsksi_objfree(rec.tlvtype, obj);
	}

	if(*bHasRecHashes && (nRecs != (*bs)->recCount)) {
		r = RSGTE_INVLD_RECCNT;
		goto done;
	}

	if(bRewind) {
		if(fseeko(fp, rewindPos, SEEK_SET) != 0) {
			r = RSGTE_IO;
			goto done;
		}
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
int
rsksi_chkFileHdr(FILE *fp, char *expect)
{
	int r;
	char hdr[9];

	if((r = rsksi_tlvrdHeader(fp, (uchar*)hdr)) != 0) goto done;
	if(strcmp(hdr, expect))
		r = RSGTE_INVLHDR;
	else
		r = 0;
done:
	return r;
}

ksifile
rsksi_vrfyConstruct_gf(void)
{
	int ksistate;
	ksifile ksi;
	if((ksi = calloc(1, sizeof(struct ksifile_s))) == NULL)
		goto done;
	ksi->x_prev = NULL;

	/* Create new KSI Context! */
	rsksictx ctx = rsksiCtxNew();
	ksi->ctx = ctx; /* assign context to ksifile */

	/* Setting KSI Publication URL ! */ 

	ksistate = KSI_CTX_setPublicationUrl(ksi->ctx->ksi_ctx, rsksi_read_puburl);
	if(ksistate != KSI_OK) {
		fprintf(stderr, "Failed setting KSI Publication URL '%s' with error (%d): %s\n", rsksi_read_puburl, ksistate, KSI_getErrorString(ksistate));
		free(ksi);
		return NULL;
	}
	if(rsksi_read_debug)
		fprintf(stdout, "PublicationUrl set to: '%s'\n", rsksi_read_puburl);

	/* Setting KSI Extender! */ 
	ksistate = KSI_CTX_setExtender(ksi->ctx->ksi_ctx, rsksi_extend_puburl, rsksi_userid, rsksi_userkey);
	if(ksistate != KSI_OK) {
		fprintf(stderr, "Failed setting KSIExtender URL '%s' with error (%d): %s\n", rsksi_extend_puburl, ksistate, KSI_getErrorString(ksistate));
		free(ksi);
		return NULL;
	}
	if(rsksi_read_debug)
		fprintf(stdout, "ExtenderUrl set to: '%s'\n", rsksi_extend_puburl);

done:	return ksi;
}

void
rsksi_vrfyBlkInit(ksifile ksi, block_hdr_t *bh, uint8_t bHasRecHashes, uint8_t bHasIntermedHashes)
{
	ksi->hashAlg = hashID2AlgKSI(bh->hashID);
	ksi->bKeepRecordHashes = bHasRecHashes;
	ksi->bKeepTreeHashes = bHasIntermedHashes;
	free(ksi->IV);
	ksi->IV = malloc(getIVLenKSI(bh));
	memcpy(ksi->IV, bh->iv, getIVLenKSI(bh));
	ksi->x_prev = malloc(sizeof(imprint_t));
	ksi->x_prev->len=bh->lastHash.len;
	ksi->x_prev->hashID = bh->lastHash.hashID;
	ksi->x_prev->data = malloc(ksi->x_prev->len);
	memcpy(ksi->x_prev->data, bh->lastHash.data, ksi->x_prev->len);
}

static int
rsksi_vrfy_chkRecHash(ksifile ksi, FILE *sigfp, FILE *nsigfp, 
		     KSI_DataHash *hash, ksierrctx_t *ectx)
{
	int r = 0;
	imprint_t *imp = NULL;

	const unsigned char *digest;
	KSI_DataHash_extract(hash, NULL, &digest, NULL); // TODO: error check

	if((r = rsksi_tlvrdRecHash(sigfp, nsigfp, &imp)) != 0)
		reportError(r, ectx);
		goto done;
	if(imp->hashID != hashIdentifierKSI(ksi->hashAlg)) {
		reportError(r, ectx);
		r = RSGTE_INVLD_REC_HASHID;
		goto done;
	}
	if(memcmp(imp->data, digest,
		  hashOutputLengthOctetsKSI(imp->hashID))) {
		r = RSGTE_INVLD_REC_HASH;
		ectx->computedHash = hash;
		ectx->fileHash = imp;
		reportError(r, ectx);
		ectx->computedHash = NULL, ectx->fileHash = NULL;
		goto done;
	}
	r = 0;
done:
	if(imp != NULL)
		rsksi_objfree(0x0902, imp);
	return r;
}

static int
rsksi_vrfy_chkTreeHash(ksifile ksi, FILE *sigfp, FILE *nsigfp,
                      KSI_DataHash *hash, ksierrctx_t *ectx)
{
	int r = 0;
	imprint_t *imp = NULL;
	const unsigned char *digest;
	KSI_DataHash_extract(hash, NULL, &digest, NULL); // TODO: error check


	if((r = rsksi_tlvrdTreeHash(sigfp, nsigfp, &imp)) != 0) {
		reportError(r, ectx);
		goto done;
	}
	if(imp->hashID != hashIdentifierKSI(ksi->hashAlg)) {
		reportError(r, ectx);
		r = RSGTE_INVLD_TREE_HASHID;
		goto done;
	}
	if(memcmp(imp->data, digest,
		  hashOutputLengthOctetsKSI(imp->hashID))) {
		r = RSGTE_INVLD_TREE_HASH;
		ectx->computedHash = hash;
		ectx->fileHash = imp;
		reportError(r, ectx);
		ectx->computedHash = NULL, ectx->fileHash = NULL;
		goto done;
	}
	r = 0;
done:
	if(imp != NULL) {
		if(rsksi_read_debug)
			printf("debug: rsgt_vrfy_chkTreeHash returned %d, hashID=%d, Length=%d\n", r, imp->hashID, hashOutputLengthOctetsKSI(imp->hashID));
		/* Free memory */
		rsksi_objfree(0x0903, imp);
	}
	return r;
}

int
rsksi_vrfy_nextRec(ksifile ksi, FILE *sigfp, FILE *nsigfp,
	          unsigned char *rec, size_t len, ksierrctx_t *ectx)
{
	int r = 0;
	KSI_DataHash *x; /* current hash */
	KSI_DataHash *m, *recHash = NULL, *t, *t_del;
	uint8_t j;

	hash_m_ksi(ksi, &m);
	hash_r_ksi(ksi, &recHash, rec, len);
	if(ksi->bKeepRecordHashes) {
		r = rsksi_vrfy_chkRecHash(ksi, sigfp, nsigfp, recHash, ectx);
		if(r != 0) goto done;
	}
	hash_node_ksi(ksi, &x, m, recHash, 1); /* hash leaf */
	if(ksi->bKeepTreeHashes) {
		ectx->treeLevel = 0;
		ectx->lefthash = m;
		ectx->righthash = recHash;
		r = rsksi_vrfy_chkTreeHash(ksi, sigfp, nsigfp, x, ectx);
		if(r != 0) goto done;
	}
	rsksiimprintDel(ksi->x_prev);
	ksi->x_prev = rsksiImprintFromKSI_DataHash(ksi, x);
	/* add x to the forest as new leaf, update roots list */
	t = x;
	for(j = 0 ; j < ksi->nRoots ; ++j) {
		if(ksi->roots_valid[j] == 0) {
			ksi->roots_hash[j] = t;
			ksi->roots_valid[j] = 1;
			t = NULL;
			break;
		} else if(t != NULL) {
			/* hash interim node */
			ectx->treeLevel = j+1;
			ectx->righthash = t;
			t_del = t;
			hash_node_ksi(ksi, &t, ksi->roots_hash[j], t_del, j+2);
			ksi->roots_valid[j] = 0;
			if(ksi->bKeepTreeHashes) {
				ectx->lefthash = ksi->roots_hash[j];
				r = rsksi_vrfy_chkTreeHash(ksi, sigfp, nsigfp, t, ectx);
				if(r != 0) goto done; /* mem leak ok, we terminate! */
			}
			KSI_DataHash_free(ksi->roots_hash[j]);
			KSI_DataHash_free(t_del);
		}
	}
	if(t != NULL) {
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
	if(recHash != NULL)
		KSI_DataHash_free(recHash);
	return r;
}


/* TODO: think about merging this with the writer. The
 * same applies to the other computation algos.
 */
static int
verifySigblkFinish(ksifile ksi, KSI_DataHash **pRoot)
{
	KSI_DataHash *root, *rootDel;
	int8_t j;
	int r;

	if(ksi->nRecords == 0)
		goto done;

	root = NULL;
	for(j = 0 ; j < ksi->nRoots ; ++j) {
		if(root == NULL) {
			root = ksi->roots_valid[j] ? ksi->roots_hash[j] : NULL;
			ksi->roots_valid[j] = 0; /* guess this is redundant with init, maybe del */
		} else if(ksi->roots_valid[j]) {
			rootDel = root;
			hash_node_ksi(ksi, &root, ksi->roots_hash[j], root, j+2);
			ksi->roots_valid[j] = 0; /* guess this is redundant with init, maybe del */
			KSI_DataHash_free(rootDel);
		}
	}

	*pRoot = root;
	r = 0;
done:
	ksi->bInBlk = 0;
	return r;
}

/* helper for rsksi_extendSig: */
#define COPY_SUBREC_TO_NEWREC \
	memcpy(newrec.data+iWr, subrec.hdr, subrec.lenHdr); \
	iWr += subrec.lenHdr; \
	memcpy(newrec.data+iWr, subrec.data, subrec.tlvlen); \
	iWr += subrec.tlvlen;

static inline int
rsksi_extendSig(KSI_Signature *sig, ksifile ksi, tlvrecord_t *rec, ksierrctx_t *ectx)
{
	KSI_Signature *extended = NULL;
	uint8_t *der = NULL;
	size_t lenDer;
	int r, rgt;
	tlvrecord_t newrec, subrec;
	uint16_t iRd, iWr;

	/* Extend Signature now using KSI API*/
	rgt = KSI_extendSignature(ksi->ctx->ksi_ctx, sig, &extended);
	if (rgt != KSI_OK) {
		ectx->ksistate = rgt;
		r = RSGTE_SIG_EXTEND;
		goto done;
	}

	/* Serialize Signature. */
	rgt = KSI_Signature_serialize(extended, &der, &lenDer);
	if(rgt != KSI_OK) {
		ectx->ksistate = rgt;
		r = RSGTE_SIG_EXTEND;
		goto done;
	}

	/* update block_sig tlv record with new extended timestamp */
	/* we now need to copy all tlv records before the actual der
	 * encoded part.
	 */
	iRd = iWr = 0;
	// TODO; check tlvtypes at comment places below!
	CHKr(rsksi_tlvDecodeSUBREC(rec, &iRd, &subrec)); 
	/* HASH_ALGO */
	COPY_SUBREC_TO_NEWREC
	CHKr(rsksi_tlvDecodeSUBREC(rec, &iRd, &subrec));
	/* BLOCK_IV */
	COPY_SUBREC_TO_NEWREC
	CHKr(rsksi_tlvDecodeSUBREC(rec, &iRd, &subrec));
	/* LAST_HASH */
	COPY_SUBREC_TO_NEWREC
	CHKr(rsksi_tlvDecodeSUBREC(rec, &iRd, &subrec));
	/* REC_COUNT */
	COPY_SUBREC_TO_NEWREC
	CHKr(rsksi_tlvDecodeSUBREC(rec, &iRd, &subrec));
	/* actual sig! */
	newrec.data[iWr++] = 0x09 | RSKSI_FLAG_TLV16_RUNTIME;
	newrec.data[iWr++] = 0x06;
	newrec.data[iWr++] = (lenDer >> 8) & 0xff;
	newrec.data[iWr++] = lenDer & 0xff;
	/* now we know how large the new main record is */
	newrec.tlvlen = (uint16_t) iWr+lenDer;
	newrec.tlvtype = rec->tlvtype;
	newrec.hdr[0] = rec->hdr[0];
	newrec.hdr[1] = rec->hdr[1];
	newrec.hdr[2] = (newrec.tlvlen >> 8) & 0xff;
	newrec.hdr[3] = newrec.tlvlen & 0xff;
	newrec.lenHdr = 4;
	memcpy(newrec.data+iWr, der, lenDer);
	/* and finally copy back new record to existing one */
	memcpy(rec, &newrec, sizeof(newrec)-sizeof(newrec.data)+newrec.tlvlen+4);
	r = 0;
done:
	if(extended != NULL)
		KSI_Signature_free(extended);
	if (der != NULL)
		KSI_free(der);
	return r;
}

/* Verify the existance of the header. 
 */
int
verifyBLOCK_HDRKSI(FILE *sigfp, FILE *nsigfp)
{
	int r;
	tlvrecord_t rec;
	block_hdr_t *bh = NULL;
	if ((r = rsksi_tlvrd(sigfp, &rec, &bh)) != 0) goto done;
	if (rec.tlvtype != 0x0901) {
		r = RSGTE_MISS_BLOCKSIG;
		goto done;
	}
	if (nsigfp != NULL)
		if ((r = rsksi_tlvwrite(nsigfp, &rec)) != 0) goto done; 
done:	
	/*if (r == 0 || r == RSGTE_IO) */ {
		/* Only free memory if return is OK or error was RSGTE_IO was (happened in rsksi_tlvwrite) */
		if (bh != NULL)
			rsksi_objfree(rec.tlvtype, bh);
	}
	if(rsksi_read_debug)
		printf("debug: verifyBLOCK_HDRKSI returned %d\n", r);
	return r;
}

/* verify the root hash. This also means we need to compute the
 * Merkle tree root for the current block.
 */
int
verifyBLOCK_SIGKSI(block_sig_t *bs, ksifile ksi, FILE *sigfp, FILE *nsigfp,
                uint8_t bExtend, ksierrctx_t *ectx)
{
	int r;
	int ksistate;
	block_sig_t *file_bs = NULL;
	KSI_Signature *sig = NULL;
	KSI_DataHash *ksiHash = NULL;
	tlvrecord_t rec;

	if((r = verifySigblkFinish(ksi, &ksiHash)) != 0)
		goto done;
	if((r = rsksi_tlvrdVrfyBlockSig(sigfp, &file_bs, &rec)) != 0)
		goto done;
	if(ectx->recNum != bs->recCount) {
		r = RSGTE_INVLD_RECCNT;
		goto done;
	}

	/* Parse KSI Signature */
	ksistate = KSI_Signature_parse(ksi->ctx->ksi_ctx, file_bs->sig.der.data, file_bs->sig.der.len, &sig);
	if(ksistate != KSI_OK) {
		if(rsksi_read_debug)
			printf("debug: KSI_Signature_parse failed with error: %s (%d)\n", KSI_getErrorString(ksistate), ksistate); 
		r = RSGTE_INVLD_SIGNATURE;
		ectx->ksistate = ksistate;
		goto done;
	}

/* OLDCODE
	gtstate = GTTimestamp_DERDecode(file_bs->sig.der.data,
					file_bs->sig.der.len, &timestamp);
	if(gtstate != KSI_OK) {
		r = RSGTE_TS_DERDECODE;
		ectx->gtstate = gtstate;
		goto done;
	}
*/
	ksistate = KSI_Signature_verifyDataHash(sig, ksi->ctx->ksi_ctx, ksiHash);
	if (ksistate != KSI_OK) {
		if(rsksi_read_debug)
			printf("debug: KSI_Signature_verifyDataHash faile with error: %s (%d)\n", KSI_getErrorString(ksistate), ksistate); 
		r = RSGTE_INVLD_SIGNATURE;
		ectx->ksistate = ksistate;
		goto done;
		/* TODO proberly additional verify with KSI_Signature_verify*/
	}
/* OLD CODE
	gtstate = GTHTTP_verifyTimestampHash(timestamp, root, NULL,
			NULL, NULL, rsksi_read_puburl, 0, &vrfyInf);
	if(! (gtstate == KSI_OK
	      && vrfyInf->verification_errors == GT_NO_FAILURES) ) {
		r = RSGTE_INVLD_TIMESTAMP;
		ectx->gtstate = gtstate;
		goto done;
	}
*/
	if(rsksi_read_debug)
		printf("debug: verifyBLOCK_SIGKSI processed without error's\n"); 
	if(rsksi_read_showVerified)
		reportVerifySuccess(ectx); /*OLDCODE, vrfyInf);*/
	if(bExtend)
		if((r = rsksi_extendSig(sig, ksi, &rec, ectx)) != 0) goto done;
		
	if(nsigfp != NULL)
		if((r = rsksi_tlvwrite(nsigfp, &rec)) != 0) goto done;
	r = 0;
done:
	if(file_bs != NULL)
		rsksi_objfree(0x0904, file_bs);
	if(r != 0)
		reportError(r, ectx);
	if(ksiHash != NULL)
		KSI_DataHash_free(ksiHash);
	if(sig != NULL)
		KSI_Signature_free(sig);
	return r;
}

/* Helper function to enable debug */
void rsksi_set_debug(int iDebug)
{
	rsksi_read_debug = iDebug; 
}

/* Helper function to convert an old V10 signature file into V11 */
int rsksi_ConvertSigFile(char* name, FILE *oldsigfp, FILE *newsigfp, int verbose)
{
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
	while(1) { /* we will err out on EOF */
		rRead = rsksi_tlvRecRead(oldsigfp, &rec); 
		if(rRead == 0 /*|| rRead == RSGTE_EOF*/) {
			switch(rec.tlvtype) {
				case 0x0900:
				case 0x0901:
					/* Convert tlvrecord Header */
					if (rec.tlvtype == 0x0900) {
						typconv = ((0x00 /*flags*/ | 0x80 /* NEW RSKSI_FLAG_TLV16_RUNTIME*/) << 8) | 0x0902;
						rec.hdr[0] = typconv >> 8; 
						rec.hdr[1] = typconv & 0xff; 
					} else if (rec.tlvtype == 0x0901) {
						typconv = ((0x00 /*flags*/ | 0x80 /* NEW RSKSI_FLAG_TLV16_RUNTIME*/) << 8) | 0x0903;
						rec.hdr[0] = typconv >> 8; 
						rec.hdr[1] = typconv & 0xff; 
					}

					/* Debug verification output */
					r = rsksi_tlvDecodeIMPRINT(&rec, &imp);
					if(r != 0) goto donedecode;
					rsksi_printREC_HASH(stdout, imp, verbose);

					/* Output into new FILE */
					if((r = rsksi_tlvwrite(newsigfp, &rec)) != 0) goto done;

					/* Free mem*/
					free(imp->data);
					free(imp);
					imp = NULL; 
					break;
				case 0x0902:
					/* Split Data into HEADER and BLOCK */
					strtidx = 0;

					/* Create BH and BS*/
					if((bh = calloc(1, sizeof(block_hdr_t))) == NULL) {
						r = RSGTE_OOM;
						goto donedecode;
					}
					if((bs = calloc(1, sizeof(block_sig_t))) == NULL) {
						r = RSGTE_OOM;
						goto donedecode;
					}

					/* Check OLD encoded HASH ALGO */
					CHKrDecode(rsksi_tlvDecodeSUBREC(&rec, &strtidx, &subrec));
					if(!(subrec.tlvtype == 0x00 && subrec.tlvlen == 1)) {
						r = RSGTE_FMT;
						goto donedecode;
					}
					bh->hashID = subrec.data[0];

					/* Check OLD encoded BLOCK_IV */
					CHKrDecode(rsksi_tlvDecodeSUBREC(&rec, &strtidx, &subrec));
					if(!(subrec.tlvtype == 0x01)) {
						r = RSGTE_INVLTYP;
						goto donedecode;
					}
					if((bh->iv = (uint8_t*)malloc(subrec.tlvlen)) == NULL) {r=RSGTE_OOM;goto donedecode;}
					memcpy(bh->iv, subrec.data, subrec.tlvlen);

					/* Check OLD encoded LAST HASH */
					CHKrDecode(rsksi_tlvDecodeSUBREC(&rec, &strtidx, &subrec));
					if(!(subrec.tlvtype == 0x02)) { r = RSGTE_INVLTYP; goto donedecode; }
					bh->lastHash.hashID = subrec.data[0];
					if(subrec.tlvlen != 1 + hashOutputLengthOctetsKSI(bh->lastHash.hashID)) {
						r = RSGTE_LEN;
						goto donedecode;
					}
					bh->lastHash.len = subrec.tlvlen - 1;
					if((bh->lastHash.data = (uint8_t*)malloc(bh->lastHash.len)) == NULL) {r=RSGTE_OOM;goto donedecode;}
					memcpy(bh->lastHash.data, subrec.data+1, subrec.tlvlen-1);

					/* Debug verification output */
					rsksi_printBLOCK_HDR(stdout, bh, verbose);

					/* Check OLD encoded COUNT */
					CHKrDecode(rsksi_tlvDecodeSUBREC(&rec, &strtidx, &subrec));
					if(!(subrec.tlvtype == 0x03 && subrec.tlvlen <= 8)) { r = RSGTE_INVLTYP; goto donedecode; }
					bs->recCount = 0;
					for(i = 0 ; i < subrec.tlvlen ; ++i) {
						bs->recCount = (bs->recCount << 8) + subrec.data[i];
					}

					/* Check OLD encoded SIG */
					CHKrDecode(rsksi_tlvDecodeSUBREC(&rec, &strtidx, &subrec));
					if(!(subrec.tlvtype == 0x0905)) { r = RSGTE_INVLTYP; goto donedecode; }
					bs->sig.der.len = subrec.tlvlen;
					bs->sigID = SIGID_RFC3161;
					if((bs->sig.der.data = (uint8_t*)malloc(bs->sig.der.len)) == NULL) {r=RSGTE_OOM;goto donedecode;}
					memcpy(bs->sig.der.data, subrec.data, bs->sig.der.len);

					/* Debug output */
					rsksi_printBLOCK_SIG(stdout, bs, verbose);

					if(strtidx != rec.tlvlen) {
						r = RSGTE_LEN;
						goto donedecode;
					}

					/* Set back to NEW default */
					RSKSI_FLAG_TLV16_RUNTIME = 0x80; 

					/* Create Block Header */
					tlvlen  = 2 + 1 /* hash algo TLV */ +
						  2 + hashOutputLengthOctetsKSI(bh->hashID) /* iv */ +
						  2 + 1 + bh->lastHash.len /* last hash */;
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
					tlvlen  = 2 + tlvlenRecords /* rec-count */ +
						  4 + bs->sig.der.len /* rfc-3161 */;
					/* write top-level TLV object (block-sig */
					CHKrDecode(rsksi_tlv16Write(newsigfp, 0x00, 0x0904, tlvlen));
					/* and now write the children */
					/* rec-count */
					CHKrDecode(rsksi_tlv8Write(newsigfp, 0x00, 0x01, tlvlenRecords));
					CHKrDecode(rsksi_tlvfileAddInt64(newsigfp, bs->recCount));
					/* rfc-3161 */
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
					if(r != 0) goto done;
					break;
				default:
					fprintf(stdout, "unknown tlv record %4.4x\n", rec.tlvtype);
					break;
			}
		} else {
			/*if(feof(oldsigfp))
				break;
			else*/
			r = rRead; 
			if(r == RSGTE_EOF) 
				r = 0; /* Successfully finished file */
			else if(rsksi_read_debug)
				printf("debug: rsksi_ConvertSigFile failed to read with error %d\n", r);
			goto done;
		}

		/* Abort further processing if EOF */
		if (rRead == RSGTE_EOF)
			goto done;
	}
done:
	if(rsksi_read_debug)
		printf("debug: rsksi_ConvertSigFile returned %d\n", r);
	return r;
}
