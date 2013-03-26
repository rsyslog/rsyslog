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

/* context for gt calls. This primarily serves as a container for the
 * config settings. The actual file-specific data is kept in gtfile.
 */
struct gtctx_s {
	enum GTHashAlgorithm hashAlg;
	uint8_t bKeepRecordHashes;
	uint8_t bKeepTreeHashes;
	uint64_t blockSizeLimit;
	char *timestamper;
	void (*errFunc)(void *, unsigned char*);
	void *usrptr; /* for error function */
};
typedef struct gtctx_s *gtctx;
typedef struct gtfile_s *gtfile;
typedef struct gterrctx_s gterrctx_t;
typedef struct imprint_s imprint_t;
typedef struct block_sig_s block_sig_t;
typedef struct tlvrecord_s tlvrecord_t;

/* this describes a file, as far as librsgt is concerned */
struct gtfile_s {
	/* the following data items are mirrored from gtctx to
	 * increase cache hit ratio (they are frequently accesed).
	 */
	enum GTHashAlgorithm hashAlg;
	uint8_t bKeepRecordHashes;
	uint8_t bKeepTreeHashes;
	/* end mirrored properties */
	uint8_t disabled; /* permits to disable this file --> set to 1 */
	uint64_t blockSizeLimit;
	uint8_t *IV; /* initial value for blinding masks */
	imprint_t *x_prev; /* last leaf hash (maybe of previous block) --> preserve on term */
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
	/* data members for the associated TLV file */
	char	tlvBuf[4096];
	int	tlvIdx; /* current index into tlvBuf */
	gtctx ctx;
};

struct tlvrecord_s {
	uint16_t tlvtype;
	uint16_t tlvlen;
	uint8_t hdr[4]; /* the raw header (as persisted to file) */
	uint8_t lenHdr; /* length of raw header */
	uint8_t data[64*1024];	/* the actual data part (of length tlvlen) */
};

/* The following structure describes the "error context" to be used
 * for verification and similiar reader functions. While verifying,
 * we need some information (like filenames or block numbers) that
 * is not readily available from the other objects (or not even known
 * to librsgt). In order to provide meaningful error messages, this
 * information must be passed in from the external callers. In order
 * to centralize information (and make it more manageable), we use
 * ths error context here, which contains everything needed to
 * generate good error messages. Members of this structure are
 * maintained both by library users (the callers) as well as
 * the library itself. Who does what simply depends on who has
 * the relevant information.
 */
struct gterrctx_s {
	FILE *fp;	/**< file for error messages */
	char *filename;
	uint8_t verbose;
	uint64_t recNumInFile;
	uint64_t recNum;
	uint64_t blkNum;
	uint8_t treeLevel;
	GTDataHash *computedHash;
	GTDataHash *lefthash, *righthash; /* hashes to display if tree hash fails */
	imprint_t *fileHash;
	int gtstate;	/* status from last relevant GT.*() function call */
	char *errRec;
	char *frstRecInBlk; /* This holds the first message seen inside the current block */
};

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

/* Flags and record types for TLV handling */
#define RSGT_FLAG_TLV16 0x20

/* error states */
#define RSGTE_IO 1 	/* any kind of io error */
#define RSGTE_FMT 2	/* data fromat error */
#define RSGTE_INVLTYP 3	/* invalid TLV type record (unexcpected at this point) */
#define RSGTE_OOM 4	/* ran out of memory */
#define RSGTE_LEN 5	/* error related to length records */
#define RSGTE_TS_EXTEND 6/* error extending timestamp */
#define RSGTE_INVLD_RECCNT 7/* mismatch between actual records and records
                               given in block-sig record */
#define RSGTE_INVLHDR 8/* invalid file header */
#define RSGTE_EOF 9 	/* specific EOF */
#define RSGTE_MISS_REC_HASH 10 /* record hash missing when expected */
#define RSGTE_MISS_TREE_HASH 11 /* tree hash missing when expected */
#define RSGTE_INVLD_REC_HASH 12 /* invalid record hash (failed verification) */
#define RSGTE_INVLD_TREE_HASH 13 /* invalid tree hash (failed verification) */
#define RSGTE_INVLD_REC_HASHID 14 /* invalid record hash ID (failed verification) */
#define RSGTE_INVLD_TREE_HASHID 15 /* invalid tree hash ID (failed verification) */
#define RSGTE_MISS_BLOCKSIG 16 /* block signature record missing when expected */
#define RSGTE_INVLD_TIMESTAMP 17 /* RFC3161 timestamp is invalid */
#define RSGTE_TS_DERDECODE 18 /* error DER-Decoding a timestamp */
#define RSGTE_TS_DERENCODE 19 /* error DER-Encoding a timestamp */
#define RSGTE_HASH_CREATE 20 /* error creating a hash */

/* the following function maps RSGTE_* state to a string - must be updated
 * whenever a new state is added.
 * Note: it is thread-safe to call this function, as it returns a pointer
 * into constant memory pool.
 */
