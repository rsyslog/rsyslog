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

#include <gt_base.h>
#include <gt_http.h>

#include "librsgt.h"

typedef unsigned char uchar;
#ifndef VERSION
#define VERSION "no-version"
#endif

static void
outputhash(GTDataHash *hash)
{
	int i;
	for(i = 0 ; i < hash->digest_length ; ++i)
		printf("%2.2x", hash->digest[i]);
	printf("\n");
}



void
rsgtInit(char *usragent)
{
	int ret = GT_OK;

	srand(time(NULL) * 7); /* see comments in seedIV() */

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
	fprintf(stderr, "emu: writing TLV file!\n");
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
tlvbufAddOctetString(gtctx ctx, int8_t *octet, int size)
{
	int i;
	for(i = 0 ; i < size ; ++i)
		tlvbufAddOctet(ctx, octet[i]);
}
static inline void
tlvbufAddInteger(gtctx ctx, uint32_t val)
{
	tlvbufAddOctet(ctx, (val >> 24) & 0xff);
	tlvbufAddOctet(ctx, (val >> 16) & 0xff);
	tlvbufAddOctet(ctx, (val >>  8) & 0xff);
	tlvbufAddOctet(ctx,  val        & 0xff);
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
	tlvbufAddOctet(ctx, ((flags|1) << 13)|tlvtype);
	tlvbufAddOctet(ctx, (len >> 8) & 0xff);
	tlvbufAddOctet(ctx, len & 0xff);
} 

void
tlvFlush(gtctx ctx)
{
	if(ctx->tlvIdx != 0)
		tlvbufPhysWrite(ctx);
}

void tlvClose(gtctx ctx)
{
	tlvFlush(ctx);
	fprintf(stderr, "emu: close tlv file\n");
}

/* note: if file exists, the last hash for chaining must
 * be read from file.
 */
void tlvOpen(gtctx ctx)
{
	fprintf(stderr, "emu: open tlv file\n");
	ctx->tlvIdx = 0;
}

void
seedIV(gtctx ctx)
{
	/* FIXME: this currently is "kindergarten cryptography" - use a
	 * sufficiently strong PRNG instead! Just a PoC so far! Do NOT
	 * use in production!!!
	 * As of some Linux and security expert I spoke to, /dev/urandom
	 * provides very strong random numbers, even if it runs out of
	 * entropy. As far as he knew, this is save for all applications
	 * (and he had good proof that I currently am not permitted to
	 * reproduce). -- rgerhards, 2013-03-04
	 */
	ctx->IV = rand() * 1000037;
}

gtctx
rsgtCtxNew(void, char *logfilename)
{
	gtctx ctx;
	ctx =  calloc(1, sizeof(struct gtctx_s));
	ctx->logfilename = strdup(logfilename);
	tlvOpen(ctx);
	return ctx;
}

void
rsgtCtxDel(gtctx ctx)
{
	if(ctx == NULL)
		goto done;

	tlvClose(ctx);
	free(ctx->logfilename);
	free(ctx);
	/* TODO: persist! */
done:	return;
}

/* new sigblk is initialized, but maybe in existing ctx */
void
sigblkInit(gtctx ctx)
{
	seedIV(ctx);
//	if(ctx->x_prev == NULL) {
		ctx->x_prev = NULL;
		/* FIXME: do the real thing - or in a later step as currently? */
//	}
	memset(ctx->roots_valid, 0, sizeof(ctx->roots_valid)/sizeof(char));
	ctx->nRoots = 0;
	ctx->nRecords = 0;
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
		memset(buf+*len, 0, 32); /* FIXME: depends on hash function! */
		*len += 32;
	} else {
		memcpy(buf+*len, hash->digest, hash->digest_length);
		*len += hash->digest_length;
	}
}
/* concat: add tree level to buffer */
static inline void
bufAddLevel(gtctx ctx, uchar *buf, size_t *len, int level)
{
	memcpy(buf+*len, &level, sizeof(level));
	*len += sizeof(level);
}


static void
hash_m(gtctx ctx, GTDataHash **m)
{
	// m = hash(concat(ctx->x_prev, IV));
	int r;
	uchar concatBuf[16*1024];
	size_t len = 0;

	bufAddHash(ctx, concatBuf, &len, ctx->x_prev);
	bufAddIV(ctx, concatBuf, &len);
	r = GTDataHash_create(GT_HASHALG_SHA256, concatBuf, len, m);
}

static void
hash_r(gtctx ctx, GTDataHash **r, const uchar *rec, const size_t len)
{
	// r = hash(canonicalize(rec));
	int ret;

	ret = GTDataHash_create(GT_HASHALG_SHA256, rec, len, r);
}


static void
hash_node(gtctx ctx, GTDataHash **node, GTDataHash *m, GTDataHash *r, int level)
{
	// x = hash(concat(m, r, 0)); /* hash leaf */
	int ret;
	uchar concatBuf[16*1024];
	size_t len = 0;

	bufAddHash(ctx, concatBuf, &len, m);
	bufAddHash(ctx, concatBuf, &len, r);
	bufAddLevel(ctx, concatBuf, &len, level);
	ret = GTDataHash_create(GT_HASHALG_SHA256, concatBuf, len, node);
}
void
sigblkAddRecord(gtctx ctx, const uchar *rec, const size_t len)
{
	GTDataHash *x; /* current hash */
	GTDataHash *m, *r, *t;
	int8_t j;
	int ret;

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
	GTDataHash_free(m);
	GTDataHash_free(r);
}

static void
timestampIt(gtctx ctx, GTDataHash *hash)
{
	int r = GT_OK;
	GTTimestamp *timestamp = NULL;
	unsigned char *der = NULL;
	char *sigFile = "logsigner.TIMESTAMP";
	size_t der_len;

	/* Get the timestamp. */
	r = GTHTTP_createTimestampHash(hash,
		"http://stamper.guardtime.net/gt-signingservice", &timestamp);

	if(r != GT_OK) {
		fprintf(stderr, "GTHTTP_createTimestampHash() failed: %d (%s)\n",
				r, GTHTTP_getErrorString(r));
		goto done;
	}

	/* Encode timestamp. */
	r = GTTimestamp_getDEREncoded(timestamp, &der, &der_len);
	if(r != GT_OK) {
		fprintf(stderr, "GTTimestamp_getDEREncoded() failed: %d (%s)\n",
				r, GT_getErrorString(r));
		goto done;
	}

	/* Save DER-encoded timestamp to file. */
	r = GT_saveFile(sigFile, der, der_len);
	if(r != GT_OK) {
		fprintf(stderr, "Cannot save timestamp to file %s: %d (%s)\n",
				sigFile, r, GT_getErrorString(r));
		if(r == GT_IO_ERROR) {
			fprintf(stderr, "\t%d (%s)\n", errno, strerror(errno));
		}
		goto done;
	}
	printf("Timestamping succeeded!\n");
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
}
