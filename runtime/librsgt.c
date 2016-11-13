/* librsgt.c - rsyslog's guardtime support library
 *
 * Regarding the online algorithm for Merkle tree signing. Expected 
 * calling sequence is:
 *
 * sigblkConstruct
 * for each signature block:
 *    sigblkInit
 *    for each record:
 *       sigblkAddRecord
 *    sigblkFinish
 * sigblkDestruct
 *
 * Obviously, the next call after sigblkFinsh must either be to 
 * sigblkInit or sigblkDestruct (if no more signature blocks are
 * to be emitted, e.g. on file close). sigblkDestruct saves state
 * information (most importantly last block hash) and sigblkConstruct
 * reads (or initilizes if not present) it.
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
#define MAXFNAME 1024

#include <gt_http.h>

#include "librsgt_common.h"
#include "librsgt.h"

typedef unsigned char uchar;
#ifndef VERSION
#define VERSION "no-version"
#endif

int RSGT_FLAG_TLV16_RUNTIME = RSGT_FLAG_TLV16;
int RSGT_FLAG_NONCRIT_RUNTIME = RSGT_FLAG_NONCRIT; 

const char *
RSGTE2String(int err)
{
	switch(err) {
	case 0:
		return "success";
	case RSGTE_IO:
		return "i/o error";
	case RSGTE_FMT:
		return "data format error";
	case RSGTE_INVLTYP:
		return "invalid/unexpected tlv record type";
	case RSGTE_OOM:
		return "out of memory";
	case RSGTE_LEN:
		return "length record problem";
	case RSGTE_TS_EXTEND:
		return "error extending timestamp";
	case RSGTE_INVLD_RECCNT:
		return "mismatch between actual record count and number in block signature record";
	case RSGTE_INVLHDR:
		return "invalid file header";
	case RSGTE_EOF:
		return "EOF";
	case RSGTE_MISS_REC_HASH:
		return "record hash missing";
	case RSGTE_MISS_TREE_HASH:
		return "tree hash missing";
	case RSGTE_INVLD_REC_HASH:
		return "record hash mismatch";
	case RSGTE_INVLD_TREE_HASH:
		return "tree hash mismatch";
	case RSGTE_INVLD_REC_HASHID:
		return "invalid record hash ID";
	case RSGTE_INVLD_TREE_HASHID:
		return "invalid tree hash ID";
	case RSGTE_MISS_BLOCKSIG:
		return "missing block signature record";
	case RSGTE_INVLD_TIMESTAMP:
		return "RFC3161 timestamp invalid";
	case RSGTE_TS_DERDECODE:
		return "error DER-decoding RFC3161 timestamp";
	case RSGTE_TS_DERENCODE:
		return "error DER-encoding RFC3161 timestamp";
	case RSGTE_HASH_CREATE:
		return "error creating hash";
	case RSGTE_END_OF_SIG:
		return "unexpected end of signature";
	case RSGTE_END_OF_LOG:
		return "unexpected end of log";
	default:
		return "unknown error";
	}
}


uint16_t
hashOutputLengthOctets(uint8_t hashID)
{
	switch(hashID) {
	case GT_HASHALG_SHA1:	/* paper: SHA1 */
		return 20;
	case GT_HASHALG_RIPEMD160: /* paper: RIPEMD-160 */
		return 20;
	case GT_HASHALG_SHA224:	/* paper: SHA2-224 */
		return 28;
	case GT_HASHALG_SHA256: /* paper: SHA2-256 */
		return 32;
	case GT_HASHALG_SHA384: /* paper: SHA2-384 */
		return 48;
	case GT_HASHALG_SHA512:	/* paper: SHA2-512 */
		return 64;
	default:return 32;
	}
}

