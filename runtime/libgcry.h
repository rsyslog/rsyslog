/* libgcry.h - rsyslog's guardtime support library
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
#ifndef INCLUDED_LIBGCRY_H
#define INCLUDED_LIBGCRY_H
#include <gt_base.h>


struct gcryctx_s {
	uchar *key;
	size_t keyLen;
	int algo;
	int mode;
};
typedef struct gcryctx_s *gcryctx;
typedef struct gcryfile_s *gcryfile;

/* this describes a file, as far as libgcry is concerned */
struct gcryfile_s {
	gcry_cipher_hd_t chd; /* cypher handle */
	size_t blkLength; /* size of low-level crypto block */
	uchar *eiName; /* name of .encinfo file */
	int fd; /* descriptor of .encinfo file (-1 if not open) */
	gcryctx ctx;
};

int gcryGetKeyFromFile(char *fn, char **key, unsigned *keylen);
int rsgcryInit(void);
void rsgcryExit(void);
int rsgcrySetKey(gcryctx ctx, unsigned char *key, uint16_t keyLen);
rsRetVal rsgcrySetMode(gcryctx ctx, uchar *algoname);
rsRetVal rsgcrySetAlgo(gcryctx ctx, uchar *modename);
gcryctx gcryCtxNew(void);
void rsgcryCtxDel(gcryctx ctx);
int gcryfileDestruct(gcryfile gf, off64_t offsLogfile);
rsRetVal rsgcryInitCrypt(gcryctx ctx, gcryfile *pgf, uchar *fname);
int rsgcryEncrypt(gcryfile pF, uchar *buf, size_t *len);

/* error states */
#define RSGCRYE_EI_OPEN 1 	/* error opening .encinfo file */
#define RSGCRYE_OOM 4	/* ran out of memory */

#define EIF_MAX_RECTYPE_LEN 31 /* max length of record types */
#define EIF_MAX_VALUE_LEN 1023 /* max length of value types */
#define RSGCRY_FILETYPE_NAME "rsyslog-enrcyption-info"
#define ENCINFO_SUFFIX ".encinfo"

static inline int
rsgcryAlgoname2Algo(char *algoname) {
	if(!strcmp((char*)algoname, "3DES")) return GCRY_CIPHER_3DES;
	if(!strcmp((char*)algoname, "CAST5")) return GCRY_CIPHER_CAST5;
	if(!strcmp((char*)algoname, "BLOWFISH")) return GCRY_CIPHER_BLOWFISH;
	if(!strcmp((char*)algoname, "AES128")) return GCRY_CIPHER_AES128;
	if(!strcmp((char*)algoname, "AES192")) return GCRY_CIPHER_AES192;
	if(!strcmp((char*)algoname, "AES256")) return GCRY_CIPHER_AES256;
	if(!strcmp((char*)algoname, "TWOFISH")) return GCRY_CIPHER_TWOFISH;
	if(!strcmp((char*)algoname, "TWOFISH128")) return GCRY_CIPHER_TWOFISH128;
	if(!strcmp((char*)algoname, "ARCFOUR")) return GCRY_CIPHER_ARCFOUR;
	if(!strcmp((char*)algoname, "DES")) return GCRY_CIPHER_DES;
	if(!strcmp((char*)algoname, "SERPENT128")) return GCRY_CIPHER_SERPENT128;
	if(!strcmp((char*)algoname, "SERPENT192")) return GCRY_CIPHER_SERPENT192;
	if(!strcmp((char*)algoname, "SERPENT256")) return GCRY_CIPHER_SERPENT256;
	if(!strcmp((char*)algoname, "RFC2268_40")) return GCRY_CIPHER_RFC2268_40;
	if(!strcmp((char*)algoname, "SEED")) return GCRY_CIPHER_SEED;
	if(!strcmp((char*)algoname, "CAMELLIA128")) return GCRY_CIPHER_CAMELLIA128;
	if(!strcmp((char*)algoname, "CAMELLIA192")) return GCRY_CIPHER_CAMELLIA192;
	if(!strcmp((char*)algoname, "CAMELLIA256")) return GCRY_CIPHER_CAMELLIA256;
	return GCRY_CIPHER_NONE;
}

static inline int
rsgcryModename2Mode(char *modename) {
	if(!strcmp((char*)modename, "ECB")) return GCRY_CIPHER_MODE_ECB;
	if(!strcmp((char*)modename, "CFB")) return GCRY_CIPHER_MODE_CFB;
	if(!strcmp((char*)modename, "CBC")) return GCRY_CIPHER_MODE_CBC;
	if(!strcmp((char*)modename, "STREAM")) return GCRY_CIPHER_MODE_STREAM;
	if(!strcmp((char*)modename, "OFB")) return GCRY_CIPHER_MODE_OFB;
	if(!strcmp((char*)modename, "CTR")) return GCRY_CIPHER_MODE_CTR;
	if(!strcmp((char*)modename, "AESWRAP")) return GCRY_CIPHER_MODE_AESWRAP;
	return GCRY_CIPHER_MODE_NONE;
}
#endif  /* #ifndef INCLUDED_LIBGCRY_H */
