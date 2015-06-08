/* librsksi.c - rsyslog's KSI support library
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
#include "librsksi.h"

typedef unsigned char uchar;
#ifndef VERSION
#define VERSION "no-version"
#endif


static void
reportErr(rsksictx ctx, char *errmsg)
{
	if(ctx->errFunc == NULL)
		goto done;
	ctx->errFunc(ctx->usrptr, (uchar*)errmsg);
done:	return;
}

static void
reportKSIAPIErr(rsksictx ctx, ksifile gf, char *apiname, int ecode)
{
	char errbuf[4096];
	snprintf(errbuf, sizeof(errbuf), "%s[%s:%d]: %s",
		 (gf == NULL) ? (uchar*)"" : gf->sigfilename,
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
rsksiImprintFromKSI_DataHash(KSI_DataHash *hash)
{
	imprint_t *imp;
	const unsigned char *digest;
	unsigned digest_len;

	if((imp = calloc(1, sizeof(imprint_t))) == NULL) {
		goto done;
	}
	int hashID;
	KSI_DataHash_extract(hash, &hashID, &digest, &digest_len); // TODO: error check
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

static inline ksifile
rsksifileConstruct(rsksictx ctx)
{
	ksifile gf;
	if((gf = calloc(1, sizeof(struct ksifile_s))) == NULL)
		goto done;
	gf->ctx = ctx;
	gf->hashAlg = ctx->hashAlg;
	gf->blockSizeLimit = ctx->blockSizeLimit;
	gf->bKeepRecordHashes = ctx->bKeepRecordHashes;
	gf->bKeepTreeHashes = ctx->bKeepTreeHashes;
	gf->x_prev = NULL;

done:	return gf;
}

static inline int
tlvbufPhysWrite(ksifile gf)
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

static inline int
tlvbufChkWrite(ksifile gf)
{
	if(gf->tlvIdx == sizeof(gf->tlvBuf)) {
		return tlvbufPhysWrite(gf);
	}
	return 0;
}


/* write to TLV file buffer. If buffer is full, an actual call occurs. Else
 * output is written only on flush or close.
 */
