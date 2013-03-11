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

#include "librsgt.h"

typedef unsigned char uchar;
#ifndef VERSION
#define VERSION "no-version"
#endif


void
rsgtInit(char *usragent)
{
	int ret = GT_OK;

	ret = GT_init();
	if(ret != GT_OK) {
		fprintf(stderr, "GT_init() failed: %d (%s)\n",
				ret, GT_getErrorString(ret));
		goto done;
	}
	ret = GTHTTP_init(usragent, 1);
	if(ret != GT_OK) {
		fprintf(stderr, "GTHTTP_init() failed: %d (%s)\n",
				ret, GTHTTP_getErrorString(ret));
		goto done;
	}
done:	return;
}

void
rsgtExit(void)
{
	GTHTTP_finalize();
	GT_finalize();
}


static inline gtfile
rsgtfileConstruct(gtctx ctx)
{
	gtfile gf;
	if((gf = calloc(1, sizeof(struct gtfile_s))) == NULL)
		goto done;
	gf->ctx = ctx;
	gf->hashAlg = ctx->hashAlg;
	gf->bKeepRecordHashes = ctx->bKeepRecordHashes;
	gf->bKeepTreeHashes = ctx->bKeepTreeHashes;
	gf->x_prev = NULL;

done:	return gf;
}

static inline void
tlvbufPhysWrite(gtfile gf)
{
	ssize_t lenBuf;
	ssize_t iTotalWritten;
	ssize_t iWritten;
	char *pWriteBuf;

	lenBuf = gf->tlvIdx;
	pWriteBuf = gf->tlvBuf;
	iTotalWritten = 0;
	do {
		iWritten = write(gf->fd, pWriteBuf, lenBuf);
		if(iWritten < 0) {
			//char errStr[1024];
			int err = errno;
			iWritten = 0; /* we have written NO bytes! */
		/*	rs_strerror_r(err, errStr, sizeof(errStr));
			DBGPRINTF("log file (%d) write error %d: %s\n", pThis->fd, err, errStr);
			*/
			if(err == EINTR) {
				/*NO ERROR, just continue */;
			} else {
				goto finalize_it; //ABORT_FINALIZE(RS_RET_IO_ERROR);
				/* FIXME: flag error */
			}
	 	} 
		/* advance buffer to next write position */
		iTotalWritten += iWritten;
		lenBuf -= iWritten;
		pWriteBuf += iWritten;
	} while(lenBuf > 0);	/* Warning: do..while()! */

finalize_it:
	gf->tlvIdx = 0;
}

static inline void
tlvbufChkWrite(gtfile gf)
{
	if(gf->tlvIdx == sizeof(gf->tlvBuf)) {
		tlvbufPhysWrite(gf);
	}
}


/* write to TLV file buffer. If buffer is full, an actual call occurs. Else
 * output is written only on flush or close.
 */