static inline char *
RSGTE2String(int err)
{
	switch(err) {
	case 0:
		return "success";
	case RSGTE_IO:
		return "i/o error";
	case RSGTE_FMT:
		return "data format error";
	case RSGTE_INVLTYP:
		return "invalid/unexpected tlv record type";
	case RSGTE_OOM:
		return "out of memory";
	case RSGTE_LEN:
		return "length record problem";
	case RSGTE_TS_EXTEND:
		return "error extending timestamp";
	case RSGTE_INVLD_RECCNT:
		return "mismatch between actual record count and number in block signature record";
	case RSGTE_INVLHDR:
		return "invalid file header";
	case RSGTE_EOF:
		return "EOF";
	case RSGTE_MISS_REC_HASH:
		return "record hash missing";
	case RSGTE_MISS_TREE_HASH:
		return "tree hash missing";
	case RSGTE_INVLD_REC_HASH:
		return "record hash mismatch";
	case RSGTE_INVLD_TREE_HASH:
		return "tree hash mismatch";
	case RSGTE_INVLD_REC_HASHID:
		return "invalid record hash ID";
	case RSGTE_INVLD_TREE_HASHID:
		return "invalid tree hash ID";
	case RSGTE_MISS_BLOCKSIG:
		return "missing block signature record";
	case RSGTE_INVLD_TIMESTAMP:
		return "RFC3161 timestamp invalid";
	case RSGTE_TS_DERDECODE:
		return "error DER-decoding RFC3161 timestamp";
	case RSGTE_TS_DERENCODE:
		return "error DER-encoding RFC3161 timestamp";
	case RSGTE_HASH_CREATE:
		return "error creating hash";
	default:
		return "unknown error";
	}
}


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
hashIdentifier(enum GTHashAlgorithm hashID)
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
static inline enum GTHashAlgorithm
hashID2Alg(uint8_t hashID)
{
	switch(hashID) {
	case 0x00:
		return GT_HASHALG_SHA1;
	case 0x02:
		return GT_HASHALG_RIPEMD160;
	case 0x03:
		return GT_HASHALG_SHA224;
	case 0x01:
		return GT_HASHALG_SHA256;
	case 0x04:
		return GT_HASHALG_SHA384;
	case 0x05:
		return GT_HASHALG_SHA512;
	default:
		return 0xff;
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
int rsgtInit(char *usragent);
void rsgtExit(void);
gtctx rsgtCtxNew(void);
void rsgtsetErrFunc(gtctx ctx, void (*func)(void*, unsigned char *), void *usrptr);
gtfile rsgtCtxOpenFile(gtctx ctx, unsigned char *logfn);
int rsgtfileDestruct(gtfile gf);
void rsgtCtxDel(gtctx ctx);
void sigblkInit(gtfile gf);
int sigblkAddRecord(gtfile gf, const unsigned char *rec, const size_t len);
int sigblkFinish(gtfile gf);
imprint_t * rsgtImprintFromGTDataHash(GTDataHash *hash);
void rsgtimprintDel(imprint_t *imp);
/* reader functions */
int rsgt_tlvrdHeader(FILE *fp, unsigned char *hdr);
int rsgt_tlvrd(FILE *fp, tlvrecord_t *rec, void *obj);
void rsgt_tlvprint(FILE *fp, uint16_t tlvtype, void *obj, uint8_t verbose);
void rsgt_printBLOCK_SIG(FILE *fp, block_sig_t *bs, uint8_t verbose);
int rsgt_getBlockParams(FILE *fp, uint8_t bRewind, block_sig_t **bs, uint8_t *bHasRecHashes, uint8_t *bHasIntermedHashes);
int rsgt_chkFileHdr(FILE *fp, char *expect);
gtfile rsgt_vrfyConstruct_gf(void);
void rsgt_vrfyBlkInit(gtfile gf, block_sig_t *bs, uint8_t bHasRecHashes, uint8_t bHasIntermedHashes);
int rsgt_vrfy_nextRec(block_sig_t *bs, gtfile gf, FILE *sigfp, FILE *nsigfp, unsigned char *rec, size_t len, gterrctx_t *ectx);
int verifyBLOCK_SIG(block_sig_t *bs, gtfile gf, FILE *sigfp, FILE *nsigfp, uint8_t bExtend, gterrctx_t *ectx);
void rsgt_errctxInit(gterrctx_t *ectx);
void rsgt_errctxExit(gterrctx_t *ectx);
void rsgt_errctxSetErrRec(gterrctx_t *ectx, char *rec);
void rsgt_errctxFrstRecInBlk(gterrctx_t *ectx, char *rec);
void rsgt_objfree(uint16_t tlvtype, void *obj);


/* TODO: replace these? */
int hash_m(gtfile gf, GTDataHash **m);
int hash_r(gtfile gf, GTDataHash **r, const unsigned char *rec, const size_t len);
int hash_node(gtfile gf, GTDataHash **node, GTDataHash *m, GTDataHash *r, uint8_t level);
extern char *rsgt_read_puburl; /**< url of publication server */
extern uint8_t rsgt_read_showVerified;

#endif  /* #ifndef INCLUDED_LIBRSGT_H */
