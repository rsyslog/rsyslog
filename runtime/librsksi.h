/* librsksi.h - rsyslog's KSI support library
 *
 * Copyright 2013-2015 Adiscon GmbH.
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
#ifndef INCLUDED_LIBRSKSI_H
#define INCLUDED_LIBRSKSI_H
#include <ksi/ksi.h>
typedef enum KSI_HashAlgorithm_en KSI_HashAlgorithm;

/* Max number of roots inside the forest. This permits blocks of up to
 * 2^MAX_ROOTS records. We assume that 64 is sufficient for all use
 * cases ;) [and 64 is not really a waste of memory, so we do not even
 * try to work with reallocs and such...]
 */
/*#define MAX_ROOTS 64
#define LOGSIGHDR "LOGSIG10"
*/ 

/* context for gt calls. This primarily serves as a container for the
 * config settings. The actual file-specific data is kept in ksifile.
 */
struct rsksictx_s {
	KSI_CTX *ksi_ctx;	/* libksi's context object */
	KSI_HashAlgorithm hashAlg;
	uint8_t bKeepRecordHashes;
	uint8_t bKeepTreeHashes;
	uint64_t blockSizeLimit;
	char *timestamper;
	void (*errFunc)(void *, unsigned char*);
	void *usrptr; /* for error function */
};
typedef struct rsksictx_s *rsksictx;
typedef struct ksifile_s *ksifile;
typedef struct ksierrctx_s ksierrctx_t;

/* this describes a file, as far as librsksi is concerned */
struct ksifile_s {
	/* the following data items are mirrored from rsksictx to
	 * increase cache hit ratio (they are frequently accesed).
	 */
	KSI_HashAlgorithm hashAlg;
	uint8_t bKeepRecordHashes;
	uint8_t bKeepTreeHashes;
	/* end mirrored properties */
	uint8_t disabled; /* permits to disable this file --> set to 1 */
	uint64_t blockSizeLimit;
	uint8_t *IV; /* initial value for blinding masks */
	imprint_t *x_prev;		/* last leaf hash (maybe of previous block)		--> preserve on term */
	unsigned char *sigfilename;
	unsigned char *statefilename;
	int fd;
	uint64_t nRecords;  /* current number of records in current block */
	uint64_t bInBlk;    /* are we currently inside a blk --> need to finish on close */
	int8_t nRoots;
	/* algo engineering: roots structure is split into two arrays
	 * in order to improve cache hits.
	 */
	int8_t roots_valid[MAX_ROOTS];
	KSI_DataHash *roots_hash[MAX_ROOTS];
	/* data members for the associated TLV file */
	char	tlvBuf[4096];
	int	tlvIdx; /* current index into tlvBuf */
	rsksictx ctx;
};

/* The following structure describes the "error context" to be used
 * for verification and similiar reader functions. While verifying,
 * we need some information (like filenames or block numbers) that
 * is not readily available from the other objects (or not even known
 * to librsksi). In order to provide meaningful error messages, this
 * information must be passed in from the external callers. In order
 * to centralize information (and make it more manageable), we use
 * ths error context here, which contains everything needed to
 * generate good error messages. Members of this structure are
 * maintained both by library users (the callers) as well as
 * the library itself. Who does what simply depends on who has
 * the relevant information.
 */
struct ksierrctx_s {
	FILE *fp;	/**< file for error messages */
	char *filename;
	uint8_t verbose;
	uint64_t recNumInFile;
	uint64_t recNum;
	uint64_t blkNum;
	uint8_t treeLevel;
	KSI_DataHash *computedHash;
	KSI_DataHash *lefthash, *righthash; /* hashes to display if tree hash fails */
	imprint_t *fileHash;
	int ksistate;	/* status from last relevant GT.*() function call */
	char *errRec;
	char *frstRecInBlk; /* This holds the first message seen inside the current block */
};

/* the following defines the ksistate file record. Currently, this record
 * is fixed, we may change that over time.
 */
struct rsksistatefile {
	char hdr[9];	/* must be "KSISTAT10" */
	uint8_t hashID;
	uint8_t lenHash;
	/* after that, the hash value is contained within the file */
};