static inline int
tlvbufAddOctet(ksifile gf, int8_t octet)
{
	int r;
	r = tlvbufChkWrite(gf);
	if(r != 0) goto done;
	gf->tlvBuf[gf->tlvIdx++] = octet;
done:	return r;
}
static inline int
tlvbufAddOctetString(ksifile gf, uint8_t *octet, int size)
{
	int i, r = 0;
	for(i = 0 ; i < size ; ++i) {
		r = tlvbufAddOctet(gf, octet[i]);
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
tlvbufAddInt64(ksifile gf, uint64_t val)
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


int
tlv8Write(ksifile gf, int flags, int tlvtype, int len)
{
	int r;
	assert((flags & RSGT_TYPE_MASK) == 0);
	assert((tlvtype & RSGT_TYPE_MASK) == tlvtype);
	r = tlvbufAddOctet(gf, (flags & ~RSGT_FLAG_TLV16) | tlvtype);
	if(r != 0) goto done;
	r = tlvbufAddOctet(gf, len & 0xff);
done:	return r;
} 

int
tlv16Write(ksifile gf, int flags, int tlvtype, uint16_t len)
{
	uint16_t typ;
	int r;
	assert((flags & RSGT_TYPE_MASK) == 0);
	assert((tlvtype >> 8 & RSGT_TYPE_MASK) == (tlvtype >> 8));
	typ = ((flags | RSGT_FLAG_TLV16) << 8) | tlvtype;
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
tlvFlush(ksifile gf)
{
	return (gf->tlvIdx == 0) ? 0 : tlvbufPhysWrite(gf);
}

int
tlvWriteHash(ksifile gf, uint16_t tlvtype, KSI_DataHash *rec)
{
	unsigned tlvlen;
	int r;
	const unsigned char *digest;
	unsigned digest_len;
	KSI_DataHash_extract(rec, NULL, &digest, &digest_len); // TODO: error check
	tlvlen = digest_len;
	r = tlv16Write(gf, 0x00, tlvtype, tlvlen);
	if(r != 0) goto done;
	r = tlvbufAddOctetString(gf, (unsigned char*)digest, digest_len);
done:	return r;
}

int
tlvWriteBlockSig(ksifile gf, uchar *der, uint16_t lenDer)
{
	unsigned tlvlen;
	uint8_t tlvlenRecords;
	int r;

	tlvlenRecords = tlvbufGetInt64OctetSize(gf->nRecords);
	tlvlen  = 2 + 1 /* hash algo TLV */ +
	 	  2 + hashOutputLengthOctets(gf->hashAlg) /* iv */ +
		  2 + 1 + gf->lenBlkStrtHash /* last hash */ +
		  2 + tlvlenRecords /* rec-count */ +
		  4 + lenDer /* rfc-3161 */;
	/* write top-level TLV object (block-sig */
	r = tlv16Write(gf, 0x00, 0x0902, tlvlen);
	if(r != 0) goto done;
	/* and now write the children */
	//FIXME: flags???
	/* hash-algo */
	r = tlv8Write(gf, 0x00, 0x00, 1);
	if(r != 0) goto done;
	r = tlvbufAddOctet(gf, hashIdentifier(gf->hashAlg));
	if(r != 0) goto done;
	/* block-iv */
	r = tlv8Write(gf, 0x00, 0x01, hashOutputLengthOctets(gf->hashAlg));
	if(r != 0) goto done;
	r = tlvbufAddOctetString(gf, gf->IV, hashOutputLengthOctets(gf->hashAlg));
	if(r != 0) goto done;
	/* last-hash */
	r = tlv8Write(gf, 0x00, 0x02, gf->lenBlkStrtHash+1);
	if(r != 0) goto done;
	r = tlvbufAddOctet(gf, hashIdentifier(gf->hashAlg));
	if(r != 0) goto done;
	r = tlvbufAddOctetString(gf, gf->blkStrtHash, gf->lenBlkStrtHash);
	if(r != 0) goto done;
	/* rec-count */
	r = tlv8Write(gf, 0x00, 0x03, tlvlenRecords);
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
readStateFile(ksifile gf)
{
	int fd;
	struct rsksistatefile sf;

	fd = open((char*)gf->statefilename, O_RDONLY|O_NOCTTY|O_CLOEXEC, 0600);
	if(fd == -1) goto err;

	if(read(fd, &sf, sizeof(sf)) != sizeof(sf)) goto err;
	if(strncmp(sf.hdr, "GTSTAT10", 8)) goto err;

	gf->lenBlkStrtHash = sf.lenHash;
	gf->blkStrtHash = calloc(1, gf->lenBlkStrtHash);
	if(read(fd, gf->blkStrtHash, gf->lenBlkStrtHash)
		!= gf->lenBlkStrtHash) {
		free(gf->blkStrtHash);
		goto err;
	}
	close(fd);
return;

err:
	gf->lenBlkStrtHash = hashOutputLengthOctets(gf->hashAlg);
	gf->blkStrtHash = calloc(1, gf->lenBlkStrtHash);
}

/* persist all information that we need to re-open and append
 * to a log signature file.
 */
static void
writeStateFile(ksifile gf)
{
	int fd;
	struct rsksistatefile sf;

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
tlvClose(ksifile gf)
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
tlvOpen(ksifile gf, char *hdr, unsigned lenHdr)
{
	int r = 0;
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
		memcpy(gf->tlvBuf, hdr, lenHdr);
		gf->tlvIdx = lenHdr;
	} else {
		gf->tlvIdx = 0; /* header already present! */
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
seedIV(ksifile gf)
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
	ksifile gf;
	char fn[MAXFNAME+1];

	if((gf = rsksifileConstruct(ctx)) == NULL)
		goto done;

	snprintf(fn, sizeof(fn), "%s.gtsig", logfn);
	fn[MAXFNAME] = '\0'; /* be on save side */
	gf->sigfilename = (uchar*) strdup(fn);
	snprintf(fn, sizeof(fn), "%s.gtstate", logfn);
	fn[MAXFNAME] = '\0'; /* be on save side */
	gf->statefilename = (uchar*) strdup(fn);
	if(tlvOpen(gf, LOGSIGHDR, sizeof(LOGSIGHDR)-1) != 0) {
		reportErr(ctx, "signature file open failed");
		gf = NULL;
	}
done:	return gf;
}
 

/* returns 0 on succes, 1 if algo is unknown */
int
rsksiSetHashFunction(rsksictx ctx, char *algName)
{
	int r = 0;
	if(!strcmp(algName, "SHA2-256"))
		ctx->hashAlg = KSI_HASHALG_SHA2_256;
	else if(!strcmp(algName, "SHA2-384"))
		ctx->hashAlg = KSI_HASHALG_SHA2_384;
	else if(!strcmp(algName, "SHA2-512"))
		ctx->hashAlg = KSI_HASHALG_SHA2_512;
	else if(!strcmp(algName, "SHA1"))
		ctx->hashAlg = KSI_HASHALG_SHA1;
	else if(!strcmp(algName, "RIPEMD-160"))
		ctx->hashAlg = KSI_HASHALG_RIPEMD160;
	else if(!strcmp(algName, "SHA2-224"))
		ctx->hashAlg = KSI_HASHALG_SHA2_224;
	else
		r = 1;
	return r;
}

int
rsksifileDestruct(ksifile gf)
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
	free(gf->blkStrtHash);
	rsksiimprintDel(gf->x_prev);
	free(gf);
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
sigblkInit(ksifile gf)
{
	if(gf == NULL) goto done;
	seedIV(gf);
	memset(gf->roots_valid, 0, sizeof(gf->roots_valid)/sizeof(char));
	gf->nRoots = 0;
	gf->nRecords = 0;
	gf->bInBlk = 1;
done:	return;
}


/* concat: add IV to buffer */
static inline void
bufAddIV(ksifile gf, uchar *buf, size_t *len)
{
	memcpy(buf+*len, gf->IV, hashOutputLengthOctets(gf->hashAlg));
	*len += sizeof(gf->IV);
}


/* concat: add imprint to buffer */
static inline void
bufAddImprint(ksifile gf, uchar *buf, size_t *len, imprint_t *imp)
{
	if(imp == NULL) {
	/* TODO: how to get the REAL HASH ID? --> add field? */
		buf[*len] = hashIdentifier(gf->hashAlg);
		++(*len);
		memcpy(buf+*len, gf->blkStrtHash, gf->lenBlkStrtHash);
		*len += gf->lenBlkStrtHash;
	} else {
		buf[*len] = imp->hashID;
		++(*len);
		memcpy(buf+*len, imp->data, imp->len);
		*len += imp->len;
	}
}
/* concat: add hash to buffer */
static inline void
bufAddHash(ksifile gf, uchar *buf, size_t *len, KSI_DataHash *hash)
{
	const unsigned char *digest;
	unsigned digest_len;
	KSI_DataHash_extract(hash, NULL, &digest, &digest_len); // TODO: error check
	buf[*len] = hashIdentifier(gf->hashAlg);
	++(*len);
	memcpy(buf+*len, digest, digest_len);
	*len += digest_len;
}
/* concat: add tree level to buffer */
static inline void
bufAddLevel(uchar *buf, size_t *len, uint8_t level)
{
	memcpy(buf+*len, &level, sizeof(level));
	*len += sizeof(level);
}


int
hash_m(ksifile gf, KSI_DataHash **m)
{
	int rgt;
	uchar concatBuf[16*1024];
	size_t len = 0;
	int r = 0;

	bufAddImprint(gf, concatBuf, &len, gf->x_prev);
	bufAddIV(gf, concatBuf, &len);
	rgt = KSI_DataHash_create(gf->ctx->ksi_ctx, concatBuf, len, gf->hashAlg, m);
	if(rgt != KSI_OK) {
		reportKSIAPIErr(gf->ctx, gf, "KSI_DataHash_create", rgt);
		r = RSGTE_HASH_CREATE;
		goto done;
	}
done:	return r;
}

int
hash_r(ksifile gf, KSI_DataHash **r, const uchar *rec, const size_t len)
{
	int ret = 0, rgt;
	rgt = KSI_DataHash_create(gf->ctx->ksi_ctx, rec, len, gf->hashAlg, r);
	if(rgt != KSI_OK) {
		reportKSIAPIErr(gf->ctx, gf, "KSI_DataHash_create", rgt);
		ret = RSGTE_HASH_CREATE;
		goto done;
	}
done:	return ret;
}


int
hash_node(ksifile gf, KSI_DataHash **node, KSI_DataHash *m, KSI_DataHash *rec,
          uint8_t level)
{
	int r = 0, rgt;
	uchar concatBuf[16*1024];
	size_t len = 0;

	bufAddHash(gf, concatBuf, &len, m);
	bufAddHash(gf, concatBuf, &len, rec);
	bufAddLevel(concatBuf, &len, level);
	rgt = KSI_DataHash_create(gf->ctx->ksi_ctx, concatBuf, len, gf->hashAlg, node);
	if(rgt != KSI_OK) {
		reportKSIAPIErr(gf->ctx, gf, "KSI_DataHash_create", rgt);
		r = RSGTE_HASH_CREATE;
		goto done;
	}
done:	return r;
}


int
sigblkAddRecord(ksifile gf, const uchar *rec, const size_t len)
{
	KSI_DataHash *x; /* current hash */
	KSI_DataHash *m, *r, *t, *t_del;
	uint8_t j;
	int ret = 0;

	if(gf == NULL || gf->disabled) goto done;
	if((ret = hash_m(gf, &m)) != 0) goto done;
	if((ret = hash_r(gf, &r, rec, len)) != 0) goto done;
	if(gf->bKeepRecordHashes)
		tlvWriteHash(gf, 0x0900, r);
	if((ret = hash_node(gf, &x, m, r, 1)) != 0) goto done; /* hash leaf */
	/* persists x here if Merkle tree needs to be persisted! */
	if(gf->bKeepTreeHashes)
		tlvWriteHash(gf, 0x0901, x);
	rsksiimprintDel(gf->x_prev);
	gf->x_prev = rsksiImprintFromKSI_DataHash(x);
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
			KSI_DataHash_free(gf->roots_hash[j]);
			KSI_DataHash_free(t_del);
			if(ret != 0) goto done;
			if(gf->bKeepTreeHashes)
				tlvWriteHash(gf, 0x0901, t);
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
	KSI_DataHash_free(m);
	KSI_DataHash_free(r);

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
timestampIt(ksifile gf, KSI_DataHash *hash)
{
	unsigned char *der = NULL;
	unsigned lenDer;
	int r = KSI_OK;
	int ret = 0;
	KSI_Signature *sig = NULL;

	/* Get the timestamp. */
	r = KSI_createSignature(gf->ctx->ksi_ctx, hash, &sig);
	if(r != KSI_OK) {
		reportKSIAPIErr(gf->ctx, gf, "KSI_createSignature", r);
		ret = 1;
		goto done;
	}

	/* Encode timestamp. */
	r = KSI_Signature_serialize(sig, &der, &lenDer);
	if(r != KSI_OK) {
		reportKSIAPIErr(gf->ctx, gf, "KSI_Signature_serialize", r);
		ret = 1;
		goto done;
	}

	tlvWriteBlockSig(gf, der, lenDer);

done:
	KSI_free(der);
	return ret;
}


int
sigblkFinish(ksifile gf)
{
	KSI_DataHash *root, *rootDel;
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
			KSI_DataHash_free(gf->roots_hash[j]);
			KSI_DataHash_free(rootDel);
			if(ret != 0) goto done; /* checks hash_node() result! */
		}
	}
	if((ret = timestampIt(gf, root)) != 0) goto done;

	KSI_DataHash_free(root);
	free(gf->blkStrtHash);
	gf->lenBlkStrtHash = gf->x_prev->len;
	gf->blkStrtHash = malloc(gf->lenBlkStrtHash);
	memcpy(gf->blkStrtHash, gf->x_prev->data, gf->x_prev->len);
done:
	gf->bInBlk = 0;
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
