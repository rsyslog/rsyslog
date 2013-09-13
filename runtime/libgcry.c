/* gcry.c - rsyslog's libgcrypt based crypto provider
 *
 * Copyright 2013 Adiscon GmbH.
 *
 * We need to store some additional information in support of encryption.
 * For this, we create a side-file, which is named like the actual log
 * file, but with the suffix ".encinfo" appended. It contains the following
 * records:
 * IV:<hex>   The initial vector used at block start. Also indicates start
 *            start of block.
 * END:<int>  The end offset of the block, as uint64_t in decimal notation.
 *            This is used during encryption to know when the current
 *            encryption block ends.
 * For the current implementation, there must always be an IV record
 * followed by an END record. Each records is LF-terminated. Record
 * types can simply be extended in the future by specifying new 
 * types (like "IV") before the colon.
 * To identify a file as rsyslog encryption info file, it must start with
 * the line "FILETYPE:rsyslog-enrcyption-info"
 * There are some size constraints: the recordtype must be 31 bytes at
 * most and the actual value (between : and LF) must be 1023 bytes at most.
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
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <gcrypt.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "rsyslog.h"
#include "libgcry.h"


static rsRetVal
eiWriteRec(gcryfile gf, char *recHdr, size_t lenRecHdr, char *buf, size_t lenBuf)
{
	struct iovec iov[3];
	ssize_t nwritten, towrite;
	DEFiRet;

	iov[0].iov_base = recHdr;
	iov[0].iov_len = lenRecHdr;
	iov[1].iov_base = buf;
	iov[1].iov_len = lenBuf;
	iov[2].iov_base = "\n";
	iov[2].iov_len = 1;
	towrite = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len;
	nwritten = writev(gf->fd, iov, sizeof(iov)/sizeof(struct iovec));
	if(nwritten != towrite) {
		DBGPRINTF("eiWrite%s: error writing file, towrite %d, "
			"nwritten %d\n", recHdr, (int) towrite, (int) nwritten);
		ABORT_FINALIZE(RS_RET_EI_WR_ERR);
	}
	DBGPRINTF("encryption info file %s: written %s, len %d\n",
		  recHdr, gf->eiName, (int) nwritten);
finalize_it:
	RETiRet;
}

static rsRetVal
eiOpenRead(gcryfile gf)
{
	DEFiRet;
	gf->fd = open((char*)gf->eiName, O_RDONLY|O_NOCTTY|O_CLOEXEC);
	if(gf->fd == -1) {
		ABORT_FINALIZE(errno == ENOENT ? RS_RET_EI_NO_EXISTS : RS_RET_EI_OPN_ERR);
	}
finalize_it:
	RETiRet;
}


static rsRetVal
eiCheckFiletype(gcryfile gf)
{
	char hdrBuf[128];
	size_t toRead, didRead;
	DEFiRet;

	CHKiRet(eiOpenRead(gf));
	if(Debug) memset(hdrBuf, 0, sizeof(hdrBuf)); /* for dbgprintf below! */
	toRead = sizeof("FILETYPE:")-1 + sizeof(RSGCRY_FILETYPE_NAME)-1 + 1;
	didRead = read(gf->fd, hdrBuf, toRead);
	close(gf->fd);
	DBGPRINTF("eiCheckFiletype read %d bytes: '%s'\n", didRead, hdrBuf);
	if(   didRead != toRead
	   || strncmp(hdrBuf, "FILETYPE:" RSGCRY_FILETYPE_NAME "\n", toRead))
		iRet = RS_RET_EI_INVLD_FILE;
finalize_it:
	RETiRet;
}

static rsRetVal
eiOpenAppend(gcryfile gf)
{
	rsRetVal localRet;
	DEFiRet;
	localRet = eiCheckFiletype(gf);
	if(localRet == RS_RET_OK) {
		gf->fd = open((char*)gf->eiName,
			       O_WRONLY|O_APPEND|O_NOCTTY|O_CLOEXEC, 0600);
		if(gf->fd == -1) {
			ABORT_FINALIZE(RS_RET_EI_OPN_ERR);
		}
	} else if(localRet == RS_RET_EI_NO_EXISTS) {
		/* looks like we need to create a new file */
		gf->fd = open((char*)gf->eiName,
			       O_WRONLY|O_CREAT|O_NOCTTY|O_CLOEXEC, 0600);
		if(gf->fd == -1) {
			ABORT_FINALIZE(RS_RET_EI_OPN_ERR);
		}
		CHKiRet(eiWriteRec(gf, "FILETYPE:", 9, RSGCRY_FILETYPE_NAME,
			    sizeof(RSGCRY_FILETYPE_NAME)-1));
	} else {
		gf->fd = -1;
		ABORT_FINALIZE(localRet);
	}
	DBGPRINTF("encryption info file %s: opened as #%d\n",
		gf->eiName, gf->fd);
finalize_it:
	RETiRet;
}