/* error states */
#define RSGTE_SUCCESS 0 /* Success state */
#define RSGTE_IO 1 	/* any kind of io error */
#define RSGTE_FMT 2	/* data fromat error */
#define RSGTE_INVLTYP 3	/* invalid TLV type record (unexcpected at this point) */
#define RSGTE_OOM 4	/* ran out of memory */
#define RSGTE_LEN 5	/* error related to length records */
#define RSGTE_SIG_EXTEND 6/* error extending signature */
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
#define RSGTE_INVLD_SIGNATURE 17 /* Signature is invalid (KSI_Signature_verifyDataHash)*/
#define RSGTE_TS_CREATEHASH 18 /* error creating HASH (KSI_DataHash_create) */
#define RSGTE_TS_DERENCODE 19 /* error DER-Encoding a timestamp */
#define RSGTE_HASH_CREATE 20 /* error creating a hash */
#define RSGTE_END_OF_SIG 21 /* unexpected end of signature - more log line exist */
#define RSGTE_END_OF_LOG 22 /* unexpected end of log file - more signatures exist */
#define RSGTE_EXTRACT_HASH 23 /* error extracting hashes for record */

/* the following function maps RSGTE_* state to a string - must be updated
 * whenever a new state is added.
 * Note: it is thread-safe to call this function, as it returns a pointer
 * into constant memory pool.
 */
static inline char *
RSKSIE2String(int err)
{
	switch(err) {
	case RSGTE_SUCCESS:
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
	case RSGTE_SIG_EXTEND:
		return "error extending signature";
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
	case RSGTE_INVLD_SIGNATURE:
		return "Signature invalid";
	case RSGTE_TS_CREATEHASH:
		return "error creating HASH";
	case RSGTE_TS_DERENCODE:
		return "error DER-encoding RFC3161 timestamp";
	case RSGTE_HASH_CREATE:
		return "error creating hash";
	case RSGTE_END_OF_SIG:
		return "unexpected end of signature";
	case RSGTE_END_OF_LOG:
		return "unexpected end of log";
	case RSGTE_EXTRACT_HASH:
		return "either record-hash, left-hash or right-hash was empty";
	default:
		return "unknown error";
	}
}


static inline uint16_t
hashOutputLengthOctetsKSI(uint8_t hashID)
{
	switch(hashID) {
	case KSI_HASHALG_SHA1: /** The SHA-1 algorithm. */
		return 20;
	case KSI_HASHALG_SHA2_256: /** The SHA-256 algorithm. */
		return 32;
	case KSI_HASHALG_RIPEMD160: /** The RIPEMD-160 algorithm. */
		return 20;
	case KSI_HASHALG_SHA2_224: /** The SHA-224 algorithm. */
		return 28;
	case KSI_HASHALG_SHA2_384: /** The SHA-384 algorithm. */
		return 48;
	case KSI_HASHALG_SHA2_512: /** The SHA-512 algorithm. */
		return 64;
	case KSI_HASHALG_SHA3_244: /** The SHA3-244 algorithm. */
		return 28;
	case KSI_HASHALG_SHA3_256: /** The SHA3-256 algorithm. */
		return 32;
	case KSI_HASHALG_SHA3_384: /** The SHA3-384 algorithm. */
		return 48;
	case KSI_HASHALG_SHA3_512: /** The SHA3-512 algorithm */
		return 64;
	case KSI_HASHALG_SM3: /** The SM3 algorithm.*/
		return 32;
	default:return 32;
	}
}

