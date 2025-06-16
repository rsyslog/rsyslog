/* libossl.c - rsyslog's openssl based crypto provider
 *
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
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <openssl/evp.h>
#include <string.h>

#include "rsyslog.h"
#include "srUtils.h"
#include "debug.h"
#include "libossl.h"
#include "libcry_common.h"

#define READBUF_SIZE 4096	/* size of the read buffer */
static rsRetVal rsosslBlkBegin(osslfile gf);

/* read a key from a key file
 * @param[out] key - key buffer, must be freed by caller
 * @param[out] keylen - length of buffer
 * @returns 0 if OK, something else otherwise (we do not use
 *            iRet as this is also called from non-rsyslog w/o runtime)
 *	      on error, errno is set and can be queried
 * The key length is limited to 64KiB to prevent DoS.
 * Note well: key is a blob, not a C string (NUL may be present!)
 */
int osslGetKeyFromFile(const char* const fn, char** const key, unsigned* const keylen) {
	struct stat sb;
	int r = -1;

	const int fd = open(fn, O_RDONLY);
	if (fd < 0) goto done;
	if (fstat(fd, &sb) == -1) goto done;
	if (sb.st_size > 64 * 1024) {
		errno = EMSGSIZE;
		goto done;
	}
	if ((*key = malloc(sb.st_size)) == NULL) goto done;
	if (read(fd, *key, sb.st_size) != sb.st_size) goto done;
	*keylen = sb.st_size;
	r = 0;
done:
	if (fd >= 0) {
		close(fd);
	}
	return r;
}

static void
addPadding(osslfile pF, uchar* buf, size_t* plen) {
	unsigned i;
	size_t nPad;
	nPad = (pF->blkLength - *plen % pF->blkLength) % pF->blkLength;
	DBGPRINTF("libgcry: addPadding %zd chars, blkLength %zd, mod %zd, pad %zd\n",
		*plen, pF->blkLength, *plen % pF->blkLength, nPad);
	for (i = 0; i < nPad; ++i)
		buf[(*plen) + i] = 0x00;
	(*plen) += nPad;
}

static void ATTR_NONNULL()
removePadding(uchar* const buf, size_t* const plen) {
	const size_t len = *plen;
	size_t iSrc, iDst;

	iSrc = 0;
	/* skip to first NUL */
	while (iSrc < len && buf[iSrc] == '\0') {
		++iSrc;
	}
	iDst = iSrc;

	while (iSrc < len) {
		if (buf[iSrc] != 0x00)
			buf[iDst++] = buf[iSrc];
		++iSrc;
	}

	*plen = iDst;
}

