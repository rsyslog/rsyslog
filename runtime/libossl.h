/* libossl.h - rsyslog's ossl crypto provider support library
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
#ifndef INCLUDED_LIBOSSL_H
#define INCLUDED_LIBOSSL_H
#include <stdint.h>

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

struct osslctx_s {
	uchar *key;
	size_t keyLen;
	const EVP_CIPHER *cipher; /* container for algorithm + mode */
};
typedef struct osslctx_s* osslctx;
typedef struct osslfile_s *osslfile;

/* this describes a file, as far as libgcry is concerned */
struct osslfile_s {
	// gcry_cipher_hd_t chd; /* cypher handle */ TODO
	EVP_CIPHER_CTX* chd;
	size_t blkLength; /* size of low-level crypto block */
	uchar *eiName; /* name of .encinfo file */
	int fd; /* descriptor of .encinfo file (-1 if not open) */
	char openMode; /* 'r': read, 'w': write */
	osslctx ctx;
	uchar *readBuf;
	int16_t readBufIdx;
	int16_t readBufMaxIdx;
	int8_t bDeleteOnClose; /* for queue support, similar to stream subsys */
	ssize_t bytesToBlkEnd; /* number of bytes remaining in current crypto block
				-1 means -> no end (still being writen to, queue files),
				0 means -> end of block, new one must be started. */
};

osslctx osslCtxNew(void);
void rsosslCtxDel(osslctx ctx);
rsRetVal rsosslSetAlgoMode(osslctx ctx, uchar* algorithm);
int osslGetKeyFromFile(const char* const fn, char** const key, unsigned* const keylen);
int rsosslSetKey(osslctx ctx, unsigned char* key, uint16_t keyLen);
rsRetVal osslfileGetBytesLeftInBlock(osslfile gf, ssize_t* left);
rsRetVal osslfileDeleteState(uchar* logfn);
rsRetVal rsosslInitCrypt(osslctx ctx, osslfile* pgf, uchar* fname, char openMode);
rsRetVal rsosslDecrypt(osslfile pF, uchar* buf, size_t* len);
rsRetVal rsosslEncrypt(osslfile pF, uchar* buf, size_t* len);
int osslfileDestruct(osslfile gf, off64_t offsLogfile);
int rsosslInit(void);
void rsosslExit(void);


// FIXME refactor
static inline void __attribute__((unused))
osslfileSetDeleteOnClose(osslfile gf, const int val) {
	if (gf != NULL)
		gf->bDeleteOnClose = val;
}

#endif  /* #ifndef INCLUDED_LIBOSSL_H */