static inline uint8_t
hashIdentifierKSI(KSI_HashAlgorithm hashID)
{
	switch(hashID) {
	case KSI_HASHALG_SHA1: /** The SHA-1 algorithm. */
		return 0x00;
	case KSI_HASHALG_SHA2_256: /** The SHA-256 algorithm. */
		return 0x01;
	case KSI_HASHALG_RIPEMD160: /** The RIPEMD-160 algorithm. */
		return 0x02;
	case KSI_HASHALG_SHA2_224: /** The SHA-224 algorithm. */
		return 0x03;
	case KSI_HASHALG_SHA2_384: /** The SHA-384 algorithm. */
		return 0x04;
	case KSI_HASHALG_SHA2_512: /** The SHA-512 algorithm. */
		return 0x05;
	case KSI_HASHALG_SHA3_244: /** The SHA3-244 algorithm. */
		return 0x07;
	case KSI_HASHALG_SHA3_256: /** The SHA3-256 algorithm. */
		return 0x08;
	case KSI_HASHALG_SHA3_384: /** The SHA3-384 algorithm. */
		return 0x09;
	case KSI_HASHALG_SHA3_512: /** The SHA3-512 algorithm */
		return 0x0a;
	case KSI_HASHALG_SM3: /** The SM3 algorithm.*/
		return 0x0b;
	default:return 0xff;
	}
}
static inline char *
hashAlgNameKSI(uint8_t hashID)
{
	switch(hashID) {
	case KSI_HASHALG_SHA1:
		return "SHA1";
	case KSI_HASHALG_SHA2_256:
		return "SHA2-256";
	case KSI_HASHALG_RIPEMD160:
		return "RIPEMD-160";
	case KSI_HASHALG_SHA2_224:
		return "SHA2-224";
	case KSI_HASHALG_SHA2_384:
		return "SHA2-384";
	case KSI_HASHALG_SHA2_512:
		return "SHA2-512";
	case KSI_HASHALG_SHA3_244:
		return "SHA3-224";
	case KSI_HASHALG_SHA3_256:
		return "SHA3-256";
	case KSI_HASHALG_SHA3_384:
		return "SHA3-384";
	case KSI_HASHALG_SHA3_512:
		return "SHA3-512";
	case KSI_HASHALG_SM3:
		return "SM3";
	default:return "[unknown]";
	}
}
static inline KSI_HashAlgorithm
hashID2AlgKSI(uint8_t hashID)
{
	switch(hashID) {
	case 0x00:
		return KSI_HASHALG_SHA1;
	case 0x01:
		return KSI_HASHALG_SHA2_256;
	case 0x02:
		return KSI_HASHALG_RIPEMD160;
	case 0x03:
		return KSI_HASHALG_SHA2_224;
	case 0x04:
		return KSI_HASHALG_SHA2_384;
	case 0x05:
		return KSI_HASHALG_SHA2_512;
	case 0x07:
		return KSI_HASHALG_SHA3_244;
	case 0x08:
		return KSI_HASHALG_SHA3_256;
	case 0x09:
		return KSI_HASHALG_SHA3_384;
	case 0x0a:
		return KSI_HASHALG_SHA3_512;
	case 0x0b:
		return KSI_HASHALG_SM3;
	default:
		return 0xff;
	}
}
static inline uint16_t
getIVLenKSI(block_hdr_t *bh)
{
	return hashOutputLengthOctetsKSI(bh->hashID);
}
static inline void
rsksiSetBlockSizeLimit(rsksictx ctx, uint64_t limit)
{
	ctx->blockSizeLimit = limit;
}
static inline void
rsksiSetKeepRecordHashes(rsksictx ctx, int val)
{
	ctx->bKeepRecordHashes = val;
}
static inline void
rsksiSetKeepTreeHashes(rsksictx ctx, int val)
{
	ctx->bKeepTreeHashes = val;
}

int rsksiSetAggregator(rsksictx ctx, char *uri, char *loginid, char *key);
int rsksiSetHashFunction(rsksictx ctx, char *algName);
int rsksiInit(char *usragent);
void rsksiExit(void);
rsksictx rsksiCtxNew(void);
void rsksisetErrFunc(rsksictx ctx, void (*func)(void*, unsigned char *), void *usrptr);
void reportKSIAPIErr(rsksictx ctx, ksifile ksi, char *apiname, int ecode); 
ksifile rsksiCtxOpenFile(rsksictx ctx, unsigned char *logfn);
int rsksifileDestruct(ksifile ksi);
void rsksiCtxDel(rsksictx ctx);
void sigblkInitKSI(ksifile ksi);
int sigblkAddRecordKSI(ksifile ksi, const unsigned char *rec, const size_t len);
int sigblkFinishKSI(ksifile ksi);
int rsksiIntoImprintFromKSI_DataHash(imprint_t* imp, ksifile ksi, KSI_DataHash *hash);
imprint_t* rsksiImprintFromKSI_DataHash(ksifile ksi, KSI_DataHash *hash);
void rsksiimprintDel(imprint_t *imp);
/* reader functions */
int rsksi_tlvrdHeader(FILE *fp, unsigned char *hdr);
int rsksi_tlvrd(FILE *fp, tlvrecord_t *rec, void *obj);
void rsksi_tlvprint(FILE *fp, uint16_t tlvtype, void *obj, uint8_t verbose);
void rsksi_printBLOCK_HDR(FILE *fp, block_hdr_t *bh, uint8_t verbose);
void rsksi_printBLOCK_SIG(FILE *fp, block_sig_t *bs, uint8_t verbose);
int rsksi_getBlockParams(ksifile ksi, FILE *fp, uint8_t bRewind, block_sig_t **bs, block_hdr_t **bh, uint8_t *bHasRecHashes, uint8_t *bHasIntermedHashes);
int rsksi_getExcerptBlockParams(ksifile ksi, FILE *fp, uint8_t bRewind, block_sig_t **bs, block_hdr_t **bh); 
int rsksi_chkFileHdr(FILE *fp, char *expect, uint8_t verbose);
ksifile rsksi_vrfyConstruct_gf(void);
void rsksi_vrfyBlkInit(ksifile ksi, block_hdr_t *bh, uint8_t bHasRecHashes, uint8_t bHasIntermedHashes);
int rsksi_vrfy_nextRec(ksifile ksi, FILE *sigfp, FILE *nsigfp, unsigned char *rec, size_t len, ksierrctx_t *ectx);
	int rsksi_vrfy_nextRecExtract(ksifile ksi, FILE *sigfp, FILE *nsigfp, unsigned char *rec, size_t len, ksierrctx_t *ectx, block_hashchain_t *hashchain, int storehashchain); 
	int rsksi_vrfy_nextHashChain(ksifile ksi, block_sig_t *bs, FILE *sigfp, unsigned char *rec, size_t len, ksierrctx_t *ectx);