uint8_t
hashIdentifier(enum GTHashAlgorithm hashID)
{
	switch(hashID) {
	case GT_HASHALG_SHA1:	/* paper: SHA1 */
		return 0x00;
	case GT_HASHALG_RIPEMD160: /* paper: RIPEMD-160 */
		return 0x02;
	case GT_HASHALG_SHA224:	/* paper: SHA2-224 */
		return 0x03;
	case GT_HASHALG_SHA256: /* paper: SHA2-256 */
		return 0x01;
	case GT_HASHALG_SHA384: /* paper: SHA2-384 */
		return 0x04;
	case GT_HASHALG_SHA512:	/* paper: SHA2-512 */
		return 0x05;
	default:return 0xff;
	}
}
const char *
hashAlgName(uint8_t hashID)
{
	switch(hashID) {
	case GT_HASHALG_SHA1:
		return "SHA1";
	case GT_HASHALG_RIPEMD160:
		return "RIPEMD-160";
	case GT_HASHALG_SHA224:
		return "SHA2-224";
	case GT_HASHALG_SHA256:
		return "SHA2-256";
	case GT_HASHALG_SHA384:
		return "SHA2-384";
	case GT_HASHALG_SHA512:
		return "SHA2-512";
	default:return "[unknown]";
	}
}

enum GTHashAlgorithm
hashID2Alg(uint8_t hashID)
{
	switch(hashID) {
	case 0x00:
		return GT_HASHALG_SHA1;
	case 0x02:
		return GT_HASHALG_RIPEMD160;
	case 0x03:
		return GT_HASHALG_SHA224;
	case 0x01:
		return GT_HASHALG_SHA256;
	case 0x04:
		return GT_HASHALG_SHA384;
	case 0x05:
		return GT_HASHALG_SHA512;
	default:
		return 0xff;
	}
}

static void
reportErr(gtctx ctx, char *errmsg)
{
	if(ctx->errFunc == NULL)
		goto done;
	ctx->errFunc(ctx->usrptr, (uchar*)errmsg);
done:	return;
}

static void
reportGTAPIErr(gtctx ctx, gtfile gf, char *apiname, int ecode)
{
	char errbuf[4096];
	snprintf(errbuf, sizeof(errbuf), "%s[%s:%d]: %s",
		 (gf == NULL) ? (uchar*)"" : gf->sigfilename,
		 apiname, ecode, GTHTTP_getErrorString(ecode));
	errbuf[sizeof(errbuf)-1] = '\0';
	reportErr(ctx, errbuf);
}

void
rsgtsetErrFunc(gtctx ctx, void (*func)(void*, uchar *), void *usrptr)
{
	ctx->usrptr = usrptr;
	ctx->errFunc = func;
}


imprint_t *
rsgtImprintFromGTDataHash(GTDataHash *hash)
{
	imprint_t *imp;

	if((imp = calloc(1, sizeof(imprint_t))) == NULL) {
		goto done;
	}
	imp->hashID = hashIdentifier(hash->algorithm),
	imp->len = hash->digest_length;
	if((imp->data = (uint8_t*)malloc(imp->len)) == NULL) {
		free(imp); imp = NULL; goto done;
	}
	memcpy(imp->data, hash->digest, imp->len);
done:	return imp;
}

void
rsgtimprintDel(imprint_t *imp)
{
	if(imp != NULL) {
		free(imp->data),
		free(imp);
	}
}

int
rsgtInit(const char *usragent)
{
	int r = 0;
	int ret = GT_OK;

	ret = GT_init();
	if(ret != GT_OK) {
		r = 1;
		goto done;
	}
	ret = GTHTTP_init(usragent, 1);
	if(ret != GT_OK) {
		r = 1;
		goto done;
	}
done:	return r;
}

void
rsgtExit(void)
{
	GTHTTP_finalize();
	GT_finalize();
}


static gtfile
rsgtfileConstruct(gtctx ctx)
{
	gtfile gf = NULL;
	if((gf = calloc(1, sizeof(struct gtfile_s))) == NULL)
		goto done;
	gf->ctx = ctx;
	gf->hashAlg = ctx->hashAlg;
	gf->blockSizeLimit = ctx->blockSizeLimit;
	gf->bKeepRecordHashes = ctx->bKeepRecordHashes;
	gf->bKeepTreeHashes = ctx->bKeepTreeHashes;
	gf->x_prev = NULL;

done:	return gf;
}

static int
tlvbufPhysWrite(gtfile gf)
{
	ssize_t lenBuf;
	ssize_t iTotalWritten;
	ssize_t iWritten;
	char *pWriteBuf;
	int r = 0;

	lenBuf = gf->tlvIdx;
	pWriteBuf = gf->tlvBuf;
	iTotalWritten = 0;
	do {
		iWritten = write(gf->fd, pWriteBuf, lenBuf);
		if(iWritten < 0) {
			iWritten = 0; /* we have written NO bytes! */
			if(errno == EINTR) {
				/*NO ERROR, just continue */;
			} else {
				reportErr(gf->ctx, "signature file write error");
				r = RSGTE_IO;
				goto finalize_it;
			}
	 	} 
		/* advance buffer to next write position */
		iTotalWritten += iWritten;
		lenBuf -= iWritten;
		pWriteBuf += iWritten;
	} while(lenBuf > 0);	/* Warning: do..while()! */

finalize_it:
	gf->tlvIdx = 0;
	return r;
}

