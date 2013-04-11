/* gcry.c - rsyslog's libgcrypt based crypto provider
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
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <gcrypt.h>

#include "rsyslog.h"
#include "libgcry.h"

#define GCRY_CIPHER GCRY_CIPHER_3DES  // TODO: make configurable

static inline gcryfile
gcryfileConstruct(gcryctx ctx)
{
	gcryfile gf;
	if((gf = calloc(1, sizeof(struct gcryfile_s))) == NULL)
		goto done;
	gf->ctx = ctx;
done:	return gf;
}
gcryctx
gcryCtxNew(void)
{
	gcryctx ctx;
	ctx = calloc(1, sizeof(struct gcryctx_s));
	return ctx;
}

int
gcryfileDestruct(gcryfile gf)
{
	int r = 0;
	if(gf == NULL)
		goto done;

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
	dbgprintf("DDDD: addPadding %d chars, blkLength %d, mod %d, pad %d\n",
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
	uint16_t reqKeyLen = gcry_cipher_get_algo_keylen(GCRY_CIPHER);
	int r;

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
rsgcryInitCrypt(gcryctx ctx, gcryfile *pgf, int gcry_mode, char *iniVector)
{
	gcry_error_t gcryError;
	gcryfile gf = NULL;
	DEFiRet;

	CHKmalloc(gf = gcryfileConstruct(ctx));

	gf->blkLength = gcry_cipher_get_algo_blklen(GCRY_CIPHER);

	gcryError = gcry_cipher_open(
		&gf->chd, // gcry_cipher_hd_t *
		GCRY_CIPHER,   // int
		gcry_mode,     // int
		0);            // unsigned int
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

	gcryError = gcry_cipher_setiv(gf->chd, iniVector, gf->blkLength);
	if (gcryError) {
		dbgprintf("gcry_cipher_setiv failed:  %s/%s\n",
			gcry_strsource(gcryError),
			gcry_strerror(gcryError));
		ABORT_FINALIZE(RS_RET_ERR);
	}
	*pgf = gf;
finalize_it:
	if(iRet != RS_RET_OK && gf != NULL)
		gcryfileDestruct(gf);
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

#if 0 // we use this for the tool, only!
static void
doDeCrypt(FILE *fpin, FILE *fpout)
{
	gcry_error_t     gcryError;
	char	buf[64*1024];
	size_t	nRead, nWritten;
	size_t nPad;
	
	while(1) {
		nRead = fread(buf, 1, sizeof(buf), fpin);
		if(nRead == 0)
			break;
		nPad = (blkLength - nRead % blkLength) % blkLength;
		fprintf(stderr, "read %d chars, blkLength %d, mod %d, pad %d\n", nRead, blkLength,
			nRead % blkLength, nPad);
		gcryError = gcry_cipher_decrypt(
				gcryCipherHd, // gcry_cipher_hd_t
				buf,    // void *
				nRead,    // size_t
				NULL,    // const void *
				0);   // size_t
		if (gcryError) {
			fprintf(stderr, "gcry_cipher_encrypt failed:  %s/%s\n",
			gcry_strsource(gcryError),
			gcry_strerror(gcryError));
			return;
		}
fprintf(stderr, "in remove pad, %d\n", nRead);
		removePadding(buf, &nRead);
fprintf(stderr, "out remove pad %d\n", nRead);
		nWritten = fwrite(buf, 1, nRead, fpout);
		if(nWritten != nRead) {
			perror("fpout");
			return;
		}
	}
}
#endif