static inline void
tlvbufAddOctet(gtfile gf, int8_t octet)
{
	tlvbufChkWrite(gf);
	gf->tlvBuf[gf->tlvIdx++] = octet;
}
static inline void
tlvbufAddOctetString(gtfile gf, uint8_t *octet, int size)
{
	int i;
	for(i = 0 ; i < size ; ++i)
		tlvbufAddOctet(gf, octet[i]);
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
static inline void
tlvbufAddInt64(gtfile gf, uint64_t val)
{
	uint8_t doWrite = 0;
	if(val >> 56)
		tlvbufAddOctet(gf, (val >> 56) & 0xff), doWrite = 1;
	if(doWrite || ((val >> 48) & 0xff))
		tlvbufAddOctet(gf, (val >> 48) & 0xff), doWrite = 1;
	if(doWrite || ((val >> 40) & 0xff))
		tlvbufAddOctet(gf, (val >> 40) & 0xff), doWrite = 1;
	if(doWrite || ((val >> 32) & 0xff))
		tlvbufAddOctet(gf, (val >> 32) & 0xff), doWrite = 1;
	if(doWrite || ((val >> 24) & 0xff))
		tlvbufAddOctet(gf, (val >> 24) & 0xff), doWrite = 1;
	if(doWrite || ((val >> 16) & 0xff))
		tlvbufAddOctet(gf, (val >> 16) & 0xff), doWrite = 1;
	if(doWrite || ((val >> 8) & 0xff))
		tlvbufAddOctet(gf, (val >>  8) & 0xff), doWrite = 1;
	tlvbufAddOctet(gf, val & 0xff);
}


void
tlv8Write(gtfile gf, int flags, int tlvtype, int len)
{
	tlvbufAddOctet(gf, (flags << 5)|tlvtype);
	tlvbufAddOctet(gf, len & 0xff);
} 

void
tlv16Write(gtfile gf, int flags, int tlvtype, uint16_t len)
{
	uint16_t typ;
	typ = ((flags|1) << 13)|tlvtype;
	tlvbufAddOctet(gf, typ >> 8);
	tlvbufAddOctet(gf, typ & 0xff);
	tlvbufAddOctet(gf, (len >> 8) & 0xff);
	tlvbufAddOctet(gf, len & 0xff);
} 

void
tlvFlush(gtfile gf)
{
	if(gf->tlvIdx != 0)
		tlvbufPhysWrite(gf);
}

void
tlvWriteHash(gtfile gf, uint16_t tlvtype, GTDataHash *r)
{
	unsigned tlvlen;

	tlvlen = 1 + r->digest_length;
	tlv16Write(gf, 0x00, tlvtype, tlvlen);
	tlvbufAddOctet(gf, hashIdentifier(gf->hashAlg));
	tlvbufAddOctetString(gf, r->digest, r->digest_length);
}

void
tlvWriteBlockSig(gtfile gf, uchar *der, uint16_t lenDer)
{
	unsigned tlvlen;
	uint8_t tlvlenRecords;

	tlvlenRecords = tlvbufGetInt64OctetSize(gf->nRecords);
	tlvlen  = 2 + 1 /* hash algo TLV */ +
	 	  2 + hashOutputLengthOctets(gf->hashAlg) /* iv */ +
		  2 + 1 + gf->lenBlkStrtHash /* last hash */ +
		  2 + tlvlenRecords /* rec-count */ +
		  4 + lenDer /* rfc-3161 */;
	/* write top-level TLV object (block-sig */
	tlv16Write(gf, 0x00, 0x0902, tlvlen);
	/* and now write the children */
	//FIXME: flags???
	/* hash-algo */
	tlv8Write(gf, 0x00, 0x00, 1);
	tlvbufAddOctet(gf, hashIdentifier(gf->hashAlg));
	/* block-iv */
	tlv8Write(gf, 0x00, 0x01, hashOutputLengthOctets(gf->hashAlg));
	tlvbufAddOctetString(gf, gf->IV, hashOutputLengthOctets(gf->hashAlg));
	/* last-hash */
	tlv8Write(gf, 0x00, 0x02, gf->lenBlkStrtHash+1);
	tlvbufAddOctet(gf, hashIdentifier(gf->hashAlg));
	tlvbufAddOctetString(gf, gf->blkStrtHash, gf->lenBlkStrtHash);
	/* rec-count */
	tlv8Write(gf, 0x00, 0x03, tlvlenRecords);
	tlvbufAddInt64(gf, gf->nRecords);
	/* rfc-3161 */
	tlv16Write(gf, 0x00, 0x906, lenDer);
	tlvbufAddOctetString(gf, der, lenDer);
}

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

	gf->lenBlkStrtHash = sf.lenHash;
	gf->blkStrtHash = calloc(1, gf->lenBlkStrtHash);
	if(read(fd, gf->blkStrtHash, gf->lenBlkStrtHash)
		!= gf->lenBlkStrtHash) {
		free(gf->blkStrtHash);
		goto err;
	}
return;

err:
	gf->lenBlkStrtHash = hashOutputLengthOctets(gf->hashAlg);
	gf->blkStrtHash = calloc(1, gf->lenBlkStrtHash);
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
	sf.lenHash = gf->x_prev->digest_length;
	write(fd, &sf, sizeof(sf));
	write(fd, gf->x_prev->digest, gf->x_prev->digest_length);
	close(fd);
done:	return;
}


