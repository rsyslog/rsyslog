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

int rsgcryInit(void);
void rsgcryExit(void);
int rsgcrySetKey(gcryctx ctx, unsigned char *key, uint16_t keyLen);
gcryctx gcryCtxNew(void);
void rsgcryCtxDel(gcryctx ctx);
int gcryfileDestruct(gcryfile gf, off64_t offsLogfile);
rsRetVal rsgcryInitCrypt(gcryctx ctx, gcryfile *pgf, int gcry_mode, uchar *fname);
int rsgcryEncrypt(gcryfile pF, uchar *buf, size_t *len);

/* error states */
#define RSGCRYE_EI_OPEN 1 	/* error opening .encinfo file */
#define RSGCRYE_OOM 4	/* ran out of memory */

#define RSGCRY_FILETYPE_NAME "rsyslog-enrcyption-info"

#endif  /* #ifndef INCLUDED_LIBGCRY_H */