int verifyBLOCK_HDRKSI(ksifile ksi, FILE *sigfp, FILE *nsigfp, tlvrecord_t* tlvrec);
int verifyBLOCK_SIGKSI(block_sig_t *bs, ksifile ksi, FILE *sigfp, FILE *nsigfp, uint8_t bExtend, KSI_DataHash *ksiHash, ksierrctx_t *ectx);
void rsksi_errctxInit(ksierrctx_t *ectx);
void rsksi_errctxExit(ksierrctx_t *ectx);
void rsksi_errctxSetErrRec(ksierrctx_t *ectx, char *rec);
void rsksi_errctxFrstRecInBlk(ksierrctx_t *ectx, char *rec);
void rsksi_objfree(uint16_t tlvtype, void *obj);
void rsksi_set_debug(int iDebug); 
int rsksi_ConvertSigFile(char* name, FILE *oldsigfp, FILE *newsigfp, int verbose); 
	
	int rsksi_WriteHashChain(FILE *newsigfp, block_hashchain_t *hashchain, block_sig_t *bsIn, int verbose); 
	int rsksi_ExtractBlockSignature(FILE *newsigfp, ksifile ksi, block_sig_t *bsIn, ksierrctx_t *ectx, int verbose); 
	int rsksi_tlvwrite(FILE *fp, tlvrecord_t *rec); 
	int rsksi_tlvRecDecode(tlvrecord_t *rec, void *obj); 
	int rsksi_tlvDecodeIMPRINT(tlvrecord_t *rec, imprint_t **imprint); 
	int rsksi_tlvDecodeHASHCHAIN(tlvrecord_t *rec, block_hashchain_t **blhashchain); 
	int verifySigblkFinish(ksifile ksi, KSI_DataHash **pRoot); 
	int verifySigblkFinishChain(ksifile ksi, block_hashchain_t *hashchain, KSI_DataHash **pRoot, ksierrctx_t *ectx); 

	void outputHash(FILE *fp, const char *hdr, const uint8_t *data, const uint16_t len, const uint8_t verbose); 
	void outputKSIHash(FILE *fp, char *hdr, const KSI_DataHash *const __restrict__ hash, const uint8_t verbose); 

/* TODO: replace these? */
int hash_m_ksi(ksifile ksi, KSI_DataHash **m);
int hash_r_ksi(ksifile ksi, KSI_DataHash **r, const unsigned char *rec, const size_t len);
int hash_node_ksi(ksifile ksi, KSI_DataHash **node, KSI_DataHash *m, KSI_DataHash *r, uint8_t level);
extern char *rsksi_read_puburl;		/**< url of publication server */
extern char *rsksi_extend_puburl;	/**< url of extension server */
extern char *rsksi_userid;			/**< userid for extension server */
extern char *rsksi_userkey;			/**< userkey for extension server */
extern uint8_t rsksi_read_showVerified;
extern int RSKSI_FLAG_TLV16_RUNTIME;
extern int RSKSI_FLAG_NONCRIT_RUNTIME; 

#endif  /* #ifndef INCLUDED_LIBRSKSI_H */