void tlvClose(gtfile gf)
{
	tlvFlush(gf);
	close(gf->fd);
	gf->fd = -1;
	writeStateFile(gf);
}


/* note: if file exists, the last hash for chaining must
 * be read from file.
 */
void tlvOpen(gtfile gf, char *hdr, unsigned lenHdr)
{
	gf->fd = open((char*)gf->sigfilename,
		       O_WRONLY|O_APPEND|O_NOCTTY|O_CLOEXEC, 0600);
	if(gf->fd == -1) {
		/* looks like we need to create a new file */
		gf->fd = open((char*)gf->sigfilename,
			       O_WRONLY|O_CREAT|O_NOCTTY|O_CLOEXEC, 0600);
		// FIXME: check fd == -1
		memcpy(gf->tlvBuf, hdr, lenHdr);
		gf->tlvIdx = lenHdr;
	} else {
		gf->tlvIdx = 0; /* header already present! */
	}
	/* we now need to obtain the last previous hash, so that
	 * we can continue the hash chain.
	 */
	readStateFile(gf);
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
		read(fd, gf->IV, hashlen);
		close(fd);
	}
}

gtctx
rsgtCtxNew(void)
{
	gtctx ctx;
	ctx = calloc(1, sizeof(struct gtctx_s));
	ctx->hashAlg = GT_HASHALG_SHA256;
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
	tlvOpen(gf, LOGSIGHDR, sizeof(LOGSIGHDR)-1);
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

void
rsgtfileDestruct(gtfile gf)
{
	if(gf == NULL)
		goto done;

	if(gf->bInBlk)
		sigblkFinish(gf);
	tlvClose(gf);
	free(gf->sigfilename);
	free(gf);
done:	return;
}

void
rsgtCtxDel(gtctx ctx)
{
	if(ctx != NULL)
		free(ctx);
}

/* new sigblk is initialized, but maybe in existing ctx */
void
sigblkInit(gtfile gf)
{
	seedIV(gf);
	memset(gf->roots_valid, 0, sizeof(gf->roots_valid)/sizeof(char));
	gf->nRoots = 0;
	gf->nRecords = 0;
	gf->bInBlk = 1;
}


/* concat: add IV to buffer */
static inline void
bufAddIV(gtfile gf, uchar *buf, size_t *len)
{
	memcpy(buf+*len, &gf->IV, sizeof(gf->IV));
	*len += sizeof(gf->IV);
}


/* concat: add hash to buffer */
static inline void
bufAddHash(gtfile gf, uchar *buf, size_t *len, GTDataHash *hash)
{
	if(hash == NULL) {
	// TODO: how to get the REAL HASH ID? --> add field!
		buf[*len] = hashIdentifier(gf->hashAlg);
		++(*len);
		memcpy(buf+*len, gf->blkStrtHash, gf->lenBlkStrtHash);
		*len += gf->lenBlkStrtHash;
	} else {
		buf[*len] = hashIdentifier(gf->hashAlg);
		++(*len);
		memcpy(buf+*len, hash->digest, hash->digest_length);
		*len += hash->digest_length;
	}
}
/* concat: add tree level to buffer */
static inline void
bufAddLevel(uchar *buf, size_t *len, uint8_t level)
{
	memcpy(buf+*len, &level, sizeof(level));
	*len += sizeof(level);
}


static void
hash_m(gtfile gf, GTDataHash **m)
{
#warning Overall: check GT API return states!
	// m = hash(concat(gf->x_prev, IV));
	uchar concatBuf[16*1024];
	size_t len = 0;

	bufAddHash(gf, concatBuf, &len, gf->x_prev);
	bufAddIV(gf, concatBuf, &len);
	GTDataHash_create(gf->hashAlg, concatBuf, len, m);
}

static inline void
hash_r(gtfile gf, GTDataHash **r, const uchar *rec, const size_t len)
{
	// r = hash(canonicalize(rec));
	GTDataHash_create(gf->hashAlg, rec, len, r);
}


static void
hash_node(gtfile gf, GTDataHash **node, GTDataHash *m, GTDataHash *r,
          uint8_t level)
{
	// x = hash(concat(m, r, 0)); /* hash leaf */
	uchar concatBuf[16*1024];
	size_t len = 0;

	bufAddHash(gf, concatBuf, &len, m);
	bufAddHash(gf, concatBuf, &len, r);
	bufAddLevel(concatBuf, &len, level);
	GTDataHash_create(gf->hashAlg, concatBuf, len, node);
}

void
sigblkAddRecord(gtfile gf, const uchar *rec, const size_t len)
{
	GTDataHash *x; /* current hash */
	GTDataHash *m, *r, *t;
	uint8_t j;

	hash_m(gf, &m);
	hash_r(gf, &r, rec, len);
	if(gf->bKeepRecordHashes)
		tlvWriteHash(gf, 0x0900, r);
	hash_node(gf, &x, m, r, 1); /* hash leaf */
	/* persists x here if Merkle tree needs to be persisted! */
	if(gf->bKeepTreeHashes)
		tlvWriteHash(gf, 0x0901, x);
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
			hash_node(gf, &t, gf->roots_hash[j], t, j+2);
			gf->roots_valid[j] = 0;
			GTDataHash_free(gf->roots_hash[j]);
			// TODO: check if this is correct location (paper!)
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
	gf->x_prev = x; /* single var may be sufficient */
	++gf->nRecords;

	/* cleanup */
	/* note: x is freed later as part of roots cleanup */
	GTDataHash_free(m);
	GTDataHash_free(r);

	if(gf->nRecords == gf->blockSizeLimit) {
		sigblkFinish(gf);
		sigblkInit(gf);
	}
}

static void
timestampIt(gtfile gf, GTDataHash *hash)
{
	unsigned char *der;
	size_t lenDer;
	int r = GT_OK;
	GTTimestamp *timestamp = NULL;

	/* Get the timestamp. */
	r = GTHTTP_createTimestampHash(hash, gf->ctx->timestamper, &timestamp);

	if(r != GT_OK) {
		fprintf(stderr, "GTHTTP_createTimestampHash() failed: %d (%s)\n",
				r, GTHTTP_getErrorString(r));
		goto done;
	}

	/* Encode timestamp. */
	r = GTTimestamp_getDEREncoded(timestamp, &der, &lenDer);
	if(r != GT_OK) {
		fprintf(stderr, "GTTimestamp_getDEREncoded() failed: %d (%s)\n",
				r, GT_getErrorString(r));
		goto done;
	}

	tlvWriteBlockSig(gf, der, lenDer);

done:
	GT_free(der);
	GTTimestamp_free(timestamp);
}


void
sigblkFinish(gtfile gf)
{
	GTDataHash *root, *rootDel;
	int8_t j;

	if(gf->nRecords == 0)
		goto done;

	root = NULL;
	for(j = 0 ; j < gf->nRoots ; ++j) {
		if(root == NULL) {
			root = gf->roots_hash[j];
			gf->roots_valid[j] = 0; /* guess this is redundant with init, maybe del */
		} else if(gf->roots_valid[j]) {
			rootDel = root;
			hash_node(gf, &root, gf->roots_hash[j], root, j+2);
			gf->roots_valid[j] = 0; /* guess this is redundant with init, maybe del */
			GTDataHash_free(rootDel);
		}
	}
	timestampIt(gf, root);

	free(gf->blkStrtHash);
	gf->lenBlkStrtHash = gf->x_prev->digest_length;
	gf->blkStrtHash = malloc(gf->lenBlkStrtHash);
	memcpy(gf->blkStrtHash, gf->x_prev->digest, gf->lenBlkStrtHash);
done:
	gf->bInBlk = 0;
}
