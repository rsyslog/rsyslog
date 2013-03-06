/* librsgt.h - rsyslog's guardtime support library
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
#ifndef INCLUDED_LIBRSGT_H
#define INCLUDED_LIBRSGT_H
#include <gt_base.h>

/* Max number of roots inside the forest. This permits blocks of up to
 * 2^MAX_ROOTS records. We assume that 64 is sufficient for all use
 * cases ;) [and 64 is not really a waste of memory, so we do not even
 * try to work with reallocs and such...]
 */
#define MAX_ROOTS 64
#define LOGSIGHDR "LOGSIG10"

/* context for gt calls. All state data is kept here, this permits
 * multiple concurrent callers.
 */
struct gtctx_s {
	enum GTHashAlgorithm hashAlg;
	uint8_t *IV; /* initial value for blinding masks (where to do we get it from?) */
	GTDataHash *x_prev; /* last leaf hash (maybe of previous block) --> preserve on term */
	char *timestamper;
	unsigned char *sigfilename;
	int fd;
	unsigned char *blkStrtHash; /* last hash from previous block */
	uint16_t lenBlkStrtHash;
	uint64_t nRecords;  /* current number of records in current block */
	uint64_t bInBlk;    /* are we currently inside a blk --> need to finish on close */
	int8_t nRoots;
	/* algo engineering: roots structure is split into two arrays
	 * in order to improve cache hits.
	 */
	int8_t roots_valid[MAX_ROOTS];
	GTDataHash *roots_hash[MAX_ROOTS];
	/* data mambers for the associated TLV file */
	char	tlvBuf[4096];
	int	tlvIdx; /* current index into tlvBuf */
};
typedef struct gtctx_s *gtctx;

typedef struct imprint_s imprint_t;
typedef struct block_sig_s block_sig_t;

struct imprint_s {
	uint8_t hashID;
	int	len;
	uint8_t *data;
};

#define SIGID_RFC3161 0
struct block_sig_s {
	uint8_t hashID;
	uint8_t sigID; /* what type of *signature*? */
	uint8_t *iv;
	imprint_t lastHash;
	uint64_t recCount;
	struct {
		struct {
			uint8_t *data;
			size_t len; /* must be size_t due to GT API! */
		} der;
	} sig;
};

void rsgtInit(char *usragent);
void rsgtExit(void);
gtctx rsgtCtxNew(unsigned char *logfn, enum GTHashAlgorithm hashAlg);
void rsgtCtxDel(gtctx ctx);
void sigblkInit(gtctx ctx);
void sigblkAddRecord(gtctx ctx, const unsigned char *rec, const size_t len);
void sigblkFinish(gtctx ctx);
void rsgtCtxSetLogfileName(gtctx ctx, char *logfilename);
#endif  /* #ifndef INCLUDED_LIBRSGT_H */