static rsRetVal
eiWriteIV(gcryfile gf, uchar *iv)
{
	static const char hexchars[16] =
	   {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
	unsigned iSrc, iDst;
	char hex[4096];
	DEFiRet;

	if(gf->blkLength > sizeof(hex)/2) {
		DBGPRINTF("eiWriteIV: crypto block len way too large, aborting "
			  "write");
		ABORT_FINALIZE(RS_RET_ERR);
	}

	for(iSrc = iDst = 0 ; iSrc < gf->blkLength ; ++iSrc) {
		hex[iDst++] = hexchars[iv[iSrc]>>4];
		hex[iDst++] = hexchars[iv[iSrc]&0x0f];
	}

	iRet = eiWriteRec(gf, "IV:", 3, hex, gf->blkLength*2);
finalize_it:
	RETiRet;
}

/* we do not return an error state, as we MUST close the file,
 * no matter what happens.
 */
static void
eiClose(gcryfile gf, off64_t offsLogfile)
{
	char offs[21];
	size_t len;
	if(gf->fd == -1)
		return;
	/* 2^64 is 20 digits, so the snprintf buffer is large enough */
	len = snprintf(offs, sizeof(offs), "%lld", offsLogfile);
	eiWriteRec(gf, "END:", 4, offs, len);
	close(gf->fd);
	DBGPRINTF("encryption info file %s: closed\n", gf->eiName);
}

static rsRetVal
gcryfileConstruct(gcryctx ctx, gcryfile *pgf, uchar *logfn)
{
	char fn[MAXFNAME+1];
	gcryfile gf;
	DEFiRet;

	CHKmalloc(gf = calloc(1, sizeof(struct gcryfile_s)));
	gf->ctx = ctx;
	snprintf(fn, sizeof(fn), "%s%s", logfn, ENCINFO_SUFFIX);
	fn[MAXFNAME] = '\0'; /* be on save side */
	gf->eiName = (uchar*) strdup(fn);
	*pgf = gf;
finalize_it:
	RETiRet;
}


gcryctx
gcryCtxNew(void)
{
	gcryctx ctx;
	ctx = calloc(1, sizeof(struct gcryctx_s));
	ctx->algo = GCRY_CIPHER_AES128;
	ctx->mode = GCRY_CIPHER_MODE_CBC;
	return ctx;
}

int
gcryfileDestruct(gcryfile gf, off64_t offsLogfile)
{
	int r = 0;
	if(gf == NULL)
		goto done;

	eiClose(gf, offsLogfile);
	free(gf->eiName);
	free(gf);
done:	return r;
}
void
rsgcryCtxDel(gcryctx ctx)
{
	if(ctx != NULL) {
		free(ctx);
	}
}

static inline void
addPadding(gcryfile pF, uchar *buf, size_t *plen)
{
	unsigned i;
	size_t nPad;
	nPad = (pF->blkLength - *plen % pF->blkLength) % pF->blkLength;
	DBGPRINTF("libgcry: addPadding %d chars, blkLength %d, mod %d, pad %d\n",
		  *plen, pF->blkLength, *plen % pF->blkLength, nPad);
	for(i = 0 ; i < nPad ; ++i)
		buf[(*plen)+i] = 0x00;
	(*plen)+= nPad;
}

static inline void
removePadding(char *buf, size_t *plen)
{
	unsigned len = (unsigned) *plen;
	unsigned iSrc, iDst;
	char *frstNUL;

	frstNUL = strchr(buf, 0x00);
	if(frstNUL == NULL)
		goto done;
	iDst = iSrc = frstNUL - buf;

	while(iSrc < len) {
		if(buf[iSrc] != 0x00)
			buf[iDst++] = buf[iSrc];
		++iSrc;
	}

	*plen = iDst;
done:	return;
}

/* returns 0 on succes, positive if key length does not match and key
 * of return value size is required.
 */
int
rsgcrySetKey(gcryctx ctx, unsigned char *key, uint16_t keyLen)
{
	uint16_t reqKeyLen;
	int r;

	reqKeyLen = gcry_cipher_get_algo_keylen(ctx->algo);
	if(keyLen != reqKeyLen) {
		r = reqKeyLen;
		goto done;
	}
	ctx->keyLen = keyLen;
	ctx->key = malloc(keyLen);
	memcpy(ctx->key, key, keyLen);
	r = 0;
done:	return r;
}

rsRetVal
rsgcrySetMode(gcryctx ctx, uchar *modename)
{
	int mode;
	DEFiRet;

	mode = rsgcryModename2Mode((char *)modename);
	if(mode == GCRY_CIPHER_MODE_NONE) {
		ABORT_FINALIZE(RS_RET_CRY_INVLD_MODE);
	}
	ctx->mode = mode;
finalize_it:
	RETiRet;
}

rsRetVal
rsgcrySetAlgo(gcryctx ctx, uchar *algoname)
{
	int algo;
	DEFiRet;

	algo = rsgcryAlgoname2Algo((char *)algoname);
	if(algo == GCRY_CIPHER_NONE) {
		ABORT_FINALIZE(RS_RET_CRY_INVLD_ALGO);
	}
	ctx->algo = algo;
finalize_it:
	RETiRet;
}

/* As of some Linux and security expert I spoke to, /dev/urandom
 * provides very strong random numbers, even if it runs out of
 * entropy. As far as he knew, this is save for all applications
 * (and he had good proof that I currently am not permitted to
 * reproduce). -- rgerhards, 2013-03-04
 */
void
seedIV(gcryfile gf, uchar **iv)
{
	int fd;

	*iv = malloc(gf->blkLength); /* do NOT zero-out! */
	/* if we cannot obtain data from /dev/urandom, we use whatever
	 * is present at the current memory location as random data. Of
	 * course, this is very weak and we should consider a different
	 * option, especially when not running under Linux (for Linux,
	 * unavailability of /dev/urandom is just a theoretic thing, it
	 * will always work...).  -- TODO -- rgerhards, 2013-03-06
	 */
	if((fd = open("/dev/urandom", O_RDONLY)) > 0) {
		if(read(fd, *iv, gf->blkLength)) {}; /* keep compiler happy */
		close(fd);
	}
}

rsRetVal
rsgcryInitCrypt(gcryctx ctx, gcryfile *pgf, uchar *fname)
{
	gcry_error_t gcryError;
	gcryfile gf = NULL;
	uchar *iv = NULL;
	DEFiRet;

	CHKiRet(gcryfileConstruct(ctx, &gf, fname));

	gf->blkLength = gcry_cipher_get_algo_blklen(ctx->algo);

	gcryError = gcry_cipher_open(&gf->chd, ctx->algo, ctx->mode, 0);
	if (gcryError) {
		dbgprintf("gcry_cipher_open failed:  %s/%s\n",
			gcry_strsource(gcryError),
			gcry_strerror(gcryError));
		ABORT_FINALIZE(RS_RET_ERR);
	}

	gcryError = gcry_cipher_setkey(gf->chd, gf->ctx->key, gf->ctx->keyLen);
	if (gcryError) {
		dbgprintf("gcry_cipher_setkey failed:  %s/%s\n",
			gcry_strsource(gcryError),
			gcry_strerror(gcryError));
		ABORT_FINALIZE(RS_RET_ERR);
	}

	seedIV(gf, &iv);
	gcryError = gcry_cipher_setiv(gf->chd, iv, gf->blkLength);
	if (gcryError) {
		dbgprintf("gcry_cipher_setiv failed:  %s/%s\n",
			gcry_strsource(gcryError),
			gcry_strerror(gcryError));
		ABORT_FINALIZE(RS_RET_ERR);
	}
	CHKiRet(eiOpenAppend(gf));
	CHKiRet(eiWriteIV(gf, iv));
	*pgf = gf;
finalize_it:
	free(iv);
	if(iRet != RS_RET_OK && gf != NULL)
		gcryfileDestruct(gf, -1);
	RETiRet;
}

int
rsgcryEncrypt(gcryfile pF, uchar *buf, size_t *len)
{
	int gcryError;
	DEFiRet;
	
	if(*len == 0)
		FINALIZE;

	addPadding(pF, buf, len);
	gcryError = gcry_cipher_encrypt(pF->chd, buf, *len, NULL, 0);
	if(gcryError) {
		dbgprintf("gcry_cipher_encrypt failed:  %s/%s\n",
			gcry_strsource(gcryError),
			gcry_strerror(gcryError));
		ABORT_FINALIZE(RS_RET_ERR);
	}
finalize_it:
	RETiRet;
}


/* module-init dummy for potential later use */
int
rsgcryInit(void)
{
	return 0;
}

/* module-deinit dummy for potential later use */
void
rsgcryExit(void)
{
	return;
}
