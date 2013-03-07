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

static void
outputhash(GTDataHash *hash)
{
	unsigned i;
	for(i = 0 ; i < hash->digest_length ; ++i)
		printf("%2.2x", hash->digest[i]);
	printf("\n");
}


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


static inline void
tlvbufPhysWrite(gtctx ctx)
{
	ssize_t lenBuf;
	ssize_t iTotalWritten;
	ssize_t iWritten;
	char *pWriteBuf;

	lenBuf = ctx->tlvIdx;
	pWriteBuf = ctx->tlvBuf;
	iTotalWritten = 0;
	do {
		iWritten = write(ctx->fd, pWriteBuf, lenBuf);
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
	ctx->tlvIdx = 0;
}

static inline void
tlvbufChkWrite(gtctx ctx)
{
	if(ctx->tlvIdx == sizeof(ctx->tlvBuf)) {
		tlvbufPhysWrite(ctx);
	}
}


/* write to TLV file buffer. If buffer is full, an actual call occurs. Else
 * output is written only on flush or close.
 */
static inline void
tlvbufAddOctet(gtctx ctx, int8_t octet)
{
	tlvbufChkWrite(ctx);
	ctx->tlvBuf[ctx->tlvIdx++] = octet;
}
static inline void
tlvbufAddOctetString(gtctx ctx, uint8_t *octet, int size)
{
	int i;
	for(i = 0 ; i < size ; ++i)
		tlvbufAddOctet(ctx, octet[i]);
}
static inline void
tlvbufAddInt32(gtctx ctx, uint32_t val)
{
	tlvbufAddOctet(ctx, (val >> 24) & 0xff);
	tlvbufAddOctet(ctx, (val >> 16) & 0xff);
	tlvbufAddOctet(ctx, (val >>  8) & 0xff);
	tlvbufAddOctet(ctx,  val        & 0xff);
}
static inline void
tlvbufAddInt64(gtctx ctx, uint64_t val)
{
	tlvbufAddInt32(ctx, (val >> 32) & 0xffffffff);
	tlvbufAddInt32(ctx,  val        & 0xffffffff);
}


void
tlv8Write(gtctx ctx, int flags, int tlvtype, int len)
{
	tlvbufAddOctet(ctx, (flags << 5)|tlvtype);
	tlvbufAddOctet(ctx, len & 0xff);
} 

void
tlv16Write(gtctx ctx, int flags, int tlvtype, uint16_t len)
{
	uint16_t typ;
	typ = ((flags|1) << 13)|tlvtype;
	tlvbufAddOctet(ctx, typ >> 8);
	tlvbufAddOctet(ctx, typ & 0xff);
	tlvbufAddOctet(ctx, (len >> 8) & 0xff);
	tlvbufAddOctet(ctx, len & 0xff);
} 

void
tlvFlush(gtctx ctx)
{
	if(ctx->tlvIdx != 0)
		tlvbufPhysWrite(ctx);
}

void
tlvWriteBlockSig(gtctx ctx, uchar *der, uint16_t lenDer)
{
	unsigned tlvlen;

	tlvlen  = 2 + 1 /* hash algo TLV */ +
	 	  2 + hashOutputLengthOctets(ctx->hashAlg) /* iv */ +
		  2 + 1 + ctx->lenBlkStrtHash /* last hash */ +
		  2 + 8 /* rec-count (64 bit integer) */ +
		  4 + lenDer /* rfc-3161 */;
printf("TTTT: tlvlen %u, lenDer %u\n", tlvlen, lenDer);
	/* write top-level TLV object (block-sig */
	tlv16Write(ctx, 0x00, 0x0902, tlvlen);
	/* and now write the children */
	//FIXME: flags???
	/* hash-algo */
	tlv8Write(ctx, 0x00, 0x00, 1);
	tlvbufAddOctet(ctx, hashIdentifier(ctx->hashAlg));
	/* block-iv */
	tlv8Write(ctx, 0x00, 0x01, hashOutputLengthOctets(ctx->hashAlg));
	tlvbufAddOctetString(ctx, ctx->IV, hashOutputLengthOctets(ctx->hashAlg));
	/* last-hash */
	tlv8Write(ctx, 0x00, 0x02, ctx->lenBlkStrtHash+1);
	tlvbufAddOctet(ctx, hashIdentifier(ctx->hashAlg));
	tlvbufAddOctetString(ctx, ctx->blkStrtHash, ctx->lenBlkStrtHash);
	/* rec-count */
	tlv8Write(ctx, 0x00, 0x03, 8);
	tlvbufAddInt64(ctx, ctx->nRecords);
	/* rfc-3161 */
	tlv16Write(ctx, 0x00, 0x906, lenDer);
	tlvbufAddOctetString(ctx, der, lenDer);
}

/* read rsyslog log state file; if we cannot access it or the
 * contents looks invalid, we flag it as non-present (and thus
 * begin a new hash chain).
 * The context is initialized accordingly.
 */
static void
readStateFile(gtctx ctx)
{
	int fd;
	struct rsgtstatefile sf;
	int rr;

	fd = open((char*)ctx->statefilename, O_RDONLY|O_NOCTTY|O_CLOEXEC, 0600);
	if(fd == -1) goto err;

	if(read(fd, &sf, sizeof(sf)) != sizeof(sf)) goto err;
	if(strncmp(sf.hdr, "GTSTAT10", 8)) goto err;

	ctx->lenBlkStrtHash = hashOutputLengthOctets(sf.lenHash);
	ctx->blkStrtHash = calloc(1, ctx->lenBlkStrtHash);
	if((rr=read(fd, ctx->blkStrtHash, ctx->lenBlkStrtHash))
		!= ctx->lenBlkStrtHash) {
		free(ctx->blkStrtHash);
		goto err;
	}
return;

err:
	ctx->lenBlkStrtHash = hashOutputLengthOctets(ctx->hashAlg);
	ctx->blkStrtHash = calloc(1, ctx->lenBlkStrtHash);
}

/* persist all information that we need to re-open and append
 * to a log signature file.
 */
static void
writeStateFile(gtctx ctx)
{
	int fd;
	struct rsgtstatefile sf;

	fd = open((char*)ctx->statefilename,
		       O_WRONLY|O_CREAT|O_TRUNC|O_NOCTTY|O_CLOEXEC, 0600);
	if(fd == -1)
		goto done;

	memcpy(sf.hdr, "GTSTAT10", 8);
	sf.hashID = hashIdentifier(ctx->hashAlg);
	sf.lenHash = ctx->x_prev->digest_length;
	write(fd, &sf, sizeof(sf));
	write(fd, ctx->x_prev->digest, ctx->x_prev->digest_length);
	close(fd);
done:	return;
}


void tlvClose(gtctx ctx)
{
	tlvFlush(ctx);
	close(ctx->fd);
	ctx->fd = -1;
	writeStateFile(ctx);
}


/* note: if file exists, the last hash for chaining must
 * be read from file.
 */
void tlvOpen(gtctx ctx, char *hdr, unsigned lenHdr)
{
	ctx->fd = open((char*)ctx->sigfilename,
		       O_WRONLY|O_APPEND|O_NOCTTY|O_CLOEXEC, 0600);
	if(ctx->fd == -1) {
		/* looks like we need to create a new file */
		ctx->fd = open((char*)ctx->sigfilename,
			       O_WRONLY|O_CREAT|O_NOCTTY|O_CLOEXEC, 0600);
		// FIXME: check fd == -1
		memcpy(ctx->tlvBuf, hdr, lenHdr);
		ctx->tlvIdx = lenHdr;
	} else {
		ctx->tlvIdx = 0; /* header already present! */
	}
	/* we now need to obtain the last previous hash, so that
	 * we can continue the hash chain.
	 */
	readStateFile(ctx);
}

/*
 * As of some Linux and security expert I spoke to, /dev/urandom
 * provides very strong random numbers, even if it runs out of
 * entropy. As far as he knew, this is save for all applications
 * (and he had good proof that I currently am not permitted to
 * reproduce). -- rgerhards, 2013-03-04
 */
void
seedIV(gtctx ctx)
{
	int hashlen;
	int fd;

	hashlen = hashOutputLengthOctets(ctx->hashAlg);
	ctx->IV = malloc(hashlen); /* do NOT zero-out! */
	/* if we cannot obtain data from /dev/urandom, we use whatever
	 * is present at the current memory location as random data. Of
	 * course, this is very weak and we should consider a different
	 * option, especially when not running under Linux (for Linux,
	 * unavailability of /dev/urandom is just a theoretic thing, it
	 * will always work...).  -- TODO -- rgerhards, 2013-03-06
	 */
	if((fd = open("/dev/urandom", O_RDONLY)) > 0) {
		read(fd, ctx->IV, hashlen);
		close(fd);
	}
}

gtctx
rsgtCtxNew(unsigned char *logfn, enum GTHashAlgorithm hashAlg)
{
	char fn[MAXFNAME+1];
	gtctx ctx;
	ctx =  calloc(1, sizeof(struct gtctx_s));
	ctx->x_prev = NULL;
	ctx->hashAlg = hashAlg;
	ctx->timestamper = strdup(
			   "http://stamper.guardtime.net/gt-signingservice");
	snprintf(fn, sizeof(fn), "%s.gtsig", logfn);
	fn[MAXFNAME] = '\0'; /* be on save side */
	ctx->sigfilename = (uchar*) strdup(fn);
	snprintf(fn, sizeof(fn), "%s.gtstate", logfn);
	fn[MAXFNAME] = '\0'; /* be on save side */
	ctx->statefilename = (uchar*) strdup(fn);
	tlvOpen(ctx, LOGSIGHDR, sizeof(LOGSIGHDR)-1);
	return ctx;
}
 
void
rsgtCtxDel(gtctx ctx)
{
	if(ctx == NULL)
		goto done;

	if(ctx->bInBlk)
		sigblkFinish(ctx);
	tlvClose(ctx);
	free(ctx->sigfilename);
	free(ctx);
	/* TODO: persist! */
done:	return;
}

/* new sigblk is initialized, but maybe in existing ctx */
void
sigblkInit(gtctx ctx)
{
	seedIV(ctx);
	memset(ctx->roots_valid, 0, sizeof(ctx->roots_valid)/sizeof(char));
	ctx->nRoots = 0;
	ctx->nRecords = 0;
	ctx->bInBlk = 1;
}


/* concat: add IV to buffer */
static inline void
bufAddIV(gtctx ctx, uchar *buf, size_t *len)
{
	memcpy(buf+*len, &ctx->IV, sizeof(ctx->IV));
	*len += sizeof(ctx->IV);
}


/* concat: add hash to buffer */
static inline void
bufAddHash(gtctx ctx, uchar *buf, size_t *len, GTDataHash *hash)
{
	if(hash == NULL) {
		memcpy(buf+*len, ctx->blkStrtHash, ctx->lenBlkStrtHash);
		*len += ctx->lenBlkStrtHash;
	} else {
		memcpy(buf+*len, hash->digest, hash->digest_length);
		*len += hash->digest_length;
	}
}
/* concat: add tree level to buffer */
static inline void
bufAddLevel(uchar *buf, size_t *len, int level)
{
	memcpy(buf+*len, &level, sizeof(level));
	*len += sizeof(level);
}


static void
hash_m(gtctx ctx, GTDataHash **m)
{
#warning Overall: check GT API return states!
	// m = hash(concat(ctx->x_prev, IV));
	uchar concatBuf[16*1024];
	size_t len = 0;

	bufAddHash(ctx, concatBuf, &len, ctx->x_prev);
	bufAddIV(ctx, concatBuf, &len);
	GTDataHash_create(ctx->hashAlg, concatBuf, len, m);
}

static void
hash_r(gtctx ctx, GTDataHash **r, const uchar *rec, const size_t len)
{
	// r = hash(canonicalize(rec));
	GTDataHash_create(ctx->hashAlg, rec, len, r);
}


static void
hash_node(gtctx ctx, GTDataHash **node, GTDataHash *m, GTDataHash *r, int level)
{
	// x = hash(concat(m, r, 0)); /* hash leaf */
	uchar concatBuf[16*1024];
	size_t len = 0;

	bufAddHash(ctx, concatBuf, &len, m);
	bufAddHash(ctx, concatBuf, &len, r);
	bufAddLevel(concatBuf, &len, level);
	GTDataHash_create(ctx->hashAlg, concatBuf, len, node);
}

void
sigblkAddRecord(gtctx ctx, const uchar *rec, const size_t len)
{
	GTDataHash *x; /* current hash */
	GTDataHash *m, *r, *t;
	int8_t j;

	hash_m(ctx, &m);
	hash_r(ctx, &r, rec, len);
	hash_node(ctx, &x, m, r, 0); /* hash leaf */
	/* persists x here if Merkle tree needs to be persisted! */
	/* add x to the forest as new leaf, update roots list */
	t = x;
	for(j = 0 ; j < ctx->nRoots ; ++j) {
		if(ctx->roots_valid[j] == 0) {
			GTDataHash_free(ctx->roots_hash[j]);
			ctx->roots_hash[j] = t;
			ctx->roots_valid[j] = 1;
			t = NULL;
		} else if(t != NULL) {
			/* hash interim node */
			hash_node(ctx, &t, ctx->roots_hash[j], t, j+1);
			ctx->roots_valid[j] = 0;
		}
	}
	if(t != NULL) {
		/* new level, append "at the top" */
		ctx->roots_hash[ctx->nRoots] = t;
		++ctx->nRoots;
		assert(ctx->nRoots < MAX_ROOTS);
		t = NULL;
	}
	ctx->x_prev = x; /* single var may be sufficient */
	++ctx->nRecords;

	/* cleanup */
	/* note: x is freed later as part of roots cleanup */
	GTDataHash_free(m);
	GTDataHash_free(r);
}

static void
timestampIt(gtctx ctx, GTDataHash *hash)
{
	unsigned char *der;
	size_t lenDer;
	int r = GT_OK;
	GTTimestamp *timestamp = NULL;

	/* Get the timestamp. */
	r = GTHTTP_createTimestampHash(hash, ctx->timestamper, &timestamp);

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

	tlvWriteBlockSig(ctx, der, lenDer);

done:
	GT_free(der);
	GTTimestamp_free(timestamp);
}


void
sigblkFinish(gtctx ctx)
{
	GTDataHash *root, *rootDel;
	int8_t j;

	root = NULL;
	for(j = 0 ; j < ctx->nRoots ; ++j) {
		if(root == NULL) {
			root = ctx->roots_hash[j];
			ctx->roots_valid[j] = 0; /* guess this is redundant with init, maybe del */
		} else if(ctx->roots_valid[j]) {
			rootDel = root;
			hash_node(ctx, &root, ctx->roots_hash[j], root, j+1);
			ctx->roots_valid[j] = 0; /* guess this is redundant with init, maybe del */
			GTDataHash_free(rootDel);
		}
	}
	/* persist root value here (callback?) */
printf("root hash is:\n"); outputhash(root);
	timestampIt(ctx, root);
	ctx->bInBlk = 0;
}
