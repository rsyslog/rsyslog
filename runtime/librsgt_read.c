/* librsgt_read.c - rsyslog's guardtime support library
 * This includes functions used for reading signature (and 
 * other related) files. Well, actually it also contains
 * some writing functionality, but only as far as rsyslog
 * itself is not concerned, but "just" the utility programs.
 *
 * This part of the library uses C stdio and expects that the
 * caller will open and close the file to be read itself.
 *
 * Copyright 2013 Adiscon GmbH.
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
#include <gt_http.h>

#include "librsgt_common.h"
#include "librsgt.h"

typedef unsigned char uchar;
#ifndef VERSION
#define VERSION "no-version"
#endif
#define MAXFNAME 1024

static int rsgt_read_debug = 0;
char *rsgt_read_puburl = "http://verify.guardtime.com/gt-controlpublications.bin";
char *rsgt_extend_puburl = "http://verifier.guardtime.net/gt-extendingservice";
char *rsgt_userid = ""; 
char *rsgt_userkey = ""; 
uint8_t rsgt_read_showVerified = 0;

/* macro to obtain next char from file including error tracking */
#define NEXTC	if((c = fgetc(fp)) == EOF) { \
			r = feof(fp) ? RSGTE_EOF : RSGTE_IO; \
			goto done; \
		}

/* if verbose==0, only the first and last two octets are shown,
 * otherwise everything.
 */
static void
outputHexBlob(FILE *fp, uint8_t *blob, uint16_t len, uint8_t verbose)
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
outputHash(FILE *fp, char *hdr, uint8_t *data, uint16_t len, uint8_t verbose)
{
	fprintf(fp, "%s", hdr);
	outputHexBlob(fp, data, len, verbose);
	fputc('\n', fp);
}

void
rsgt_errctxInit(gterrctx_t *ectx)
{
	ectx->fp = NULL;
	ectx->filename = NULL;
	ectx->recNum = 0;
	ectx->gtstate = 0;
	ectx->recNumInFile = 0;
	ectx->blkNum = 0;
	ectx->verbose = 0;
	ectx->errRec = NULL;
	ectx->frstRecInBlk = NULL;
	ectx->fileHash = NULL;
	ectx->lefthash = ectx->righthash = ectx->computedHash = NULL;
}
void
rsgt_errctxExit(gterrctx_t *ectx)
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
rsgt_errctxSetErrRec(gterrctx_t *ectx, char *rec)
{
	ectx->errRec = strdup(rec);
}
/* This stores the block's first record. Here we copy the data,
 * as the caller will usually not preserve it long enough.
 */
void
rsgt_errctxFrstRecInBlk(gterrctx_t *ectx, char *rec)
{
	free(ectx->frstRecInBlk);
	ectx->frstRecInBlk = strdup(rec);
}

static void
reportError(int errcode, gterrctx_t *ectx)
{
	if(ectx->fp != NULL) {
		fprintf(ectx->fp, "%s[%llu:%llu:%llu]: error[%u]: %s\n",
			ectx->filename,
			(long long unsigned) ectx->blkNum, (long long unsigned) ectx->recNum,
			(long long unsigned) ectx->recNumInFile,
			errcode, RSGTE2String(errcode));
		if(ectx->frstRecInBlk != NULL)
			fprintf(ectx->fp, "\tBlock Start Record.: '%s'\n", ectx->frstRecInBlk);
		if(ectx->errRec != NULL)
			fprintf(ectx->fp, "\tRecord in Question.: '%s'\n", ectx->errRec);
		if(ectx->computedHash != NULL) {
			outputHash(ectx->fp, "\tComputed Hash......: ", ectx->computedHash->digest,
				ectx->computedHash->digest_length, ectx->verbose);
		}
		if(ectx->fileHash != NULL) {
			outputHash(ectx->fp, "\tSignature File Hash: ", ectx->fileHash->data,
				ectx->fileHash->len, ectx->verbose);
		}
		if(errcode == RSGTE_INVLD_TREE_HASH ||
		   errcode == RSGTE_INVLD_TREE_HASHID) {
			fprintf(ectx->fp, "\tTree Level.........: %d\n", (int) ectx->treeLevel);
			outputHash(ectx->fp, "\tTree Left Hash.....: ", ectx->lefthash->digest,
				ectx->lefthash->digest_length, ectx->verbose);
			outputHash(ectx->fp, "\tTree Right Hash....: ", ectx->righthash->digest,
				ectx->righthash->digest_length, ectx->verbose);
		}
		if(errcode == RSGTE_INVLD_TIMESTAMP ||
		   errcode == RSGTE_TS_DERDECODE) {
			fprintf(ectx->fp, "\tPublication Server.: %s\n", rsgt_read_puburl);
			fprintf(ectx->fp, "\tGT Verify Timestamp: [%u]%s\n",
				ectx->gtstate, GTHTTP_getErrorString(ectx->gtstate));
		}
		if(errcode == RSGTE_TS_EXTEND ||
		   errcode == RSGTE_TS_DERDECODE) {
			fprintf(ectx->fp, "\tExtending Server...: %s\n", rsgt_extend_puburl);
			fprintf(ectx->fp, "\tGT Extend Timestamp: [%u]%s\n",
				ectx->gtstate, GTHTTP_getErrorString(ectx->gtstate));
		}
		if(errcode == RSGTE_TS_DERENCODE) {
			fprintf(ectx->fp, "\tAPI return state...: [%u]%s\n",
				ectx->gtstate, GTHTTP_getErrorString(ectx->gtstate));
		}
	}
}

/* obviously, this is not an error-reporting function. We still use
 * ectx, as it has most information we need.
 */
