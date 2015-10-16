/* librsksi.c - rsyslog's KSI support library
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

#include <ksi/ksi.h>
#include "librsgt_common.h"
#include "librsksi.h"

typedef unsigned char uchar;
#ifndef VERSION
#define VERSION "no-version"
#endif

int RSKSI_FLAG_TLV16_RUNTIME = RSGT_FLAG_TLV16;
int RSKSI_FLAG_NONCRIT_RUNTIME = RSGT_FLAG_NONCRIT; 

static void
reportErr(rsksictx ctx, char *errmsg)
{
	if(ctx->errFunc == NULL)
		goto done;
	ctx->errFunc(ctx->usrptr, (uchar*)errmsg);
done:	return;
}

static void
reportKSIAPIErr(rsksictx ctx, ksifile ksi, char *apiname, int ecode)
{
	char errbuf[4096];
	snprintf(errbuf, sizeof(errbuf), "%s[%s:%d]: %s",
		 (ksi == NULL) ? (uchar*)"" : ksi->sigfilename,
		 apiname, ecode, KSI_getErrorString(ecode));
	errbuf[sizeof(errbuf)-1] = '\0';
	reportErr(ctx, errbuf);
}

void
rsksisetErrFunc(rsksictx ctx, void (*func)(void*, uchar *), void *usrptr)
{
	ctx->usrptr = usrptr;
	ctx->errFunc = func;
}

imprint_t *
rsksiImprintFromKSI_DataHash(ksifile ksi, KSI_DataHash *hash)
{
	int r;
	imprint_t *imp;
	const unsigned char *digest;
	unsigned digest_len;

	if((imp = calloc(1, sizeof(imprint_t))) == NULL) {
		goto done;
	}
	int hashID;
	r = KSI_DataHash_extract(hash, &hashID, &digest, &digest_len); 
	if (r != KSI_OK){
		reportKSIAPIErr(ksi->ctx, ksi, "KSI_DataHash_extract", r);
		free(imp); imp = NULL; goto done;
	}

	imp->hashID = hashID;
	imp->len = digest_len;
	if((imp->data = (uint8_t*)malloc(imp->len)) == NULL) {
		free(imp); imp = NULL; goto done;
	}
	memcpy(imp->data, digest, digest_len);
done:	return imp;
}

void
rsksiimprintDel(imprint_t *imp)
{
	if(imp != NULL) {
		free(imp->data),
		free(imp);
	}
}

int
rsksiInit(char *usragent)
{
	int r = 0;
	int ret = KSI_OK;

/*
	ret = GT_init();
	if(ret != KSI_OK) {
		r = 1;
		goto done;
	}
	ret = GTHTTP_init(usragent, 1);
	if(ret != KSI_OK) {
		r = 1;
		goto done;
	}
*/

done:	return r;
}

void
rsksiExit(void)
{
	return; 
}

static inline ksifile
rsksifileConstruct(rsksictx ctx)
{
	ksifile ksi;
	if((ksi = calloc(1, sizeof(struct ksifile_s))) == NULL)
		goto done;
	ksi->ctx = ctx;
	ksi->hashAlg = ctx->hashAlg;
	ksi->blockSizeLimit = ctx->blockSizeLimit;
	ksi->bKeepRecordHashes = ctx->bKeepRecordHashes;
	ksi->bKeepTreeHashes = ctx->bKeepTreeHashes;
	ksi->x_prev = NULL;

done:	return ksi;
}

