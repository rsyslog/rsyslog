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

/* context for gt calls. This primarily serves as a container for the
 * config settings. The actual file-specific data is kept in ksifile.
 */
struct rsksictx_s {
	KSI_CTX *ksi_ctx;	/* libksi's context object */
	KSI_HashAlgorithm hashAlg;
	uint8_t bKeepRecordHashes;
	uint8_t bKeepTreeHashes;
	uint64_t blockSizeLimit;
	uid_t	fileUID;	/* IDs for creation */
	uid_t	dirUID;
	gid_t	fileGID;
	gid_t	dirGID;
	int fCreateMode; /* mode to use when creating files */
	int fDirCreateMode; /* mode to use when creating files */
	char *timestamper;
	void (*errFunc)(void *, unsigned char*);
	void (*logFunc)(void *, unsigned char*);
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
#define RSGTE_CONFIG_ERROR 24 /* Configuration error */
#define RSGTE_NETWORK_ERROR 25 /* Network error */
#define RSGTE_MISS_KSISIG 26 /* KSI signature missing */

const char * RSKSIE2String(int err);
uint16_t hashOutputLengthOctetsKSI(uint8_t hashID);
uint8_t hashIdentifierKSI(KSI_HashAlgorithm hashID);
const char * hashAlgNameKSI(uint8_t hashID);
KSI_HashAlgorithm hashID2AlgKSI(uint8_t hashID);

#define getIVLenKSI(bh) (hashOutputLengthOctetsKSI((bh)->hashID))
#define rsksiSetBlockSizeLimit(ctx, limit) ((ctx)->blockSizeLimit = limit)
#define rsksiSetKeepRecordHashes(ctx, val) ((ctx)->bKeepRecordHashes = val)
#define rsksiSetKeepTreeHashes(ctx, val) ((ctx)->bKeepTreeHashes = val)
#define rsksiSetFileUID(ctx, val) ((ctx)->fileUID = val)	/* IDs for creation */
#define rsksiSetDirUID(ctx, val) ((ctx)->dirUID = val)
#define rsksiSetFileGID(ctx, val) ((ctx)->fileGID= val)
#define rsksiSetDirGID(ctx, val) ((ctx)->dirGID = val)
#define rsksiSetCreateMode(ctx, val) ((ctx)->fCreateMode= val)
#define rsksiSetDirCreateMode(ctx, val) ((ctx)->fDirCreateMode = val)



int rsksiSetAggregator(rsksictx ctx, char *uri, char *loginid, char *key);
int rsksiSetHashFunction(rsksictx ctx, char *algName);
int rsksiInit(char *usragent);
void rsksiExit(void);
rsksictx rsksiCtxNew(void);
void rsksisetErrFunc(rsksictx ctx, void (*func)(void*, unsigned char *), void *usrptr);
void rsksisetLogFunc(rsksictx ctx, void (*func)(void*, unsigned char *), void *usrptr);
void reportKSIAPIErr(rsksictx ctx, ksifile ksi, const char *apiname, int ecode);
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
int rsksi_getBlockParams(FILE *fp, uint8_t bRewind, block_sig_t **bs, block_hdr_t **bh, uint8_t *bHasRecHashes,
uint8_t *bHasIntermedHashes);
int rsksi_getExcerptBlockParams(FILE *fp, uint8_t bRewind, block_sig_t **bs, block_hdr_t **bh); 
int rsksi_chkFileHdr(FILE *fp, char *expect, uint8_t verbose);
ksifile rsksi_vrfyConstruct_gf(void);
void rsksi_vrfyBlkInit(ksifile ksi, block_hdr_t *bh, uint8_t bHasRecHashes, uint8_t bHasIntermedHashes);
int rsksi_vrfy_nextRec(ksifile ksi, FILE *sigfp, FILE *nsigfp, unsigned char *rec, size_t len, ksierrctx_t *ectx);
int rsksi_vrfy_nextRecExtract(ksifile ksi, FILE *sigfp, FILE *nsigfp, unsigned char *rec, size_t len,
ksierrctx_t *ectx, block_hashchain_t *hashchain, int storehashchain); 
int rsksi_vrfy_nextHashChain(ksifile ksi, block_sig_t *bs, FILE *sigfp, unsigned char *rec, size_t len,
ksierrctx_t *ectx);
int verifyBLOCK_HDRKSI(FILE *sigfp, FILE *nsigfp, tlvrecord_t* tlvrec);
int verifyBLOCK_SIGKSI(block_sig_t *bs, ksifile ksi, FILE *sigfp, FILE *nsigfp, uint8_t bExtend,
KSI_DataHash *ksiHash, ksierrctx_t *ectx);
void rsksi_errctxInit(ksierrctx_t *ectx);
void rsksi_errctxExit(ksierrctx_t *ectx);
void rsksi_errctxSetErrRec(ksierrctx_t *ectx, char *rec);
void rsksi_errctxFrstRecInBlk(ksierrctx_t *ectx, char *rec);
void rsksi_objfree(uint16_t tlvtype, void *obj);
void rsksi_set_debug(int iDebug); 
int rsksi_ConvertSigFile(FILE *oldsigfp, FILE *newsigfp, int verbose); 
int rsksi_WriteHashChain(FILE *newsigfp, block_hashchain_t *hashchain, int verbose); 
int rsksi_ExtractBlockSignature(FILE *newsigfp, block_sig_t *bsIn); 
int rsksi_tlvwrite(FILE *fp, tlvrecord_t *rec); 
int rsksi_tlvRecDecode(tlvrecord_t *rec, void *obj); 
int rsksi_tlvDecodeIMPRINT(tlvrecord_t *rec, imprint_t **imprint); 
int rsksi_tlvDecodeHASHCHAIN(tlvrecord_t *rec, block_hashchain_t **blhashchain); 
int verifySigblkFinish(ksifile ksi, KSI_DataHash **pRoot); 
int verifySigblkFinishChain(ksifile ksi, block_hashchain_t *hashchain, KSI_DataHash **pRoot, ksierrctx_t *ectx); 
void outputHash(FILE *fp, const char *hdr, const uint8_t *data, const uint16_t len, const uint8_t verbose); 
void outputKSIHash(FILE *fp, const char *hdr, const KSI_DataHash *const __restrict__ hash, const uint8_t verbose); 
int rsksi_setDefaultConstraint(ksifile ksi, char *stroid, char *strvalue);

/* TODO: replace these? */
int hash_m_ksi(ksifile ksi, KSI_DataHash **m);
int hash_r_ksi(ksifile ksi, KSI_DataHash **r, const unsigned char *rec, const size_t len);
int hash_node_ksi(ksifile ksi, KSI_DataHash **node, KSI_DataHash *m, KSI_DataHash *r, uint8_t level);
extern const char *rsksi_read_puburl;		/**< url of publication server */
extern const char *rsksi_extend_puburl;	/**< url of extension server */
extern const char *rsksi_userid;			/**< userid for extension server */
extern const char *rsksi_userkey;			/**< userkey for extension server */
extern uint8_t rsksi_read_showVerified;
extern int RSKSI_FLAG_TLV16_RUNTIME;
extern int RSKSI_FLAG_NONCRIT_RUNTIME; 

#endif  /* #ifndef INCLUDED_LIBRSKSI_H */