static int
tlvbufChkWrite(gtfile gf)
{
	if(gf->tlvIdx == sizeof(gf->tlvBuf)) {
		return tlvbufPhysWrite(gf);
	}
	return 0;
}


/* write to TLV file buffer. If buffer is full, an actual call occurs. Else
 * output is written only on flush or close.
 */
static int
tlvbufAddOctet(gtfile gf, int8_t octet)
{
	int r;
	r = tlvbufChkWrite(gf);
	if(r != 0) goto done;
	gf->tlvBuf[gf->tlvIdx++] = octet;
done:	return r;
}
static int
tlvbufAddOctetString(gtfile gf, uint8_t *octet, int size)
{
	int i, r = 0;
	for(i = 0 ; i < size ; ++i) {
		r = tlvbufAddOctet(gf, octet[i]);
		if(r != 0) goto done;
	}
done:	return r;
}
/* return the actual length in to-be-written octets of an integer */
static uint8_t
tlvbufGetInt64OctetSize(uint64_t val)
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
static int
tlvbufAddInt64(gtfile gf, uint64_t val)
{
	uint8_t doWrite = 0;
	int r;
	if(val >> 56) {
		r = tlvbufAddOctet(gf, (val >> 56) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 48) & 0xff)) {
		r = tlvbufAddOctet(gf, (val >> 48) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 40) & 0xff)) {
		r = tlvbufAddOctet(gf, (val >> 40) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 32) & 0xff)) {
		r = tlvbufAddOctet(gf, (val >> 32) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 24) & 0xff)) {
		r = tlvbufAddOctet(gf, (val >> 24) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 16) & 0xff)) {
		r = tlvbufAddOctet(gf, (val >> 16) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 8) & 0xff)) {
		r = tlvbufAddOctet(gf, (val >>  8) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	r = tlvbufAddOctet(gf, val & 0xff);
done:	return r;
}


static int
tlv8Write(gtfile gf, int flags, int tlvtype, int len)
{
	int r;
	assert((flags & RSGT_TYPE_MASK) == 0);
	assert((tlvtype & RSGT_TYPE_MASK) == tlvtype);
	r = tlvbufAddOctet(gf, (flags & ~RSGT_FLAG_TLV16_RUNTIME) | tlvtype);
	if(r != 0) goto done;
	r = tlvbufAddOctet(gf, len & 0xff);
done:	return r;
} 

static int
tlv16Write(gtfile gf, int flags, int tlvtype, uint16_t len)
{
	uint16_t typ;
	int r;
	assert((flags & RSGT_TYPE_MASK) == 0);
	assert((tlvtype >> 8 & RSGT_TYPE_MASK) == (tlvtype >> 8));
	typ = ((flags | RSGT_FLAG_TLV16_RUNTIME) << 8) | tlvtype;
	r = tlvbufAddOctet(gf, typ >> 8);
	if(r != 0) goto done;
	r = tlvbufAddOctet(gf, typ & 0xff);
	if(r != 0) goto done;
	r = tlvbufAddOctet(gf, (len >> 8) & 0xff);
	if(r != 0) goto done;
	r = tlvbufAddOctet(gf, len & 0xff);
done:	return r;
} 

int
tlvFlush(gtfile gf)
{
	return (gf->tlvIdx == 0) ? 0 : tlvbufPhysWrite(gf);
}

int
tlvWriteHash(gtfile gf, uint16_t tlvtype, GTDataHash *rec)
{
	unsigned tlvlen;
	int r;
	tlvlen = 1 + rec->digest_length;
	r = tlv16Write(gf, 0x00, tlvtype, tlvlen);
	if(r != 0) goto done;
	r = tlvbufAddOctet(gf, hashIdentifier(gf->hashAlg));
	if(r != 0) goto done;
	r = tlvbufAddOctetString(gf, rec->digest, rec->digest_length);
done:	return r;
}

int
tlvWriteBlockHdr(gtfile gf) {
	unsigned tlvlen;
	int r;
	tlvlen  = 2 + 1 /* hash algo TLV */ +
	 	  2 + hashOutputLengthOctets(gf->hashAlg) /* iv */ +
		  2 + 1 + gf->x_prev->len /* last hash */;
	/* write top-level TLV object block-hdr */
	CHKr(tlv16Write(gf, 0x00, 0x0901, tlvlen));
	/* and now write the children */
	/* hash-algo */
	CHKr(tlv8Write(gf, 0x00, 0x01, 1));
	CHKr(tlvbufAddOctet(gf, hashIdentifier(gf->hashAlg)));
	/* block-iv */
	CHKr(tlv8Write(gf, 0x00, 0x02, hashOutputLengthOctets(gf->hashAlg)));
	CHKr(tlvbufAddOctetString(gf, gf->IV, hashOutputLengthOctets(gf->hashAlg)));
	/* last-hash */
	CHKr(tlv8Write(gf, 0x00, 0x03, gf->x_prev->len + 1));
	CHKr(tlvbufAddOctet(gf, gf->x_prev->hashID));
	CHKr(tlvbufAddOctetString(gf, gf->x_prev->data, gf->x_prev->len));
done:	return r;
}

int
tlvWriteBlockSig(gtfile gf, uchar *der, uint16_t lenDer)
{
	unsigned tlvlen;
	uint8_t tlvlenRecords;
	int r;

	tlvlenRecords = tlvbufGetInt64OctetSize(gf->nRecords);
	tlvlen  = 2 + tlvlenRecords /* rec-count */ +
		  4 + lenDer /* rfc-3161 */;
	/* write top-level TLV object (block-sig */
	r = tlv16Write(gf, 0x00, 0x0904, tlvlen);
	if(r != 0) goto done;
	/* and now write the children */
	//FIXME: flags???
	/* rec-count */
	r = tlv8Write(gf, 0x00, 0x01, tlvlenRecords);
	if(r != 0) goto done;
	r = tlvbufAddInt64(gf, gf->nRecords);
	if(r != 0) goto done;
	/* rfc-3161 */
	r = tlv16Write(gf, 0x00, 0x906, lenDer);
	if(r != 0) goto done;
	r = tlvbufAddOctetString(gf, der, lenDer);
done:	return r;
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
static void
readStateFile(gtfile gf)
{
	int fd;
	struct rsgtstatefile sf;

	fd = open((char*)gf->statefilename, O_RDONLY|O_NOCTTY|O_CLOEXEC, 0600);
	if(fd == -1) goto err;

	if(read(fd, &sf, sizeof(sf)) != sizeof(sf)) goto err;
	if(strncmp(sf.hdr, "GTSTAT10", 8)) goto err;

	gf->x_prev = malloc(sizeof(imprint_t));
	if (gf->x_prev == NULL) goto err;
	gf->x_prev->len = sf.lenHash;
	gf->x_prev->hashID = sf.hashID;
	gf->x_prev->data = calloc(1, gf->x_prev->len);
	if (gf->x_prev->data == NULL) {
		free(gf->x_prev);
		gf->x_prev = NULL;
		goto err;
	}

	if(read(fd, gf->x_prev->data, gf->x_prev->len)
		!= (ssize_t) gf->x_prev->len) {
		rsgtimprintDel(gf->x_prev);
		gf->x_prev = NULL;
		goto err;
	}
	close(fd);
return;

err:
	gf->x_prev = malloc(sizeof(imprint_t));
	gf->x_prev->hashID = hashIdentifier(gf->hashAlg);
	gf->x_prev->len = hashOutputLengthOctets(gf->hashAlg);
	gf->x_prev->data = calloc(1, gf->x_prev->len);
}

/* persist all information that we need to re-open and append
 * to a log signature file.
 */
static void
writeStateFile(gtfile gf)
{
	int fd;
	struct rsgtstatefile sf;

	fd = open((char*)gf->statefilename,
		       O_WRONLY|O_CREAT|O_TRUNC|O_NOCTTY|O_CLOEXEC, 0600);
	if(fd == -1)
		goto done;

	memcpy(sf.hdr, "GTSTAT10", 8);
	sf.hashID = hashIdentifier(gf->hashAlg);
	sf.lenHash = gf->x_prev->len;
	/* if the write fails, we cannot do anything against that. We check
	 * the condition just to keep the compiler happy.
	 */
	if(write(fd, &sf, sizeof(sf))){};
	if(write(fd, gf->x_prev->data, gf->x_prev->len)){};
	close(fd);
done:	return;
}


int
tlvClose(gtfile gf)
{
	int r;
	r = tlvFlush(gf);
	close(gf->fd);
	gf->fd = -1;
	writeStateFile(gf);
	return r;
}


/* note: if file exists, the last hash for chaining must
 * be read from file.
 */
int
tlvOpen(gtfile gf, char *hdr, unsigned lenHdr)
{
	int r = 0;
	struct stat stat_st;

	gf->fd = open((char*)gf->sigfilename,
		       O_WRONLY|O_APPEND|O_NOCTTY|O_CLOEXEC, 0600);
	if(gf->fd == -1) {
		/* looks like we need to create a new file */
		gf->fd = open((char*)gf->sigfilename,
			       O_WRONLY|O_CREAT|O_NOCTTY|O_CLOEXEC, 0600);
		if(gf->fd == -1) {
			r = RSGTE_IO;
			goto done;
		}

		/* Write fileHeader */
		memcpy(gf->tlvBuf, hdr, lenHdr);
		gf->tlvIdx = lenHdr;
	} else {
		/* Get FileSize from existing ksisigfile */
		if(fstat(gf->fd, &stat_st) == -1) {
			reportErr(gf->ctx, "tlvOpen: can not stat file");
			r = RSGTE_IO;
			goto done;
		}

		/* Check if size is above header length. */
		if(stat_st.st_size > 0) {
			/* header already present! */
			gf->tlvIdx = 0;
		} else {
			/* Write fileHeader */
			memcpy(gf->tlvBuf, hdr, lenHdr);
			gf->tlvIdx = lenHdr;
		}
	}
	/* we now need to obtain the last previous hash, so that
	 * we can continue the hash chain. We do not check for error
	 * as a state file error can be recovered by graceful degredation.
	 */
	readStateFile(gf);
done:	return r;
}

/*
 * As of some Linux and security expert I spoke to, /dev/urandom
 * provides very strong random numbers, even if it runs out of
 * entropy. As far as he knew, this is save for all applications
 * (and he had good proof that I currently am not permitted to
 * reproduce). -- rgerhards, 2013-03-04
 */
void
seedIV(gtfile gf)
{
	int hashlen;
	int fd;

	hashlen = hashOutputLengthOctets(gf->hashAlg);
	gf->IV = malloc(hashlen); /* do NOT zero-out! */
	/* if we cannot obtain data from /dev/urandom, we use whatever
	 * is present at the current memory location as random data. Of
	 * course, this is very weak and we should consider a different
	 * option, especially when not running under Linux (for Linux,
	 * unavailability of /dev/urandom is just a theoretic thing, it
	 * will always work...).  -- TODO -- rgerhards, 2013-03-06
	 */
	if((fd = open("/dev/urandom", O_RDONLY)) > 0) {
		if(read(fd, gf->IV, hashlen)) {}; /* keep compiler happy */
		close(fd);
	}
}

gtctx
rsgtCtxNew(void)
{
	gtctx ctx;
	ctx = calloc(1, sizeof(struct gtctx_s));
	ctx->hashAlg = GT_HASHALG_SHA256;
	ctx->errFunc = NULL;
	ctx->usrptr = NULL;
	ctx->timestamper = strdup(
			   "http://stamper.guardtime.net/gt-signingservice");
	return ctx;
}

/* either returns gtfile object or NULL if something went wrong */
gtfile
rsgtCtxOpenFile(gtctx ctx, unsigned char *logfn)
{
	gtfile gf;
	char fn[MAXFNAME+1];

	if((gf = rsgtfileConstruct(ctx)) == NULL)
		goto done;

	snprintf(fn, sizeof(fn), "%s.gtsig", logfn);
	fn[MAXFNAME] = '\0'; /* be on save side */
	gf->sigfilename = (uchar*) strdup(fn);
	snprintf(fn, sizeof(fn), "%s.gtstate", logfn);
	fn[MAXFNAME] = '\0'; /* be on save side */
	gf->statefilename = (uchar*) strdup(fn);
	if(tlvOpen(gf, LOGSIGHDR, sizeof(LOGSIGHDR)-1) != 0) {
		reportErr(ctx, "signature file open failed");
		/* Free memory */
		free(gf);
		gf = NULL;
	}
done:	return gf;
}
 

/* returns 0 on succes, 1 if algo is unknown */
int
rsgtSetHashFunction(gtctx ctx, char *algName)
{
	int r = 0;
	if(!strcmp(algName, "SHA2-256"))
		ctx->hashAlg = GT_HASHALG_SHA256;
	else if(!strcmp(algName, "SHA2-384"))
		ctx->hashAlg = GT_HASHALG_SHA384;
	else if(!strcmp(algName, "SHA2-512"))
		ctx->hashAlg = GT_HASHALG_SHA512;
	else if(!strcmp(algName, "SHA1"))
		ctx->hashAlg = GT_HASHALG_SHA1;
	else if(!strcmp(algName, "RIPEMD-160"))
		ctx->hashAlg = GT_HASHALG_RIPEMD160;
	else if(!strcmp(algName, "SHA2-224"))
		ctx->hashAlg = GT_HASHALG_SHA224;
	else
		r = 1;
	return r;
}

int
rsgtfileDestruct(gtfile gf)
{
	int r = 0;
	if(gf == NULL)
		goto done;

	if(!gf->disabled && gf->bInBlk) {
		r = sigblkFinish(gf);
		if(r != 0) gf->disabled = 1;
	}
	if(!gf->disabled)
		r = tlvClose(gf);
	free(gf->sigfilename);
	free(gf->statefilename);
	free(gf->IV);
	rsgtimprintDel(gf->x_prev);
	free(gf);
done:	return r;
}

void
rsgtCtxDel(gtctx ctx)
{
	if(ctx != NULL) {
		free(ctx->timestamper);
		free(ctx);
	}
}

/* new sigblk is initialized, but maybe in existing ctx */
void
sigblkInit(gtfile gf)
{
	if(gf == NULL) goto done;
	seedIV(gf);
	memset(gf->roots_valid, 0, sizeof(gf->roots_valid));
	gf->nRoots = 0;
	gf->nRecords = 0;
	gf->bInBlk = 1;
done:	return;
}


/* concat: add IV to buffer */
static void
bufAddIV(gtfile gf, uchar *buf, size_t *len)
{
	int hashlen;

	hashlen = hashOutputLengthOctets(gf->hashAlg);

	memcpy(buf+*len, gf->IV, hashlen);
	*len += hashlen;
}


/* concat: add imprint to buffer */
static void
bufAddImprint(uchar *buf, size_t *len, imprint_t *imp)
{
	buf[*len] = imp->hashID;
	++(*len);
	memcpy(buf+*len, imp->data, imp->len);
	*len += imp->len;
}

/* concat: add hash to buffer */
static void
bufAddHash(gtfile gf, uchar *buf, size_t *len, GTDataHash *hash)
{
	buf[*len] = hashIdentifier(gf->hashAlg);
	++(*len);
	memcpy(buf+*len, hash->digest, hash->digest_length);
	*len += hash->digest_length;
}
/* concat: add tree level to buffer */
static void
bufAddLevel(uchar *buf, size_t *len, uint8_t level)
{
	memcpy(buf+*len, &level, sizeof(level));
	*len += sizeof(level);
}


int
hash_m(gtfile gf, GTDataHash **m)
{
	int rgt;
	uchar concatBuf[16*1024];
	size_t len = 0;
	int r = 0;

	bufAddImprint(concatBuf, &len, gf->x_prev);
	bufAddIV(gf, concatBuf, &len);
	rgt = GTDataHash_create(gf->hashAlg, concatBuf, len, m);
	if(rgt != GT_OK) {
		reportGTAPIErr(gf->ctx, gf, "GTDataHash_create", rgt);
		r = RSGTE_HASH_CREATE;
		goto done;
	}
done:	return r;
}

int
hash_r(gtfile gf, GTDataHash **r, const uchar *rec, const size_t len)
{
	int ret = 0, rgt;
	rgt = GTDataHash_create(gf->hashAlg, rec, len, r);
	if(rgt != GT_OK) {
		reportGTAPIErr(gf->ctx, gf, "GTDataHash_create", rgt);
		ret = RSGTE_HASH_CREATE;
		goto done;
	}
done:	return ret;
}


int
hash_node(gtfile gf, GTDataHash **node, GTDataHash *m, GTDataHash *rec,
          uint8_t level)
{
	int r = 0, rgt;
	uchar concatBuf[16*1024];
	size_t len = 0;

	bufAddHash(gf, concatBuf, &len, m);
	bufAddHash(gf, concatBuf, &len, rec);
	bufAddLevel(concatBuf, &len, level);
	rgt = GTDataHash_create(gf->hashAlg, concatBuf, len, node);
	if(rgt != GT_OK) {
		reportGTAPIErr(gf->ctx, gf, "GTDataHash_create", rgt);
		r = RSGTE_HASH_CREATE;
		goto done;
	}
done:	return r;
}


int
sigblkAddRecord(gtfile gf, const uchar *rec, const size_t len)
{
	GTDataHash *x; /* current hash */
	GTDataHash *m, *r, *t, *t_del;
	uint8_t j;
	int ret = 0;

	if(gf == NULL || gf->disabled) goto done;
	if((ret = hash_m(gf, &m)) != 0) goto done;
	if((ret = hash_r(gf, &r, rec, len)) != 0) goto done;
	if(gf->nRecords == 0) 
		tlvWriteBlockHdr(gf);
	if(gf->bKeepRecordHashes)
		tlvWriteHash(gf, 0x0902, r);
	if((ret = hash_node(gf, &x, m, r, 1)) != 0) goto done; /* hash leaf */
	/* persists x here if Merkle tree needs to be persisted! */
	if(gf->bKeepTreeHashes)
		tlvWriteHash(gf, 0x0903, x);
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
			t_del = t;
			ret = hash_node(gf, &t, gf->roots_hash[j], t_del, j+2);
			gf->roots_valid[j] = 0;
			GTDataHash_free(gf->roots_hash[j]);
			GTDataHash_free(t_del);
			if(ret != 0) goto done;
			if(gf->bKeepTreeHashes)
				tlvWriteHash(gf, 0x0903, t);
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

	/* cleanup (x is cleared as part of the roots array) */
	GTDataHash_free(m);
	GTDataHash_free(r);

	if(gf->nRecords == gf->blockSizeLimit) {
		ret = sigblkFinish(gf);
		if(ret != 0) goto done;
		sigblkInit(gf);
	}
done:	
	if(ret != 0) {
		gf->disabled = 1;
	}
	return ret;
}

static int
timestampIt(gtfile gf, GTDataHash *hash)
{
	unsigned char *der = NULL;
	size_t lenDer;
	int r = GT_OK;
	int ret = 0;
	GTTimestamp *timestamp = NULL;

	/* Get the timestamp. */
	r = GTHTTP_createTimestampHash(hash, gf->ctx->timestamper, &timestamp);

	if(r != GT_OK) {
		reportGTAPIErr(gf->ctx, gf, "GTHTTP_createTimestampHash", r);
		ret = 1;
		goto done;
	}

	/* Encode timestamp. */
	r = GTTimestamp_getDEREncoded(timestamp, &der, &lenDer);
	if(r != GT_OK) {
		reportGTAPIErr(gf->ctx, gf, "GTTimestamp_getDEREncoded", r);
		ret = 1;
		goto done;
	}

	tlvWriteBlockSig(gf, der, lenDer);

done:
	GT_free(der);
	GTTimestamp_free(timestamp);
	return ret;
}


int
sigblkFinish(gtfile gf)
{
	GTDataHash *root, *rootDel;
	int8_t j;
	int ret = 0;

	if(gf->nRecords == 0)
		goto done;

	root = NULL;
	for(j = 0 ; j < gf->nRoots ; ++j) {
		if(root == NULL) {
			root = gf->roots_valid[j] ? gf->roots_hash[j] : NULL;
			gf->roots_valid[j] = 0;
		} else if(gf->roots_valid[j]) {
			rootDel = root;
			ret = hash_node(gf, &root, gf->roots_hash[j], rootDel, j+2);
			gf->roots_valid[j] = 0;
			GTDataHash_free(gf->roots_hash[j]);
			GTDataHash_free(rootDel);
			if(ret != 0) goto done; /* checks hash_node() result! */
		}
	}
	if((ret = timestampIt(gf, root)) != 0) goto done;

	GTDataHash_free(root);
done:
	gf->bInBlk = 0;
	return ret;
}