static inline int
tlvbufPhysWrite(ksifile ksi)
{
	ssize_t lenBuf;
	ssize_t iTotalWritten;
	ssize_t iWritten;
	char *pWriteBuf;
	int r = 0;

	lenBuf = ksi->tlvIdx;
	pWriteBuf = ksi->tlvBuf;
	iTotalWritten = 0;
	do {
		iWritten = write(ksi->fd, pWriteBuf, lenBuf);
		if(iWritten < 0) {
			iWritten = 0; /* we have written NO bytes! */
			if(errno == EINTR) {
				/*NO ERROR, just continue */;
			} else {
				reportErr(ksi->ctx, "signature file write error");
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
	ksi->tlvIdx = 0;
	return r;
}

static inline int
tlvbufChkWrite(ksifile ksi)
{
	if(ksi->tlvIdx == sizeof(ksi->tlvBuf)) {
		return tlvbufPhysWrite(ksi);
	}
	return 0;
}


/* write to TLV file buffer. If buffer is full, an actual call occurs. Else
 * output is written only on flush or close.
 */
static inline int
tlvbufAddOctet(ksifile ksi, int8_t octet)
{
	int r;
	r = tlvbufChkWrite(ksi);
	if(r != 0) goto done;
	ksi->tlvBuf[ksi->tlvIdx++] = octet;
done:	return r;
}
static inline int
tlvbufAddOctetString(ksifile ksi, uint8_t *octet, int size)
{
	int i, r = 0;
	for(i = 0 ; i < size ; ++i) {
		r = tlvbufAddOctet(ksi, octet[i]);
		if(r != 0) goto done;
	}
done:	return r;
}
/* return the actual length in to-be-written octets of an integer */
static inline uint8_t
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
static inline int
tlvbufAddInt64(ksifile ksi, uint64_t val)
{
	uint8_t doWrite = 0;
	int r;
	if(val >> 56) {
		r = tlvbufAddOctet(ksi, (val >> 56) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 48) & 0xff)) {
		r = tlvbufAddOctet(ksi, (val >> 48) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 40) & 0xff)) {
		r = tlvbufAddOctet(ksi, (val >> 40) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 32) & 0xff)) {
		r = tlvbufAddOctet(ksi, (val >> 32) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 24) & 0xff)) {
		r = tlvbufAddOctet(ksi, (val >> 24) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 16) & 0xff)) {
		r = tlvbufAddOctet(ksi, (val >> 16) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	if(doWrite || ((val >> 8) & 0xff)) {
		r = tlvbufAddOctet(ksi, (val >>  8) & 0xff), doWrite = 1;
		if(r != 0) goto done;
	}
	r = tlvbufAddOctet(ksi, val & 0xff);
done:	return r;
}


int
tlv8WriteKSI(ksifile ksi, int flags, int tlvtype, int len)
{
	int r;
	assert((flags & RSGT_TYPE_MASK) == 0);
	assert((tlvtype & RSGT_TYPE_MASK) == tlvtype);
	r = tlvbufAddOctet(ksi, (flags & ~RSKSI_FLAG_TLV16_RUNTIME) | tlvtype);
	if(r != 0) goto done;
	r = tlvbufAddOctet(ksi, len & 0xff);
done:	return r;
} 

int
tlv16WriteKSI(ksifile ksi, int flags, int tlvtype, uint16_t len)
{
	uint16_t typ;
	int r;
	assert((flags & RSGT_TYPE_MASK) == 0);
	assert((tlvtype >> 8 & RSGT_TYPE_MASK) == (tlvtype >> 8));
	typ = ((flags | RSKSI_FLAG_TLV16_RUNTIME) << 8) | tlvtype;
	r = tlvbufAddOctet(ksi, typ >> 8);
	if(r != 0) goto done;
	r = tlvbufAddOctet(ksi, typ & 0xff);
	if(r != 0) goto done;
	r = tlvbufAddOctet(ksi, (len >> 8) & 0xff);
	if(r != 0) goto done;
	r = tlvbufAddOctet(ksi, len & 0xff);
done:	return r;
} 

int
tlvFlushKSI(ksifile ksi)
{
	return (ksi->tlvIdx == 0) ? 0 : tlvbufPhysWrite(ksi);
}

int
tlvWriteHashKSI(ksifile ksi, uint16_t tlvtype, KSI_DataHash *rec)
{
	unsigned tlvlen;
	int r;
	const unsigned char *digest;
	unsigned digest_len;
	r = KSI_DataHash_extract(rec, NULL, &digest, &digest_len); 
	if (r != KSI_OK){
		reportKSIAPIErr(ksi->ctx, ksi, "KSI_DataHash_extract", r);
		goto done;
	}
	tlvlen = 1 + digest_len;
	r = tlv16WriteKSI(ksi, 0x00, tlvtype, tlvlen);
	if(r != 0) goto done;
	r = tlvbufAddOctet(ksi, hashIdentifierKSI(ksi->hashAlg));
	if(r != 0) goto done;
	r = tlvbufAddOctetString(ksi, (unsigned char*)digest, digest_len);
done:	return r;
}

int
tlvWriteBlockHdrKSI(ksifile ksi) {
	unsigned tlvlen;
	int r;
	tlvlen  = 2 + 1 /* hash algo TLV */ +
	 	  2 + hashOutputLengthOctetsKSI(ksi->hashAlg) /* iv */ +
		  2 + 1 + ksi->x_prev->len /* last hash */;
	/* write top-level TLV object block-hdr */
	r = tlv16WriteKSI(ksi, 0x00, 0x0901, tlvlen);
	/* and now write the children */
	/* hash-algo */
	r = tlv8WriteKSI(ksi, 0x00, 0x01, 1);
	if(r != 0) goto done;
	r = tlvbufAddOctet(ksi, hashIdentifierKSI(ksi->hashAlg));
	if(r != 0) goto done;
	/* block-iv */
	r = tlv8WriteKSI(ksi, 0x00, 0x02, hashOutputLengthOctetsKSI(ksi->hashAlg));
	if(r != 0) goto done;
	r = tlvbufAddOctetString(ksi, ksi->IV, hashOutputLengthOctetsKSI(ksi->hashAlg));
	if(r != 0) goto done;
	/* last-hash */
	r = tlv8WriteKSI(ksi, 0x00, 0x03, ksi->x_prev->len + 1);
	if(r != 0) goto done;
	r = tlvbufAddOctet(ksi, ksi->x_prev->hashID);
	if(r != 0) goto done;
	r = tlvbufAddOctetString(ksi, ksi->x_prev->data, ksi->x_prev->len);
	if(r != 0) goto done;
done:	return r;
}

int
tlvWriteBlockSigKSI(ksifile ksi, uchar *der, uint16_t lenDer)
{
	unsigned tlvlen;
	uint8_t tlvlenRecords;
	int r;

	tlvlenRecords = tlvbufGetInt64OctetSize(ksi->nRecords);
	tlvlen  = 2 + tlvlenRecords /* rec-count */ +
		  4 + lenDer /* rfc-3161 */;
	/* write top-level TLV object (block-sig */
	r = tlv16WriteKSI(ksi, 0x00, 0x0904, tlvlen);
	if(r != 0) goto done;
	/* and now write the children */
	//FIXME: flags???
	/* rec-count */
	r = tlv8WriteKSI(ksi, 0x00, 0x01, tlvlenRecords);
	if(r != 0) goto done;
	r = tlvbufAddInt64(ksi, ksi->nRecords);
	if(r != 0) goto done;
	/* rfc-3161 */
	r = tlv16WriteKSI(ksi, 0x00, 0x906, lenDer);
	if(r != 0) goto done;
	r = tlvbufAddOctetString(ksi, der, lenDer);
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
readStateFile(ksifile ksi)
{
	int fd;
	struct rsksistatefile sf;

	fd = open((char*)ksi->statefilename, O_RDONLY|O_NOCTTY|O_CLOEXEC, 0600);
	if(fd == -1) goto err;

	if(read(fd, &sf, sizeof(sf)) != sizeof(sf)) goto err;
	if(strncmp(sf.hdr, "KSISTAT10", 9)) goto err;

	ksi->x_prev = malloc(sizeof(imprint_t));
	if (ksi->x_prev == NULL) goto err;
	ksi->x_prev->len = sf.lenHash;
	ksi->x_prev->hashID = sf.hashID;
	ksi->x_prev->data = calloc(1, ksi->x_prev->len);
	if (ksi->x_prev->data == NULL) {
		free(ksi->x_prev);
		ksi->x_prev = NULL;
	}

	if(read(fd, ksi->x_prev->data, ksi->x_prev->len)
		!= ksi->x_prev->len) {
		rsksiimprintDel(ksi->x_prev);
		ksi->x_prev = NULL;
		goto err;
	}
	close(fd);
return;

err:

	ksi->x_prev = malloc(sizeof(imprint_t));
	ksi->x_prev->hashID = hashIdentifierKSI(ksi->hashAlg);
	ksi->x_prev->len = hashOutputLengthOctetsKSI(ksi->hashAlg);
	ksi->x_prev->data = calloc(1, ksi->x_prev->len);
}

/* persist all information that we need to re-open and append
 * to a log signature file.
 */
static void
writeStateFile(ksifile ksi)
{
	int fd;
	struct rsksistatefile sf;

	fd = open((char*)ksi->statefilename,
		       O_WRONLY|O_CREAT|O_TRUNC|O_NOCTTY|O_CLOEXEC, 0600);
	if(fd == -1)
		goto done;

	memcpy(sf.hdr, "KSISTAT10", 9);
	sf.hashID = hashIdentifierKSI(ksi->hashAlg);
	sf.lenHash = ksi->x_prev->len;
	/* if the write fails, we cannot do anything against that. We check
	 * the condition just to keep the compiler happy.
	 */
	if(write(fd, &sf, sizeof(sf))){};
	if(write(fd, ksi->x_prev->data, ksi->x_prev->len)){};
	close(fd);
done:	return;
}


int
tlvCloseKSI(ksifile ksi)
{
	int r;
	r = tlvFlushKSI(ksi);
	close(ksi->fd);
	ksi->fd = -1;
	writeStateFile(ksi);
	return r;
}


/* note: if file exists, the last hash for chaining must
 * be read from file.
 */
int
tlvOpenKSI(ksifile ksi, char *hdr, unsigned lenHdr)
{
	int r = 0;
	ksi->fd = open((char*)ksi->sigfilename,
		       O_WRONLY|O_APPEND|O_NOCTTY|O_CLOEXEC, 0600);
	if(ksi->fd == -1) {
		/* looks like we need to create a new file */
		ksi->fd = open((char*)ksi->sigfilename,
			       O_WRONLY|O_CREAT|O_NOCTTY|O_CLOEXEC, 0600);
		if(ksi->fd == -1) {
			r = RSGTE_IO;
			goto done;
		}
		memcpy(ksi->tlvBuf, hdr, lenHdr);
		ksi->tlvIdx = lenHdr;
	} else {
		ksi->tlvIdx = 0; /* header already present! */
	}
	/* we now need to obtain the last previous hash, so that
	 * we can continue the hash chain. We do not check for error
	 * as a state file error can be recovered by graceful degredation.
	 */
	readStateFile(ksi);
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
seedIVKSI(ksifile ksi)
{
	int hashlen;
	int fd;

	hashlen = hashOutputLengthOctetsKSI(ksi->hashAlg);
	ksi->IV = malloc(hashlen); /* do NOT zero-out! */
	/* if we cannot obtain data from /dev/urandom, we use whatever
	 * is present at the current memory location as random data. Of
	 * course, this is very weak and we should consider a different
	 * option, especially when not running under Linux (for Linux,
	 * unavailability of /dev/urandom is just a theoretic thing, it
	 * will always work...).  -- TODO -- rgerhards, 2013-03-06
	 */
	if((fd = open("/dev/urandom", O_RDONLY)) > 0) {
		if(read(fd, ksi->IV, hashlen)) {}; /* keep compiler happy */
		close(fd);
	}
}

rsksictx
rsksiCtxNew(void)
{
	rsksictx ctx;
	ctx = calloc(1, sizeof(struct rsksictx_s));
	KSI_CTX_new(&ctx->ksi_ctx); // TODO: error check (probably via a generic macro?)
	ctx->hashAlg = KSI_HASHALG_SHA2_256;
	ctx->errFunc = NULL;
	ctx->usrptr = NULL;
	ctx->timestamper = strdup(
			   "http://stamper.guardtime.net/gt-signingservice");
	return ctx;
}

/* either returns ksifile object or NULL if something went wrong */
ksifile
rsksiCtxOpenFile(rsksictx ctx, unsigned char *logfn)
{
	ksifile ksi;
	char fn[MAXFNAME+1];

	if((ksi = rsksifileConstruct(ctx)) == NULL)
		goto done;

	snprintf(fn, sizeof(fn), "%s.ksisig", logfn);
	fn[MAXFNAME] = '\0'; /* be on save side */
	ksi->sigfilename = (uchar*) strdup(fn);
	snprintf(fn, sizeof(fn), "%s.ksistate", logfn);
	fn[MAXFNAME] = '\0'; /* be on save side */
	ksi->statefilename = (uchar*) strdup(fn);
	if(tlvOpenKSI(ksi, LOGSIGHDR, sizeof(LOGSIGHDR)-1) != 0) {
		reportErr(ctx, "signature file open failed");
		ksi = NULL;
	}
done:	return ksi;
}
 

/* returns 0 on succes, 1 if algo is unknown */
int
rsksiSetHashFunction(rsksictx ctx, char *algName)
{
	int r = 0;
	if(!strcmp(algName, "SHA1"))
		ctx->hashAlg = KSI_HASHALG_SHA1;
	else if(!strcmp(algName, "SHA2-256"))
		ctx->hashAlg = KSI_HASHALG_SHA2_256;
	else if(!strcmp(algName, "RIPEMD-160"))
		ctx->hashAlg = KSI_HASHALG_RIPEMD160;
	else if(!strcmp(algName, "SHA2-224"))
		ctx->hashAlg = KSI_HASHALG_SHA2_224;
	else if(!strcmp(algName, "SHA2-384"))
		ctx->hashAlg = KSI_HASHALG_SHA2_384;
	else if(!strcmp(algName, "SHA2-512"))
		ctx->hashAlg = KSI_HASHALG_SHA2_512;
	else if(!strcmp(algName, "RIPEMD-256"))
		ctx->hashAlg = KSI_HASHALG_RIPEMD_256;
	else if(!strcmp(algName, "SHA3-244"))
		ctx->hashAlg = KSI_HASHALG_SHA3_244;
	else if(!strcmp(algName, "SHA3-256"))
		ctx->hashAlg = KSI_HASHALG_SHA3_256;
	else if(!strcmp(algName, "SHA3-384"))
		ctx->hashAlg = KSI_HASHALG_SHA3_384;
	else if(!strcmp(algName, "SHA3-512"))
		ctx->hashAlg = KSI_HASHALG_SHA3_512;
	else if(!strcmp(algName, "SM3"))
		ctx->hashAlg = KSI_HASHALG_SM3;
	else
		r = 1;
	return r;
}

int
rsksifileDestruct(ksifile ksi)
{
	int r = 0;
	if(ksi == NULL)
		goto done;

	if(!ksi->disabled && ksi->bInBlk) {
		r = sigblkFinishKSI(ksi);
		if(r != 0) ksi->disabled = 1;
	}
	if(!ksi->disabled)
		r = tlvCloseKSI(ksi);
	free(ksi->sigfilename);
	free(ksi->statefilename);
	free(ksi->IV);
	rsksiimprintDel(ksi->x_prev);
	free(ksi);
done:	return r;
}

void
rsksiCtxDel(rsksictx ctx)
{
	if(ctx != NULL) {
		free(ctx->timestamper);
		KSI_CTX_free(ctx->ksi_ctx);
		free(ctx);
	}
}

/* new sigblk is initialized, but maybe in existing ctx */
void
sigblkInitKSI(ksifile ksi)
{
	if(ksi == NULL) goto done;
	seedIVKSI(ksi);
	memset(ksi->roots_valid, 0, sizeof(ksi->roots_valid)/sizeof(char));
	ksi->nRoots = 0;
	ksi->nRecords = 0;
	ksi->bInBlk = 1;
done:	return;
}


/* concat: add IV to buffer */
static inline void
bufAddIV(ksifile ksi, uchar *buf, size_t *len)
{
	int hashlen;

	hashlen = hashOutputLengthOctetsKSI(ksi->hashAlg);

	memcpy(buf+*len, ksi->IV, hashlen);
	*len += hashlen;
}


/* concat: add imprint to buffer */
static inline void
bufAddImprint(ksifile ksi, uchar *buf, size_t *len, imprint_t *imp)
{
	buf[*len] = imp->hashID;
	++(*len);
	memcpy(buf+*len, imp->data, imp->len);
	*len += imp->len;
}

/* concat: add hash to buffer */
static inline void
bufAddHash(ksifile ksi, uchar *buf, size_t *len, KSI_DataHash *hash)
{
	int r; 
	const unsigned char *digest;
	unsigned digest_len;
	r = KSI_DataHash_extract(hash, NULL, &digest, &digest_len); // TODO: error check
	if (r != KSI_OK){
		reportKSIAPIErr(ksi->ctx, ksi, "KSI_DataHash_extract", r);
		goto done;
	}
	buf[*len] = hashIdentifierKSI(ksi->hashAlg);
	++(*len);
	memcpy(buf+*len, digest, digest_len);
	*len += digest_len;
done:	return;
}
/* concat: add tree level to buffer */
static inline void
bufAddLevel(uchar *buf, size_t *len, uint8_t level)
{
	memcpy(buf+*len, &level, sizeof(level));
	*len += sizeof(level);
}


int
hash_m_ksi(ksifile ksi, KSI_DataHash **m)
{
	int rgt;
	uchar concatBuf[16*1024];
	size_t len = 0;
	int r = 0;

	bufAddImprint(ksi, concatBuf, &len, ksi->x_prev);
	bufAddIV(ksi, concatBuf, &len);
	rgt = KSI_DataHash_create(ksi->ctx->ksi_ctx, concatBuf, len, ksi->hashAlg, m);
	if(rgt != KSI_OK) {
		reportKSIAPIErr(ksi->ctx, ksi, "KSI_DataHash_create", rgt);
		r = RSGTE_HASH_CREATE;
		goto done;
	}
done:	return r;
}

int
hash_r_ksi(ksifile ksi, KSI_DataHash **r, const uchar *rec, const size_t len)
{
	int ret = 0, rgt;
	rgt = KSI_DataHash_create(ksi->ctx->ksi_ctx, rec, len, ksi->hashAlg, r);
	if(rgt != KSI_OK) {
		reportKSIAPIErr(ksi->ctx, ksi, "KSI_DataHash_create", rgt);
		ret = RSGTE_HASH_CREATE;
		goto done;
	}
done:	return ret;
}


int
hash_node_ksi(ksifile ksi, KSI_DataHash **node, KSI_DataHash *m, KSI_DataHash *rec,
          uint8_t level)
{
	int r = 0, rgt;
	uchar concatBuf[16*1024];
	size_t len = 0;

	bufAddHash(ksi, concatBuf, &len, m);
	bufAddHash(ksi, concatBuf, &len, rec);
	bufAddLevel(concatBuf, &len, level);
	rgt = KSI_DataHash_create(ksi->ctx->ksi_ctx, concatBuf, len, ksi->hashAlg, node);
	if(rgt != KSI_OK) {
		reportKSIAPIErr(ksi->ctx, ksi, "KSI_DataHash_create", rgt);
		r = RSGTE_HASH_CREATE;
		goto done;
	}
done:	return r;
}


int
sigblkAddRecordKSI(ksifile ksi, const uchar *rec, const size_t len)
{
	KSI_DataHash *x; /* current hash */
	KSI_DataHash *m, *r, *t, *t_del;
	uint8_t j;
	int ret = 0;

	if(ksi == NULL || ksi->disabled) goto done;
	if((ret = hash_m_ksi(ksi, &m)) != 0) goto done;
	if((ret = hash_r_ksi(ksi, &r, rec, len)) != 0) goto done;
	if(ksi->nRecords == 0) 
		tlvWriteBlockHdrKSI(ksi);
	if(ksi->bKeepRecordHashes)
		tlvWriteHashKSI(ksi, 0x0902, r);
	if((ret = hash_node_ksi(ksi, &x, m, r, 1)) != 0) goto done; /* hash leaf */
	/* persists x here if Merkle tree needs to be persisted! */
	if(ksi->bKeepTreeHashes)
		tlvWriteHashKSI(ksi, 0x0903, x);
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
			t_del = t;
			ret = hash_node_ksi(ksi, &t, ksi->roots_hash[j], t_del, j+2);
			ksi->roots_valid[j] = 0;
			KSI_DataHash_free(ksi->roots_hash[j]);
			KSI_DataHash_free(t_del);
			if(ret != 0) goto done;
			if(ksi->bKeepTreeHashes)
				tlvWriteHashKSI(ksi, 0x0903, t);
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

	/* cleanup (x is cleared as part of the roots array) */
	KSI_DataHash_free(m);
	KSI_DataHash_free(r);

	if(ksi->nRecords == ksi->blockSizeLimit) {
		ret = sigblkFinishKSI(ksi);
		if(ret != 0) goto done;
		sigblkInitKSI(ksi);
	}
done:	
	if(ret != 0) {
		ksi->disabled = 1;
	}
	return ret;
}

static int
signIt(ksifile ksi, KSI_DataHash *hash)
{
	unsigned char *der = NULL;
	unsigned lenDer;
	int r = KSI_OK;
	int ret = 0;
	KSI_Signature *sig = NULL;

	/* Sign the root hash. */
	r = KSI_Signature_createAggregated(ksi->ctx->ksi_ctx, hash, 0, &sig);
	if(r != KSI_OK) {
		reportKSIAPIErr(ksi->ctx, ksi, "KSI_Signature_createAggregated", r);
		ret = 1;
		goto done;
	}

	/* Sign the hash. */
/*	r = KSI_createSignature(ksi->ctx->ksi_ctx, hash, &sig);
	if(r != KSI_OK) {
		reportKSIAPIErr(ksi->ctx, ksi, "KSI_createSignature", r);
		ret = 1;
		goto done;
	}
*/

	/* Serialize Signature. */
	r = KSI_Signature_serialize(sig, &der, &lenDer);
	if(r != KSI_OK) {
		reportKSIAPIErr(ksi->ctx, ksi, "KSI_Signature_serialize", r);
		ret = 1;
		goto done;
	}

	tlvWriteBlockSigKSI(ksi, der, lenDer);
done:
	if (sig != NULL)
		KSI_Signature_free(sig);
	if (der != NULL)
		KSI_free(der);
	return ret;
}


int
sigblkFinishKSI(ksifile ksi)
{
	KSI_DataHash *root, *rootDel;
	int8_t j;
	int ret = 0;

	if(ksi->nRecords == 0)
		goto done;

	root = NULL;
	for(j = 0 ; j < ksi->nRoots ; ++j) {
		if(root == NULL) {
			root = ksi->roots_valid[j] ? ksi->roots_hash[j] : NULL;
			ksi->roots_valid[j] = 0;
		} else if(ksi->roots_valid[j]) {
			rootDel = root;
			ret = hash_node_ksi(ksi, &root, ksi->roots_hash[j], rootDel, j+2);
			ksi->roots_valid[j] = 0;
			KSI_DataHash_free(ksi->roots_hash[j]);
			KSI_DataHash_free(rootDel);
			if(ret != 0) goto done; /* checks hash_node_ksi() result! */
		}
	}
	if((ret = signIt(ksi, root)) != 0) goto done;

	KSI_DataHash_free(root);
done:
	ksi->bInBlk = 0;
	return ret;
}

int
rsksiSetAggregator(rsksictx ctx, char *uri, char *loginid, char *key)
{
	int r;
	r = KSI_CTX_setAggregator(ctx->ksi_ctx, uri, loginid, key);
	if(r != KSI_OK) {
		reportKSIAPIErr(ctx, NULL, "KSI_CTX_setAggregator", r);
	}
	return r;
}