static void
reportVerifySuccess(gterrctx_t *ectx, GTVerificationInfo *vrfyInf)
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
		fprintf(ectx->fp, "\tGT Verify Timestamp: [%u]%s\n",
			ectx->gtstate, GTHTTP_getErrorString(ectx->gtstate));
		GTVerificationInfo_print(ectx->fp, 0, vrfyInf);
	}
}

/* return the actual length in to-be-written octets of an integer */
static inline uint8_t rsgt_tlvGetInt64OctetSize(uint64_t val)
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

static inline int rsgt_tlvfileAddOctet(FILE *newsigfp, int8_t octet)
{
	/* Directory write into file */
	int r = 0;
	if ( fputc(octet, newsigfp) == EOF ) 
		r = RSGTE_IO; 
	return r;
}
static inline int rsgt_tlvfileAddOctetString(FILE *newsigfp, uint8_t *octet, int size)
{
	int i, r = 0;
	for(i = 0 ; i < size ; ++i) {
		r = rsgt_tlvfileAddOctet(newsigfp, octet[i]);
		if(r != 0) goto done;
	}
done:	return r;
}
static inline int rsgt_tlvfileAddInt64(FILE *newsigfp, uint64_t val)
{
	uint8_t doWrite = 0;
	int r;
	if(val >> 56) {
		r = rsgt_tlvfileAddOctet(newsigfp, (val >> 56) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 48) & 0xff)) {
		r = rsgt_tlvfileAddOctet(newsigfp, (val >> 48) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 40) & 0xff)) {
		r = rsgt_tlvfileAddOctet(newsigfp, (val >> 40) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 32) & 0xff)) {
		r = rsgt_tlvfileAddOctet(newsigfp, (val >> 32) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 24) & 0xff)) {
		r = rsgt_tlvfileAddOctet(newsigfp, (val >> 24) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 16) & 0xff)) {
		r = rsgt_tlvfileAddOctet(newsigfp, (val >> 16) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 8) & 0xff)) {
		r = rsgt_tlvfileAddOctet(newsigfp, (val >>  8) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	r = rsgt_tlvfileAddOctet(newsigfp, val & 0xff);
done:	return r;
}

static int
rsgt_tlv8Write(FILE *newsigfp, int flags, int tlvtype, int len)
{
	int r;
	assert((flags & RSGT_TYPE_MASK) == 0);
	assert((tlvtype & RSGT_TYPE_MASK) == tlvtype);
	r = rsgt_tlvfileAddOctet(newsigfp, (flags & ~RSGT_FLAG_TLV16_RUNTIME) | tlvtype);
	if(r != 0) goto done;
	r = rsgt_tlvfileAddOctet(newsigfp, len & 0xff);
done:	return r;
} 

static int
rsgt_tlv16Write(FILE *newsigfp, int flags, int tlvtype, uint16_t len)
{
	uint16_t typ;
	int r;
	assert((flags & RSGT_TYPE_MASK) == 0);
	assert((tlvtype >> 8 & RSGT_TYPE_MASK) == (tlvtype >> 8));
	typ = ((flags | RSGT_FLAG_TLV16_RUNTIME) << 8) | tlvtype;
	r = rsgt_tlvfileAddOctet(newsigfp, typ >> 8);
	if(r != 0) goto done;
	r = rsgt_tlvfileAddOctet(newsigfp, typ & 0xff);
	if(r != 0) goto done;
	r = rsgt_tlvfileAddOctet(newsigfp, (len >> 8) & 0xff);
	if(r != 0) goto done;
	r = rsgt_tlvfileAddOctet(newsigfp, len & 0xff);
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
rsgt_tlvwrite(FILE *fp, tlvrecord_t *rec)
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
rsgt_tlvrdHeader(FILE *fp, uchar *hdr)
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
rsgt_tlvRecRead(FILE *fp, tlvrecord_t *rec)
{
	int r = 1;
	int c;

	NEXTC;
	rec->hdr[0] = c;
	rec->tlvtype = c & 0x1f;
	if(c & RSGT_FLAG_TLV16_RUNTIME) { /* tlv16? */
		if(rsgt_read_debug)
			printf("debug: TL168 %d\n", c); 
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
		if(rsgt_read_debug)
			printf("debug: TLV8 %d\n", c); 
		NEXTC;
		rec->lenHdr = 2;
		rec->hdr[1] = c;
		rec->tlvlen = c;
	}
	if(fread(rec->data, (size_t) rec->tlvlen, 1, fp) != 1) {
		if(rsgt_read_debug)
			printf("debug: rec->tlvlen %d\n", rec->tlvlen); 
		r = feof(fp) ? RSGTE_EOF : RSGTE_IO;
		goto done;
	}

	r = 0;
done:	
	if(r == 0 && rsgt_read_debug)
		/* Only show debug if no fail */
		printf("debug: rsgt_tlvRecRead tlvtype %4.4x, len %u, r = %d\n", (unsigned) rec->tlvtype,
			(unsigned) rec->tlvlen, r);
	return r;
}

/* decode a sub-tlv record from an existing record's memory buffer
 */
static int
rsgt_tlvDecodeSUBREC(tlvrecord_t *rec, uint16_t *stridx, tlvrecord_t *newrec)
{
	int r = 1;
	int c;

	if(rec->tlvlen == *stridx) {r=RSGTE_LEN; goto done;}
	c = rec->data[(*stridx)++];
	newrec->hdr[0] = c;
	newrec->tlvtype = c & 0x1f;
	if(c & RSGT_FLAG_TLV16_RUNTIME) { /* tlv16? */
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
	if(rsgt_read_debug)
		printf("debug: rsgt_tlvDecodeSUBREC stridx=%d rec->tlvlen=%d newrec->tlvlen=%d\n", *stridx, rec->tlvlen, newrec->tlvlen);
	if(rec->tlvlen < *stridx + newrec->tlvlen) {r=RSGTE_LEN; goto done;}
	memcpy(newrec->data, (rec->data)+(*stridx), newrec->tlvlen);
	*stridx += newrec->tlvlen;

	if(rsgt_read_debug)
		printf("read sub-tlv: tlvtype %4.4x, len %u\n",
			(unsigned) newrec->tlvtype,
			(unsigned) newrec->tlvlen);
	r = 0;
done:	
	if(r != 0 && rsgt_read_debug)
		printf("debug: rsgt_tlvDecodeSUBREC failed with error %d, tlvtype %4.4x\n", r, (unsigned) rec->tlvtype);
	return r;
}


static int
rsgt_tlvDecodeIMPRINT(tlvrecord_t *rec, imprint_t **imprint)
{
	int r = 1;
	imprint_t *imp = NULL;

	if((imp = calloc(1, sizeof(imprint_t))) == NULL) {
		r = RSGTE_OOM;
		goto done;
	}

	imp->hashID = rec->data[0];
	if(rec->tlvlen != 1 + hashOutputLengthOctets(imp->hashID)) {
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
		if(rsgt_read_debug)
			printf("debug: read tlvDecodeIMPRINT returned %d TLVLen=%d, HashID=%d\n", r, rec->tlvlen, imp->hashID);
	} else { 
		/* Free memory on FAIL!*/
		if (imp != NULL)
			rsgt_objfree(rec->tlvtype, imp);
	}
	return r;
}

static int
rsgt_tlvDecodeHASH_ALGO(tlvrecord_t *rec, uint16_t *strtidx, uint8_t *hashAlg)
{
	int r = 1;
	tlvrecord_t subrec;

	CHKr(rsgt_tlvDecodeSUBREC(rec, strtidx, &subrec));
	if(!(subrec.tlvtype == 0x01 && subrec.tlvlen == 1)) {
		r = RSGTE_FMT;
		goto done;
	}
	*hashAlg = subrec.data[0];
	r = 0;
done:	
	if(r != 0 && rsgt_read_debug)
		printf("debug: rsgt_tlvDecodeHASH_ALGO failed with error %d, rec->tlvtype %4.4x, subrec->tlvtype %4.4x\n", r, (unsigned) rec->tlvtype, (unsigned) subrec.tlvtype);
	return r;
}
static int
rsgt_tlvDecodeBLOCK_IV(tlvrecord_t *rec, uint16_t *strtidx, uint8_t **iv)
{
	int r = 1;
	tlvrecord_t subrec;

	CHKr(rsgt_tlvDecodeSUBREC(rec, strtidx, &subrec));
	if(!(subrec.tlvtype == 0x02)) {
		r = RSGTE_INVLTYP;
		goto done;
	}
	if((*iv = (uint8_t*)malloc(subrec.tlvlen)) == NULL) {r=RSGTE_OOM;goto done;}
	memcpy(*iv, subrec.data, subrec.tlvlen);
	r = 0;
done:	
	if(r != 0 && rsgt_read_debug)
		printf("debug: rsgt_tlvDecodeBLOCK_IV failed with error %d, tlvtype %4.4x\n", r, (unsigned) rec->tlvtype);
	return r;
}
static int
rsgt_tlvDecodeLAST_HASH(tlvrecord_t *rec, uint16_t *strtidx, imprint_t *imp)
{
	int r = 1;
	tlvrecord_t subrec;

	CHKr(rsgt_tlvDecodeSUBREC(rec, strtidx, &subrec));
	if(!(subrec.tlvtype == 0x03)) { r = RSGTE_INVLTYP; goto done; }
	imp->hashID = subrec.data[0];
	if(subrec.tlvlen != 1 + hashOutputLengthOctets(imp->hashID)) {
		r = RSGTE_LEN;
		goto done;
	}
	imp->len = subrec.tlvlen - 1;
	if((imp->data = (uint8_t*)malloc(imp->len)) == NULL) {r=RSGTE_OOM;goto done;}
	memcpy(imp->data, subrec.data+1, subrec.tlvlen-1);
	r = 0;
done:	
	if(r != 0 && rsgt_read_debug)
		printf("debug: rsgt_tlvDecodeLAST_HASH failed with error %d, tlvtype %4.4x\n", r, (unsigned) rec->tlvtype);
	return r;
}
static int
rsgt_tlvDecodeREC_COUNT(tlvrecord_t *rec, uint16_t *strtidx, uint64_t *cnt)
{
	int r = 1;
	int i;
	uint64_t val;
	tlvrecord_t subrec;

	CHKr(rsgt_tlvDecodeSUBREC(rec, strtidx, &subrec));
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
rsgt_tlvDecodeSIG(tlvrecord_t *rec, uint16_t *strtidx, block_sig_t *bs)
{
	int r = 1;
	tlvrecord_t subrec;

	CHKr(rsgt_tlvDecodeSUBREC(rec, strtidx, &subrec));
	if(!(subrec.tlvtype == 0x0906)) { r = RSGTE_INVLTYP; goto done; }
	bs->sig.der.len = subrec.tlvlen;
	bs->sigID = SIGID_RFC3161;
	if((bs->sig.der.data = (uint8_t*)malloc(bs->sig.der.len)) == NULL) {r=RSGTE_OOM;goto done;}
	memcpy(bs->sig.der.data, subrec.data, bs->sig.der.len);
	r = 0;
done:	
	if(rsgt_read_debug)
		printf("debug: rsgt_tlvDecodeSIG returned %d, tlvtype %4.4x\n", r, (unsigned) rec->tlvtype);
	return r;
}

static int
rsgt_tlvDecodeBLOCK_HDR(tlvrecord_t *rec, block_hdr_t **blockhdr)
{
	int r = 1;
	uint16_t strtidx = 0;
	block_hdr_t *bh = NULL;
	if((bh = calloc(1, sizeof(block_hdr_t))) == NULL) {
		r = RSGTE_OOM;
		goto done;
	}
	CHKr(rsgt_tlvDecodeHASH_ALGO(rec, &strtidx, &(bh->hashID)));
	CHKr(rsgt_tlvDecodeBLOCK_IV(rec, &strtidx, &(bh->iv)));
	CHKr(rsgt_tlvDecodeLAST_HASH(rec, &strtidx, &(bh->lastHash)));
	if(strtidx != rec->tlvlen) {
		r = RSGTE_LEN;
		goto done;
	}
	*blockhdr = bh;
	r = 0;
done:	
	if (r == 0) {
		if(rsgt_read_debug)
			printf("debug: tlvDecodeBLOCK_HDR returned %d, tlvtype %4.4x\n", r, (unsigned) rec->tlvtype);
	} else { 
		/* Free memory on FAIL!*/
		if (bh != NULL)
			rsgt_objfree(rec->tlvtype, bh);
	}	
	return r;
}

static int
rsgt_tlvDecodeBLOCK_SIG(tlvrecord_t *rec, block_sig_t **blocksig)
{
	int r = 1;
	uint16_t strtidx = 0;
	block_sig_t *bs = NULL;
	if((bs = calloc(1, sizeof(block_sig_t))) == NULL) {
		r = RSGTE_OOM;
		goto done;
	}
	CHKr(rsgt_tlvDecodeREC_COUNT(rec, &strtidx, &(bs->recCount)));
	CHKr(rsgt_tlvDecodeSIG(rec, &strtidx, bs));
	if(strtidx != rec->tlvlen) {
		r = RSGTE_LEN;
		goto done;
	}
	*blocksig = bs;
	r = 0;
done:	
	if(r == 0) {
		if (rsgt_read_debug)
			printf("debug: rsgt_tlvDecodeBLOCK_SIG returned %d, tlvtype %4.4x\n", r, (unsigned) rec->tlvtype);
	} else { 
		/* Free memory on FAIL!*/
		if (bs != NULL)
			rsgt_objfree(rec->tlvtype, bs);
	}	
	return r;
}
static int
rsgt_tlvRecDecode(tlvrecord_t *rec, void *obj)
{
	int r = 1;
	switch(rec->tlvtype) {
		case 0x0901:
			r = rsgt_tlvDecodeBLOCK_HDR(rec, obj);
			if(r != 0) goto done;
			break;
		case 0x0902:
		case 0x0903:
			r = rsgt_tlvDecodeIMPRINT(rec, obj);
			if(r != 0) goto done;
			break;
		case 0x0904:
			r = rsgt_tlvDecodeBLOCK_SIG(rec, obj);
			if(r != 0) goto done;
			break;
	}
done:
	if(r == 0 && rsgt_read_debug)
		printf("debug: tlvRecDecode returned %d, tlvtype %4.4x\n", r, (unsigned) rec->tlvtype);
	return r;
}

static int
rsgt_tlvrdRecHash(FILE *fp, FILE *outfp, imprint_t **imp)
{
	int r;
	tlvrecord_t rec;

	if((r = rsgt_tlvrd(fp, &rec, imp)) != 0) goto done;
	if(rec.tlvtype != 0x0902) {
		r = RSGTE_MISS_REC_HASH;
		rsgt_objfree(rec.tlvtype, *imp);
		*imp = NULL; 
		goto done;
	}
	if(outfp != NULL)
		if((r = rsgt_tlvwrite(outfp, &rec)) != 0) goto done;
	r = 0;
done:	
	if(r == 0 && rsgt_read_debug)
		printf("debug: tlvrdRecHash returned %d, rec->tlvtype %4.4x\n", r, (unsigned) rec.tlvtype);
	return r;
}

static int
rsgt_tlvrdTreeHash(FILE *fp, FILE *outfp, imprint_t **imp)
{
	int r;
	tlvrecord_t rec;

	if((r = rsgt_tlvrd(fp, &rec, imp)) != 0) goto done;
	if(rec.tlvtype != 0x0903) {
		r = RSGTE_MISS_TREE_HASH;
		rsgt_objfree(rec.tlvtype, *imp);
		*imp = NULL; 
		goto done;
	}
	if(outfp != NULL)
		if((r = rsgt_tlvwrite(outfp, &rec)) != 0) goto done;
	r = 0;
done:	
	if(r == 0 && rsgt_read_debug)
		printf("debug: tlvrdTreeHash returned %d, rec->tlvtype %4.4x\n", r, (unsigned) rec.tlvtype);
	return r;
}

/* read BLOCK_SIG during verification phase */
static int
rsgt_tlvrdVrfyBlockSig(FILE *fp, block_sig_t **bs, tlvrecord_t *rec)
{
	int r;

	if((r = rsgt_tlvrd(fp, rec, bs)) != 0) goto done;
	if(rec->tlvtype != 0x0904) {
		r = RSGTE_MISS_BLOCKSIG;
		rsgt_objfree(rec->tlvtype, *bs);
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
rsgt_tlvrd(FILE *fp, tlvrecord_t *rec, void *obj)
{
	int r;
	if((r = rsgt_tlvRecRead(fp, rec)) != 0) goto done;
	r = rsgt_tlvRecDecode(rec, obj);
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
rsgt_printIMPRINT(FILE *fp, char *name, imprint_t *imp, uint8_t verbose)
{
	fprintf(fp, "%s", name);
		outputHexBlob(fp, imp->data, imp->len, verbose);
		fputc('\n', fp);
}

static void
rsgt_printREC_HASH(FILE *fp, imprint_t *imp, uint8_t verbose)
{
	rsgt_printIMPRINT(fp, "[0x0902]Record hash: ",
		imp, verbose);
}

static void
rsgt_printINT_HASH(FILE *fp, imprint_t *imp, uint8_t verbose)
{
	rsgt_printIMPRINT(fp, "[0x0903]Tree hash..: ",
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
rsgt_printBLOCK_HDR(FILE *fp, block_hdr_t *bh, uint8_t verbose)
 {
	fprintf(fp, "[0x0901]Block Header Record:\n");
 	fprintf(fp, "\tPrevious Block Hash:\n");
	fprintf(fp, "\t   Algorithm..: %s\n", hashAlgName(bh->lastHash.hashID));
 	fprintf(fp, "\t   Hash.......: ");
		outputHexBlob(fp, bh->lastHash.data, bh->lastHash.len, verbose);
 		fputc('\n', fp);
	if(blobIsZero(bh->lastHash.data, bh->lastHash.len))
 		fprintf(fp, "\t   NOTE: New Hash Chain Start!\n");
	fprintf(fp, "\tHash Algorithm: %s\n", hashAlgName(bh->hashID));
 	fprintf(fp, "\tIV............: ");
		outputHexBlob(fp, bh->iv, getIVLen(bh), verbose);
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
rsgt_printBLOCK_SIG(FILE *fp, block_sig_t *bs, uint8_t verbose)
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
rsgt_tlvprint(FILE *fp, uint16_t tlvtype, void *obj, uint8_t verbose)
{
	switch(tlvtype) {
	case 0x0901:
		rsgt_printBLOCK_HDR(fp, obj, verbose);
		break;
	case 0x0902:
		rsgt_printREC_HASH(fp, obj, verbose);
		break;
	case 0x0903:
		rsgt_printINT_HASH(fp, obj, verbose);
		break;
	case 0x0904:
		rsgt_printBLOCK_SIG(fp, obj, verbose);
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
rsgt_objfree(uint16_t tlvtype, void *obj)
{
	// check if obj is valid 
	if (obj == NULL )
		return; 

	switch(tlvtype) {
	case 0x0901:
		if ( ((block_hdr_t*)obj)->iv != NULL)
			free(((block_hdr_t*)obj)->iv);
		if ( ((block_hdr_t*)obj)->lastHash.data != NULL)
			free(((block_hdr_t*)obj)->lastHash.data);
		break;
	case 0x0902:
	case 0x0903:
		free(((imprint_t*)obj)->data);
		break;
	case 0x0904:
		if ( ((block_sig_t*)obj)->sig.der.data != NULL)
			free(((block_sig_t*)obj)->sig.der.data);
		break;
	default:fprintf(stderr, "rsgt_objfree: unknown tlv record %4.4x\n",
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
rsgt_getBlockParams(FILE *fp, uint8_t bRewind, block_sig_t **bs, block_hdr_t **bh,
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
		if((r = rsgt_tlvrd(fp, &rec, &obj)) != 0) goto done;
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
			rsgt_objfree(rec.tlvtype, obj);
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
	if(rsgt_read_debug)
		printf("debug: rsgt_getBlockParams returned %d, rec->tlvtype %4.4x\n", r, (unsigned) rec.tlvtype);
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
rsgt_chkFileHdr(FILE *fp, char *expect)
{
	int r;
	char hdr[9];

	if((r = rsgt_tlvrdHeader(fp, (uchar*)hdr)) != 0) goto done;
	if (rsgt_read_debug)
		printf("debug: rsgt_chkFileHdr header returned %s\n", hdr);
	if(strcmp(hdr, expect))
		r = RSGTE_INVLHDR;
	else
		r = 0;
done:
	return r;
}

gtfile
rsgt_vrfyConstruct_gf(void)
{
	gtfile gf;
	if((gf = calloc(1, sizeof(struct gtfile_s))) == NULL)
		goto done;
	gf->x_prev = NULL;

done:	return gf;
}

void
rsgt_vrfyBlkInit(gtfile gf, block_hdr_t *bh, uint8_t bHasRecHashes, uint8_t bHasIntermedHashes)
{
	gf->hashAlg = hashID2Alg(bh->hashID);
	gf->bKeepRecordHashes = bHasRecHashes;
	gf->bKeepTreeHashes = bHasIntermedHashes;
	free(gf->IV);
	gf->IV = malloc(getIVLen(bh));
	memcpy(gf->IV, bh->iv, getIVLen(bh));
	gf->x_prev = malloc(sizeof(imprint_t));
	gf->x_prev->len=bh->lastHash.len;
	gf->x_prev->hashID = bh->lastHash.hashID;
	gf->x_prev->data = malloc(gf->x_prev->len);
	memcpy(gf->x_prev->data, bh->lastHash.data, gf->x_prev->len);
}

static int
rsgt_vrfy_chkRecHash(gtfile gf, FILE *sigfp, FILE *nsigfp, 
		     GTDataHash *recHash, gterrctx_t *ectx)
{
	int r = 0;
	imprint_t *imp = NULL;

	if((r = rsgt_tlvrdRecHash(sigfp, nsigfp, &imp)) != 0)
		reportError(r, ectx);
		goto done;
	if(imp->hashID != hashIdentifier(gf->hashAlg)) {
		reportError(r, ectx);
		r = RSGTE_INVLD_REC_HASHID;
		goto done;
	}
	if(memcmp(imp->data, recHash->digest,
		  hashOutputLengthOctets(imp->hashID))) {
		r = RSGTE_INVLD_REC_HASH;
		ectx->computedHash = recHash;
		ectx->fileHash = imp;
		reportError(r, ectx);
		ectx->computedHash = NULL, ectx->fileHash = NULL;
		goto done;
	}
	r = 0;
done:
	if(imp != NULL)
		rsgt_objfree(0x0902, imp);
	return r;
}

static int
rsgt_vrfy_chkTreeHash(gtfile gf, FILE *sigfp, FILE *nsigfp,
                      GTDataHash *hash, gterrctx_t *ectx)
{
	int r = 0;
	imprint_t *imp = NULL;

	if((r = rsgt_tlvrdTreeHash(sigfp, nsigfp, &imp)) != 0) {
		reportError(r, ectx);
		goto done;
	}
	if(imp->hashID != hashIdentifier(gf->hashAlg)) {
		reportError(r, ectx);
		r = RSGTE_INVLD_TREE_HASHID;
		goto done;
	}
	if(memcmp(imp->data, hash->digest,
		  hashOutputLengthOctets(imp->hashID))) {
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
		if(rsgt_read_debug)
			printf("debug: rsgt_vrfy_chkTreeHash returned %d, hashID=%d, Length=%d\n", r, imp->hashID, hashOutputLengthOctets(imp->hashID));
		/* Free memory */
		rsgt_objfree(0x0903, imp);
	}
	return r;
}

int
rsgt_vrfy_nextRec(gtfile gf, FILE *sigfp, FILE *nsigfp,
	          unsigned char *rec, size_t len, gterrctx_t *ectx)
{
	int r = 0;
	GTDataHash *x; /* current hash */
	GTDataHash *m, *recHash = NULL, *t, *t_del;
	uint8_t j;

	hash_m(gf, &m);
	hash_r(gf, &recHash, rec, len);
	if(gf->bKeepRecordHashes) {
		r = rsgt_vrfy_chkRecHash(gf, sigfp, nsigfp, recHash, ectx);
		if(r != 0) goto done;
	}
	hash_node(gf, &x, m, recHash, 1); /* hash leaf */
	if(gf->bKeepTreeHashes) {
		ectx->treeLevel = 0;
		ectx->lefthash = m;
		ectx->righthash = recHash;
		r = rsgt_vrfy_chkTreeHash(gf, sigfp, nsigfp, x, ectx);
		if(r != 0) goto done;
	}
	rsgtimprintDel(gf->x_prev);
	gf->x_prev = rsgtImprintFromGTDataHash(x);
	/* add x to the forest as new leaf, update roots list */
	t = x;
	for(j = 0 ; j < gf->nRoots ; ++j) {
		if(gf->roots_valid[j] == 0) {
			gf->roots_hash[j] = t;
			gf->roots_valid[j] = 1;
			t = NULL;
			break;
		} else if(t != NULL) {
			/* hash interim node */
			ectx->treeLevel = j+1;
			ectx->righthash = t;
			t_del = t;
			hash_node(gf, &t, gf->roots_hash[j], t_del, j+2);
			gf->roots_valid[j] = 0;
			if(gf->bKeepTreeHashes) {
				ectx->lefthash = gf->roots_hash[j];
				r = rsgt_vrfy_chkTreeHash(gf, sigfp, nsigfp, t, ectx);
				if(r != 0) goto done; /* mem leak ok, we terminate! */
			}
			GTDataHash_free(gf->roots_hash[j]);
			GTDataHash_free(t_del);
		}
	}
	if(t != NULL) {
		/* new level, append "at the top" */
		gf->roots_hash[gf->nRoots] = t;
		gf->roots_valid[gf->nRoots] = 1;
		++gf->nRoots;
		assert(gf->nRoots < MAX_ROOTS);
		t = NULL;
	}
	++gf->nRecords;

	/* cleanup */
	GTDataHash_free(m);
done:
	if(recHash != NULL)
		GTDataHash_free(recHash);
	return r;
}


/* TODO: think about merging this with the writer. The
 * same applies to the other computation algos.
 */
static int
verifySigblkFinish(gtfile gf, GTDataHash **pRoot)
{
	GTDataHash *root, *rootDel;
	int8_t j;
	int r = 0;

	if(gf->nRecords == 0)
		goto done;

	root = NULL;
	for(j = 0 ; j < gf->nRoots ; ++j) {
		if(root == NULL) {
			root = gf->roots_valid[j] ? gf->roots_hash[j] : NULL;
			gf->roots_valid[j] = 0; /* guess this is redundant with init, maybe del */
		} else if(gf->roots_valid[j]) {
			rootDel = root;
			hash_node(gf, &root, gf->roots_hash[j], root, j+2);
			gf->roots_valid[j] = 0; /* guess this is redundant with init, maybe del */
			GTDataHash_free(rootDel);
		}
	}

	*pRoot = root;
	r = 0;
done:
	gf->bInBlk = 0;
	return r;
}


/* helper for rsgt_extendSig: */
#define COPY_SUBREC_TO_NEWREC \
	memcpy(newrec.data+iWr, subrec.hdr, subrec.lenHdr); \
	iWr += subrec.lenHdr; \
	memcpy(newrec.data+iWr, subrec.data, subrec.tlvlen); \
	iWr += subrec.tlvlen;

static inline int
rsgt_extendSig(GTTimestamp *timestamp, tlvrecord_t *rec, gterrctx_t *ectx)
{
	GTTimestamp *out_timestamp;
	uint8_t *der;
	size_t lenDer;
	int r, rgt;
	tlvrecord_t newrec, subrec;
	uint16_t iRd, iWr;

	rgt = GTHTTP_extendTimestamp(timestamp, rsgt_extend_puburl, &out_timestamp);
	if(rgt != GT_OK) {
		ectx->gtstate = rgt;
		r = RSGTE_TS_EXTEND;
		goto done;
	}
	r = GTTimestamp_getDEREncoded(out_timestamp, &der, &lenDer);
	if(r != GT_OK) {
		r = RSGTE_TS_DERENCODE;
		ectx->gtstate = rgt;
		goto done;
	}
	/* update block_sig tlv record with new extended timestamp */
	/* we now need to copy all tlv records before the actual der
	 * encoded part.
	 */
	iRd = iWr = 0;
	// TODO; check tlvtypes at comment places below!
	CHKr(rsgt_tlvDecodeSUBREC(rec, &iRd, &subrec));
	/* HASH_ALGO */
	COPY_SUBREC_TO_NEWREC
	CHKr(rsgt_tlvDecodeSUBREC(rec, &iRd, &subrec));
	/* BLOCK_IV */
	COPY_SUBREC_TO_NEWREC
	CHKr(rsgt_tlvDecodeSUBREC(rec, &iRd, &subrec));
	/* LAST_HASH */
	COPY_SUBREC_TO_NEWREC
	CHKr(rsgt_tlvDecodeSUBREC(rec, &iRd, &subrec));
	/* REC_COUNT */
	COPY_SUBREC_TO_NEWREC
	CHKr(rsgt_tlvDecodeSUBREC(rec, &iRd, &subrec));
	/* actual sig! */
	newrec.data[iWr++] = 0x09 | RSGT_FLAG_TLV16_RUNTIME;
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
	return r;
}

/* Verify the existance of the header. 
 */
int
verifyBLOCK_HDR(FILE *sigfp, FILE *nsigfp)
{
	int r;
	tlvrecord_t rec;
	block_hdr_t *bh = NULL;
	if ((r = rsgt_tlvrd(sigfp, &rec, &bh)) != 0) goto done;
	if (rec.tlvtype != 0x0901) {
		r = RSGTE_MISS_BLOCKSIG;
		goto done;
	}
	if (nsigfp != NULL)
		if ((r = rsgt_tlvwrite(nsigfp, &rec)) != 0) goto done; 
done:	
/*	if (r == 0 || r == RSGTE_IO)*/ {
		/* Only free memory if return is OK or error was RSGTE_IO was (happened in rsksi_tlvwrite) */
		if (bh != NULL)
			rsgt_objfree(rec.tlvtype, bh);
	}
	if(rsgt_read_debug)
		printf("debug: verifyBLOCK_HDR returned %d\n", r);
	return r;
}

/* verify the root hash. This also means we need to compute the
 * Merkle tree root for the current block.
 */
int
verifyBLOCK_SIG(block_sig_t *bs, gtfile gf, FILE *sigfp, FILE *nsigfp,
                uint8_t bExtend, gterrctx_t *ectx)
{
	int r;
	int gtstate;
	block_sig_t *file_bs = NULL;
	GTTimestamp *timestamp = NULL;
	GTVerificationInfo *vrfyInf;
	GTDataHash *root = NULL;
	tlvrecord_t rec;
	
	if((r = verifySigblkFinish(gf, &root)) != 0)
		goto done;
	if((r = rsgt_tlvrdVrfyBlockSig(sigfp, &file_bs, &rec)) != 0)
		goto done;
	if(ectx->recNum != bs->recCount) {
		r = RSGTE_INVLD_RECCNT;
		goto done;
	}

	gtstate = GTTimestamp_DERDecode(file_bs->sig.der.data,
					file_bs->sig.der.len, &timestamp);
	if(gtstate != GT_OK) {
		r = RSGTE_TS_DERDECODE;
		ectx->gtstate = gtstate;
		goto done;
	}

	gtstate = GTHTTP_verifyTimestampHash(timestamp, root, NULL,
			NULL, NULL, rsgt_read_puburl, 0, &vrfyInf);
	if(! (gtstate == GT_OK
	      && vrfyInf->verification_errors == GT_NO_FAILURES) ) {
		r = RSGTE_INVLD_TIMESTAMP;
		ectx->gtstate = gtstate;
		goto done;
	}

	if(rsgt_read_showVerified)
		reportVerifySuccess(ectx, vrfyInf);
	if(bExtend)
		if((r = rsgt_extendSig(timestamp, &rec, ectx)) != 0) goto done;
		
	if(nsigfp != NULL)
		if((r = rsgt_tlvwrite(nsigfp, &rec)) != 0) goto done;
	r = 0;
done:
	if(file_bs != NULL)
		rsgt_objfree(0x0904, file_bs);
	if(r != 0)
		reportError(r, ectx);
	if(timestamp != NULL)
		GTTimestamp_free(timestamp);
	return r;
}

/* Helper function to enable debug */
void rsgt_set_debug(int iDebug)
{
	rsgt_read_debug = iDebug; 
}

/* Helper function to convert an old V10 signature file into V11 */
int rsgt_ConvertSigFile(FILE *oldsigfp, FILE *newsigfp, int verbose)
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
	RSGT_FLAG_TLV16_RUNTIME = 0x20; 

	/* Start reading Sigblocks from old FILE */
	while(1) { /* we will err out on EOF */
		rRead = rsgt_tlvRecRead(oldsigfp, &rec); 
		if(rRead == 0 /*|| rRead == RSGTE_EOF*/) {
			switch(rec.tlvtype) {
				case 0x0900:
				case 0x0901:
					/* Convert tlvrecord Header */
					if (rec.tlvtype == 0x0900) {
						typconv = ((0x00 /*flags*/ | 0x80 /* NEW RSGT_FLAG_TLV16_RUNTIME*/) << 8) | 0x0902;
						rec.hdr[0] = typconv >> 8; 
						rec.hdr[1] = typconv & 0xff; 
					} else if (rec.tlvtype == 0x0901) {
						typconv = ((0x00 /*flags*/ | 0x80 /* NEW RSGT_FLAG_TLV16_RUNTIME*/) << 8) | 0x0903;
						rec.hdr[0] = typconv >> 8; 
						rec.hdr[1] = typconv & 0xff; 
					}

					/* Debug verification output */
					r = rsgt_tlvDecodeIMPRINT(&rec, &imp);
					if(r != 0) goto donedecode;
					rsgt_printREC_HASH(stdout, imp, verbose);

					/* Output into new FILE */
					if((r = rsgt_tlvwrite(newsigfp, &rec)) != 0) goto done;

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
					CHKrDecode(rsgt_tlvDecodeSUBREC(&rec, &strtidx, &subrec));
					if(!(subrec.tlvtype == 0x00 && subrec.tlvlen == 1)) {
						r = RSGTE_FMT;
						goto donedecode;
					}
					bh->hashID = subrec.data[0];

					/* Check OLD encoded BLOCK_IV */
					CHKrDecode(rsgt_tlvDecodeSUBREC(&rec, &strtidx, &subrec));
					if(!(subrec.tlvtype == 0x01)) {
						r = RSGTE_INVLTYP;
						goto donedecode;
					}
					if((bh->iv = (uint8_t*)malloc(subrec.tlvlen)) == NULL) {r=RSGTE_OOM;goto donedecode;}
					memcpy(bh->iv, subrec.data, subrec.tlvlen);

					/* Check OLD encoded LAST HASH */
					CHKrDecode(rsgt_tlvDecodeSUBREC(&rec, &strtidx, &subrec));
					if(!(subrec.tlvtype == 0x02)) { r = RSGTE_INVLTYP; goto donedecode; }
					bh->lastHash.hashID = subrec.data[0];
					if(subrec.tlvlen != 1 + hashOutputLengthOctets(bh->lastHash.hashID)) {
						r = RSGTE_LEN;
						goto donedecode;
					}
					bh->lastHash.len = subrec.tlvlen - 1;
					if((bh->lastHash.data = (uint8_t*)malloc(bh->lastHash.len)) == NULL) {r=RSGTE_OOM;goto donedecode;}
					memcpy(bh->lastHash.data, subrec.data+1, subrec.tlvlen-1);

					/* Debug verification output */
					rsgt_printBLOCK_HDR(stdout, bh, verbose);

					/* Check OLD encoded COUNT */
					CHKrDecode(rsgt_tlvDecodeSUBREC(&rec, &strtidx, &subrec));
					if(!(subrec.tlvtype == 0x03 && subrec.tlvlen <= 8)) { r = RSGTE_INVLTYP; goto donedecode; }
					bs->recCount = 0;
					for(i = 0 ; i < subrec.tlvlen ; ++i) {
						bs->recCount = (bs->recCount << 8) + subrec.data[i];
					}

					/* Check OLD encoded SIG */
					CHKrDecode(rsgt_tlvDecodeSUBREC(&rec, &strtidx, &subrec));
					if(!(subrec.tlvtype == 0x0906)) { r = RSGTE_INVLTYP; goto donedecode; }
					bs->sig.der.len = subrec.tlvlen;
					bs->sigID = SIGID_RFC3161;
					if((bs->sig.der.data = (uint8_t*)malloc(bs->sig.der.len)) == NULL) {r=RSGTE_OOM;goto donedecode;}
					memcpy(bs->sig.der.data, subrec.data, bs->sig.der.len);

					/* Debug output */
					rsgt_printBLOCK_SIG(stdout, bs, verbose);

					if(strtidx != rec.tlvlen) {
						r = RSGTE_LEN;
						goto donedecode;
					}

					/* Set back to NEW default */
					RSGT_FLAG_TLV16_RUNTIME = 0x80; 

					/* Create Block Header */
					tlvlen  = 2 + 1 /* hash algo TLV */ +
						  2 + hashOutputLengthOctets(bh->hashID) /* iv */ +
						  2 + 1 + bh->lastHash.len /* last hash */;
					/* write top-level TLV object block-hdr */
					CHKrDecode(rsgt_tlv16Write(newsigfp, 0x00, 0x0901, tlvlen));
					/* and now write the children */
					/* hash-algo */
					CHKrDecode(rsgt_tlv8Write(newsigfp, 0x00, 0x01, 1));
					CHKrDecode(rsgt_tlvfileAddOctet(newsigfp, hashIdentifier(bh->hashID)));
					/* block-iv */
					CHKrDecode(rsgt_tlv8Write(newsigfp, 0x00, 0x02, hashOutputLengthOctets(bh->hashID)));
					CHKrDecode(rsgt_tlvfileAddOctetString(newsigfp, bh->iv, hashOutputLengthOctets(bh->hashID)));
					/* last-hash */
					CHKrDecode(rsgt_tlv8Write(newsigfp, 0x00, 0x03, bh->lastHash.len + 1));
					CHKrDecode(rsgt_tlvfileAddOctet(newsigfp, bh->lastHash.hashID));
					CHKrDecode(rsgt_tlvfileAddOctetString(newsigfp, bh->lastHash.data, bh->lastHash.len));

					/* Create Block Signature */
					tlvlenRecords = rsgt_tlvGetInt64OctetSize(bs->recCount);
					tlvlen  = 2 + tlvlenRecords /* rec-count */ +
						  4 + bs->sig.der.len /* rfc-3161 */;
					/* write top-level TLV object (block-sig */
					CHKrDecode(rsgt_tlv16Write(newsigfp, 0x00, 0x0904, tlvlen));
					/* and now write the children */
					/* rec-count */
					CHKrDecode(rsgt_tlv8Write(newsigfp, 0x00, 0x01, tlvlenRecords));
					CHKrDecode(rsgt_tlvfileAddInt64(newsigfp, bs->recCount));
					/* rfc-3161 */
					CHKrDecode(rsgt_tlv16Write(newsigfp, 0x00, 0x906, bs->sig.der.len));
					CHKrDecode(rsgt_tlvfileAddOctetString(newsigfp, bs->sig.der.data, bs->sig.der.len));

donedecode:
					/* Set back to OLD default */
					RSGT_FLAG_TLV16_RUNTIME = 0x20; 

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
			else if(rsgt_read_debug)
				printf("debug: rsgt_ConvertSigFile failed to read with error %d\n", r);
			goto done;
		}

		/* Abort further processing if EOF */
		if (rRead == RSGTE_EOF)
			goto done;
	}
done:
	if(rsgt_read_debug)
		printf("debug: rsgt_ConvertSigFile returned %d\n", r);
	return r;
}