static rsRetVal
eiWriteRec(osslfile gf, const char *recHdr, size_t lenRecHdr, const char *buf, size_t lenBuf)
{
	struct iovec iov[3];
	ssize_t nwritten, towrite;
	DEFiRet;

	iov[0].iov_base = (void*)recHdr;
	iov[0].iov_len = lenRecHdr;
	iov[1].iov_base = (void*)buf;
	iov[1].iov_len = lenBuf;
	iov[2].iov_base = (void*)"\n";
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
eiOpenRead(osslfile gf)
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
eiRead(osslfile gf)
{
	ssize_t nRead;
	DEFiRet;

	if(gf->readBuf == NULL) {
		CHKmalloc(gf->readBuf = malloc(READBUF_SIZE));
	}

	nRead = read(gf->fd, gf->readBuf, READBUF_SIZE);
	if(nRead <= 0) {
		ABORT_FINALIZE(RS_RET_ERR);
	}
	gf->readBufMaxIdx = (int16_t) nRead;
	gf->readBufIdx = 0;

finalize_it:
	RETiRet;
}


/* returns EOF on any kind of error */
static int
eiReadChar(osslfile gf)
{
	int c;

	if(gf->readBufIdx >= gf->readBufMaxIdx) {
		if(eiRead(gf) != RS_RET_OK) {
			c = EOF;
			goto finalize_it;
		}
	}
	c = gf->readBuf[gf->readBufIdx++];
finalize_it:
	return c;
}


static rsRetVal
eiCheckFiletype(osslfile gf)
{
	char hdrBuf[128];
	size_t toRead, didRead;
	sbool bNeedClose = 0;
	DEFiRet;

	if(gf->fd == -1) {
		CHKiRet(eiOpenRead(gf));
		assert(gf->fd != -1); /* cannot happen after successful return */
		bNeedClose = 1;
	}

	if(Debug) memset(hdrBuf, 0, sizeof(hdrBuf)); /* for dbgprintf below! */
	toRead = sizeof("FILETYPE:")-1 + sizeof(RSGCRY_FILETYPE_NAME)-1 + 1;
	didRead = read(gf->fd, hdrBuf, toRead);
	if(bNeedClose) {
		close(gf->fd);
		gf->fd = -1;
	}
	DBGPRINTF("eiCheckFiletype read %zd bytes: '%s'\n", didRead, hdrBuf);
	if(   didRead != toRead
	   || strncmp(hdrBuf, "FILETYPE:" RSGCRY_FILETYPE_NAME "\n", toRead))
		iRet = RS_RET_EI_INVLD_FILE;
finalize_it:
	RETiRet;
}

/* rectype/value must be EIF_MAX_*_LEN+1 long!
 * returns 0 on success or something else on error/EOF
 */
static rsRetVal
eiGetRecord(osslfile gf, char* rectype, char* value)
{
	unsigned short i, j;
	int c;
	DEFiRet;

	c = eiReadChar(gf);
	if(c == EOF) { ABORT_FINALIZE(RS_RET_NO_DATA); }
	for(i = 0 ; i < EIF_MAX_RECTYPE_LEN ; ++i) {
		if(c == ':' || c == EOF)
			break;
		rectype[i] = c;
		c = eiReadChar(gf);
	}
	if(c != ':') { ABORT_FINALIZE(RS_RET_ERR); }
	rectype[i] = '\0';
	j = 0;
	for(++i ; i < EIF_MAX_VALUE_LEN ; ++i, ++j) {
		c = eiReadChar(gf);
		if(c == '\n' || c == EOF)
			break;
		value[j] = c;
	}
	if(c != '\n') { ABORT_FINALIZE(RS_RET_ERR); }
	value[j] = '\0';
finalize_it:
	RETiRet;
}

static rsRetVal
eiGetIV(osslfile gf, uchar* iv, size_t leniv)
{
	char rectype[EIF_MAX_RECTYPE_LEN+1];
	char value[EIF_MAX_VALUE_LEN+1];
	size_t valueLen;
	unsigned short i, j;
	unsigned char nibble;
	DEFiRet;

	CHKiRet(eiGetRecord(gf, rectype, value));
	const char *const cmp_IV = "IV"; // work-around for static analyzer
	if(strcmp(rectype, cmp_IV)) {
		DBGPRINTF("no IV record found when expected, record type "
			"seen is '%s'\n", rectype);
		ABORT_FINALIZE(RS_RET_ERR);
	}
	valueLen = strlen(value);
	if(valueLen/2 != leniv) {
		DBGPRINTF("length of IV is %zd, expected %zd\n",
			valueLen/2, leniv);
		ABORT_FINALIZE(RS_RET_ERR);
	}

	for(i = j = 0 ; i < valueLen ; ++i) {
		if(value[i] >= '0' && value[i] <= '9')
			nibble = value[i] - '0';
		else if(value[i] >= 'a' && value[i] <= 'f')
			nibble = value[i] - 'a' + 10;
		else {
			DBGPRINTF("invalid IV '%s'\n", value);
			ABORT_FINALIZE(RS_RET_ERR);
		}
		if(i % 2 == 0)
			iv[j] = nibble << 4;
		else
			iv[j++] |= nibble;
	}
finalize_it:
	RETiRet;
}

static rsRetVal
eiGetEND(osslfile gf, off64_t* offs)
{
	char rectype[EIF_MAX_RECTYPE_LEN+1];
	char value[EIF_MAX_VALUE_LEN+1];
	DEFiRet;

	CHKiRet(eiGetRecord(gf, rectype, value));
	const char *const const_END = "END"; // clang static analyzer work-around
	if(strcmp(rectype, const_END)) {
		DBGPRINTF("no END record found when expected, record type "
			  "seen is '%s'\n", rectype);
		ABORT_FINALIZE(RS_RET_ERR);
	}
	*offs = atoll(value);
finalize_it:
	RETiRet;
}

static rsRetVal
eiOpenAppend(osslfile gf)
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

static rsRetVal __attribute__((nonnull(2)))
eiWriteIV(osslfile gf, const uchar* const iv)
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
eiClose(osslfile gf, off64_t offsLogfile)
{
	char offs[21];
	size_t len;
	if(gf->fd == -1)
		return;
	if(gf->openMode == 'w') {
		/* 2^64 is 20 digits, so the snprintf buffer is large enough */
		len = snprintf(offs, sizeof(offs), "%lld", (long long) offsLogfile);
		eiWriteRec(gf, "END:", 4, offs, len);
	}
	EVP_CIPHER_CTX_free(gf->chd);
	free(gf->readBuf);
	close(gf->fd);
	gf->fd = -1;
	DBGPRINTF("encryption info file %s: closed\n", gf->eiName);
}

/* this returns the number of bytes left inside the block or -1, if the block
 * size is unbounded. The function automatically handles end-of-block and begins
 * to read the next block in this case.
 */
rsRetVal
osslfileGetBytesLeftInBlock(osslfile gf, ssize_t* left)
{
	DEFiRet;
	if(gf->bytesToBlkEnd == 0) {
		DBGPRINTF("libossl: end of current crypto block\n");
		EVP_CIPHER_CTX_free(gf->chd);
		CHKiRet(rsosslBlkBegin(gf));
	}
	*left = gf->bytesToBlkEnd;
finalize_it:
	RETiRet;
}

/* this is a special functon for use by the rsyslog disk queue subsystem. It
 * needs to have the capability to delete state when a queue file is rolled
 * over. This simply generates the file name and deletes it. It must take care
 * of "all" state files, which currently happens to be a single one.
 */
rsRetVal
osslfileDeleteState(uchar *logfn)
{
	char fn[MAXFNAME+1];
	DEFiRet;
	snprintf(fn, sizeof(fn), "%s%s", logfn, ENCINFO_SUFFIX);
	fn[MAXFNAME] = '\0'; /* be on save side */
	DBGPRINTF("ossl crypto provider deletes state file '%s' on request\n", fn);
	unlink(fn);
	RETiRet;
}

static rsRetVal
osslfileConstruct(osslctx ctx, osslfile* pgf, uchar* logfn)
{
	char fn[MAXFNAME+1];
	osslfile gf;
	DEFiRet;

	CHKmalloc(gf = calloc(1, sizeof(struct osslfile_s)));
	CHKmalloc(gf->chd = EVP_CIPHER_CTX_new());
	gf->ctx = ctx;
	gf->fd = -1;
	snprintf(fn, sizeof(fn), "%s%s", logfn, ENCINFO_SUFFIX);
	fn[MAXFNAME] = '\0'; /* be on save side */
	gf->eiName = (uchar*) strdup(fn);
	*pgf = gf;
finalize_it:
	RETiRet;
}


osslctx
osslCtxNew(void)
{
	osslctx ctx;
	ctx = calloc(1, sizeof(struct osslctx_s));
	if (ctx != NULL) {
		ctx->cipher = EVP_aes_128_cbc();
		ctx->key = NULL;
		ctx->keyLen = -1;
	}
	return ctx;
}

int
osslfileDestruct(osslfile gf, off64_t offsLogfile)
{
	int r = 0;
	if(gf == NULL)
		goto done;

	DBGPRINTF("libossl: close file %s\n", gf->eiName);
	eiClose(gf, offsLogfile);
	if(gf->bDeleteOnClose) {
		DBGPRINTF("unlink file '%s' due to bDeleteOnClose set\n", gf->eiName);
		unlink((char*)gf->eiName);
	}
	free(gf->eiName);
	free(gf);
done:
	return r;
}

void
rsosslCtxDel(osslctx ctx)
{
	if(ctx != NULL) {
		free(ctx->key);
		free(ctx);
	}
}


/* returns 0 on succes, positive if key length does not match and key
 * of return value size is required.
 */
int
rsosslSetKey(osslctx ctx, unsigned char *key, uint16_t keyLen)
{
	uint16_t reqKeyLen;
	int r;

	reqKeyLen = EVP_CIPHER_get_key_length(ctx->cipher);
	if(keyLen != reqKeyLen) {
		r = reqKeyLen;
		goto done;
	}
	ctx->keyLen = keyLen;
	ctx->key = malloc(keyLen);
	memcpy(ctx->key, key, keyLen);
	r = 0;
done:
	return r;
}

rsRetVal
rsosslSetAlgoMode(osslctx ctx, uchar *algorithm)
{
	EVP_CIPHER *cipher;
	DEFiRet;

	cipher = EVP_CIPHER_fetch(NULL, (char *)algorithm, NULL);
	if (cipher == NULL) {
		ABORT_FINALIZE(RS_RET_CRY_INVLD_ALGO);
	}
	ctx->cipher = cipher;

finalize_it:
	RETiRet;
}

/* We use random numbers to initiate the IV. Rsyslog runtime will ensure
 * we get a sufficiently large number.
 */
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wunknown-attributes"
#endif
static rsRetVal
#if defined(__clang__)
__attribute__((no_sanitize("shift"))) /* IV shift causes known overflow */
#endif
seedIV(osslfile gf, uchar **iv)
{
	long rndnum = 0; /* keep compiler happy -- this value is always overriden */
	DEFiRet;

	CHKmalloc(*iv = calloc(1, gf->blkLength));
	for(size_t i = 0 ; i < gf->blkLength; ++i) {
		const int shift = (i % 4) * 8;
		if(shift == 0) { /* need new random data? */
			rndnum = randomNumber();
		}
		(*iv)[i] = 0xff & ((rndnum & (255u << shift)) >> shift);
	}
finalize_it:
	RETiRet;
}

static rsRetVal
readIV(osslfile gf, uchar **iv)
{
	rsRetVal localRet;
	DEFiRet;

	if(gf->fd == -1) {
		while(gf->fd == -1) {
			localRet = eiOpenRead(gf);
			if(localRet == RS_RET_EI_NO_EXISTS) {
				/* wait until it is created */
				srSleep(0, 10000);
			} else {
				CHKiRet(localRet);
			}
		}
		CHKiRet(eiCheckFiletype(gf));
	}
	*iv = malloc(gf->blkLength); /* do NOT zero-out! */
	iRet = eiGetIV(gf, *iv, (size_t) gf->blkLength);
finalize_it:
	RETiRet;
}

/* this tries to read the END record. HOWEVER, no such record may be
 * present, which is the case if we handle a currently-written to queue
 * file. On the other hand, the queue file may contain multiple blocks. So
 * what we do is try to see if there is a block end or not - and set the
 * status accordingly. Note that once we found no end-of-block, we will never
 * retry. This is because that case can never happen under current queue
 * implementations. -- gerhards, 2013-05-16
 */
static rsRetVal
readBlkEnd(osslfile gf)
{
	off64_t blkEnd;
	DEFiRet;

	iRet = eiGetEND(gf, &blkEnd);
	if(iRet == RS_RET_OK) {
		gf->bytesToBlkEnd = (ssize_t) blkEnd;
	} else if(iRet == RS_RET_NO_DATA) {
		gf->bytesToBlkEnd = -1;
	} else {
		FINALIZE;
	}

finalize_it:
	RETiRet;
}



/* module-init dummy for potential later use */
int
rsosslInit(void)
{
	return 0;
}

/* module-deinit dummy for potential later use */
void
rsosslExit(void)
{
	return;
}

/* Read the block begin metadata and set our state variables accordingly.
 * Can also be used to init the first block in write case.
 */
static rsRetVal
rsosslBlkBegin(osslfile gf) {
	uchar* iv = NULL;
	DEFiRet;
	const char openMode = gf->openMode;
	// FIXME error handling

	if (openMode == 'r') {
		readIV(gf, &iv);
		readBlkEnd(gf);
	} else {
		CHKiRet(seedIV(gf, &iv));
	}

	if (openMode == 'r') {
		if ((iRet = EVP_DecryptInit_ex(gf->chd, gf->ctx->cipher, NULL, gf->ctx->key, iv)) != 1) {
			DBGPRINTF("EVP_DecryptInit_ex failed:  %d\n", iRet);
			ABORT_FINALIZE(RS_RET_ERR);
		}
	} else {
		if ((iRet = EVP_EncryptInit_ex(gf->chd, gf->ctx->cipher, NULL, gf->ctx->key, iv)) != 1){
			DBGPRINTF("EVP_EncryptInit_ex failed:  %d\n", iRet);
			ABORT_FINALIZE(RS_RET_ERR);
		}
	}

	if ((iRet = EVP_CIPHER_CTX_set_padding(gf->chd, 0)) != 1) {
		fprintf(stderr, "EVP_CIPHER_set_padding failed:  %d\n", iRet);
		ABORT_FINALIZE(RS_RET_ERR);
	}

	if (openMode == 'w') {
		CHKiRet(eiOpenAppend(gf));
		CHKiRet(eiWriteIV(gf, iv));
	}
finalize_it:
	free(iv);
	RETiRet;
}

rsRetVal
rsosslInitCrypt(osslctx ctx, osslfile* pgf, uchar* fname, char openMode) {
	osslfile gf = NULL;
	DEFiRet;

	CHKiRet(osslfileConstruct(ctx, &gf, fname));
	gf->openMode = openMode;
	gf->blkLength = EVP_CIPHER_get_block_size(ctx->cipher);
	CHKiRet(rsosslBlkBegin(gf));
	*pgf = gf;
finalize_it:
	if (iRet != RS_RET_OK && gf != NULL)
		osslfileDestruct(gf, -1);
	RETiRet;
}

rsRetVal
rsosslDecrypt(osslfile pF, uchar* buf, size_t* len) {
	DEFiRet;
	int rc;

	if (pF->bytesToBlkEnd != -1)
		pF->bytesToBlkEnd -= *len;
	rc = EVP_DecryptUpdate(pF->chd, buf, (int *)len, buf, (int) *len);
	if (rc != 1) {
		DBGPRINTF("EVP_DecryptUpdate failed\n");
		ABORT_FINALIZE(RS_RET_ERR);
	}
	removePadding(buf, len);

	dbgprintf("libossl: decrypted, bytesToBlkEnd %lld, buffer is now '%50.50s'\n",
		(long long)pF->bytesToBlkEnd, buf);

finalize_it:
	RETiRet;
}

rsRetVal
rsosslEncrypt(osslfile pF, uchar* buf, size_t* len) {
	DEFiRet;
	int tmplen;

	if (*len == 0)
		FINALIZE;

	addPadding(pF, buf, len);
	if (EVP_EncryptUpdate(pF->chd, buf, (int *)len, buf, (int) *len) != 1) {
		dbgprintf("EVP_EncryptUpdate failed\n");
		ABORT_FINALIZE(RS_RET_CRYPROV_ERR);
	}

	if (EVP_EncryptFinal_ex(pF->chd, buf + *len, &tmplen) != 1) {
		dbgprintf("EVP_EncryptFinal_ex failed\n");
		ABORT_FINALIZE(RS_RET_CRYPROV_ERR);
	}
	*len += tmplen;

finalize_it:
	RETiRet;
}
