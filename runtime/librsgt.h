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
	uint8_t bKeepRecordHashes;
	uint8_t bKeepTreeHashes;
	uint64_t blockSizeLimit;
	char *timestamper;
	unsigned char *sigfilename;
	unsigned char *statefilename;
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


/* the following defines the gtstate file record. Currently, this record
 * is fixed, we may change that over time.
 */
struct rsgtstatefile {
	char hdr[8];	/* must be "GTSTAT10" */
	uint8_t hashID;
	uint8_t lenHash;
	/* after that, the hash value is contained within the file */
};

/* error states */
#define RSGTE_IO 1 	/* any kind of io error, including EOF */
#define RSGTE_FMT 2	/* data fromat error */
#define RSGTE_INVLTYP 3	/* invalid TLV type record (unexcpected at this point) */
#define RSGTE_OOM 4	/* ran out of memory */
#define RSGTE_LEN 5	/* error related to length records */


static inline uint16_t
hashOutputLengthOctets(uint8_t hashID)
{
	switch(hashID) {
	case GT_HASHALG_SHA1:	/* paper: SHA1 */
		return 20;
	case GT_HASHALG_RIPEMD160: /* paper: RIPEMD-160 */
		return 20;
	case GT_HASHALG_SHA224:	/* paper: SHA2-224 */
		return 28;
	case GT_HASHALG_SHA256: /* paper: SHA2-256 */
		return 32;
	case GT_HASHALG_SHA384: /* paper: SHA2-384 */
		return 48;
	case GT_HASHALG_SHA512:	/* paper: SHA2-512 */
		return 64;
	default:return 32;
	}
}

static inline uint8_t
hashIdentifier(uint8_t hashID)
{
	switch(hashID) {
	case GT_HASHALG_SHA1:	/* paper: SHA1 */
		return 0x00;
	case GT_HASHALG_RIPEMD160: /* paper: RIPEMD-160 */
		return 0x02;
	case GT_HASHALG_SHA224:	/* paper: SHA2-224 */
		return 0x03;
	case GT_HASHALG_SHA256: /* paper: SHA2-256 */
		return 0x01;
	case GT_HASHALG_SHA384: /* paper: SHA2-384 */
		return 0x04;
	case GT_HASHALG_SHA512:	/* paper: SHA2-512 */
		return 0x05;
	default:return 0xff;
	}
}
static inline char *
hashAlgName(uint8_t hashID)
{
	switch(hashID) {
	case GT_HASHALG_SHA1:
		return "SHA1";
	case GT_HASHALG_RIPEMD160:
		return "RIPEMD-160";
	case GT_HASHALG_SHA224:
		return "SHA2-224";
	case GT_HASHALG_SHA256:
		return "SHA2-256";
	case GT_HASHALG_SHA384:
		return "SHA2-384";
	case GT_HASHALG_SHA512:
		return "SHA2-512";
	default:return "[unknown]";
	}
}
static inline char *
sigTypeName(uint8_t sigID)
{
	switch(sigID) {
	case SIGID_RFC3161:
		return "RFC3161";
	default:return "[unknown]";
	}
}
static inline uint16_t
getIVLen(block_sig_t *bs)
{
	return hashOutputLengthOctets(bs->hashID);
}
static inline void
rsgtSetTimestamper(gtctx ctx, char *timestamper)
{
	free(ctx->timestamper);
	ctx->timestamper = strdup(timestamper);
}
static inline void
rsgtSetBlockSizeLimit(gtctx ctx, uint64_t limit)
{
	ctx->blockSizeLimit = limit;
}
static inline void
rsgtSetKeepRecordHashes(gtctx ctx, int val)
{
	ctx->bKeepRecordHashes = val;
}
static inline void
rsgtSetKeepTreeHashes(gtctx ctx, int val)
{
	ctx->bKeepTreeHashes = val;
}

int rsgtSetHashFunction(gtctx ctx, char *algName);
void rsgtInit(char *usragent);
void rsgtExit(void);
gtctx rsgtCtxNew(void);
int rsgtCtxOpenFile(gtctx ctx, unsigned char *logfn);
void rsgtCtxDel(gtctx ctx);
void sigblkInit(gtctx ctx);
void sigblkAddRecord(gtctx ctx, const unsigned char *rec, const size_t len);
void sigblkFinish(gtctx ctx);
void rsgtCtxSetLogfileName(gtctx ctx, char *logfilename);
/* reader functions */
int rsgt_tlvrdHeader(FILE *fp, unsigned char *hdr);
int rsgt_tlvrd(FILE *fp, uint16_t *tlvtype, uint16_t *tlvlen, void *obj);
void rsgt_tlvprint(FILE *fp, uint16_t tlvtype, void *obj, uint8_t verbose);

#endif  /* #ifndef INCLUDED_LIBRSGT_H */
