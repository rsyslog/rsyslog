/* lib_ksils12.c - rsyslog's KSI-LS12 support library
 *
 * Regarding the online algorithm for Merkle tree signing. Expected 
 * calling sequence is:
 *
 * sigblkConstruct
 * for each signature block:
 *    sigblkInitKSI
 *    for each record:
 *       sigblkAddRecordKSI
 *    sigblkFinishKSI
 * sigblkDestruct
 *
 * Obviously, the next call after sigblkFinsh must either be to 
 * sigblkInitKSI or sigblkDestruct (if no more signature blocks are
 * to be emitted, e.g. on file close). sigblkDestruct saves state
 * information (most importantly last block hash) and sigblkConstruct
 * reads (or initilizes if not present) it.
 *
 * Copyright 2013-2017 Adiscon GmbH and Guardtime, Inc.
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <limits.h>
#define MAXFNAME 1024

#include <ksi/ksi.h>
#include <ksi/tlv_element.h>
#include <ksi/hash.h>
#include "lib_ksils12.h"
#include "lib_ksi_queue.h"

typedef unsigned char uchar;
#ifndef VERSION
#define VERSION "no-version"
#endif

#define KSI_BUF_SIZE 4096

static const char *blockFileSuffix = ".logsig.parts/blocks.dat";
static const char *sigFileSuffix = ".logsig.parts/block-signatures.dat";
static const char *ls12FileSuffix = ".logsig";
static const char *blockCloseReason = "com.guardtime.blockCloseReason";


#define LS12_FILE_HEADER "LOGSIG12"
#define LS12_BLOCKFILE_HEADER "LOG12BLK"
#define LS12_SIGFILE_HEADER "LOG12SIG"

/* Worker queue item type identifier */
typedef enum QITEM_type_en {
	QITEM_SIGNATURE_REQUEST = 0x00,
	QITEM_CLOSE_FILE,
	QITEM_NEW_FILE,
	QITEM_QUIT
} QITEM_type;

/* Worker queue job item */
typedef struct QueueItem_st {
	QITEM_type type;
	void *arg;
	uint64_t intarg1;
	uint64_t intarg2;
} QueueItem;

bool add_queue_item(rsksictx ctx, QITEM_type type, void *arg, uint64_t intarg1, uint64_t intarg2);
void *signer_thread(void *arg);

static void __attribute__((format(printf, 2, 3)))
report(rsksictx ctx, const char *errmsg, ...) {
	char buf[1024];
	va_list args;
	va_start(args, errmsg);

	int r = vsnprintf(buf, sizeof (buf), errmsg, args);
	buf[sizeof(buf)-1] = '\0';

	if(ctx->logFunc == NULL)
		return;

	if(r>0 && r<(int)sizeof(buf))
		ctx->logFunc(ctx->usrptr, (uchar*)buf);
	else
		ctx->logFunc(ctx->usrptr, (uchar*)errmsg);
}

static void
reportErr(rsksictx ctx, const char *const errmsg)
{
	if(ctx->errFunc == NULL)
		goto done;
	ctx->errFunc(ctx->usrptr, (uchar*)errmsg);
done:	return;
}

void
reportKSIAPIErr(rsksictx ctx, ksifile ksi, const char *apiname, int ecode)
{
	char errbuf[4096];
	snprintf(errbuf, sizeof(errbuf), "%s[%s:%d]: %s",
		(ksi == NULL) ? (uchar*) "" : ksi->blockfilename,
		apiname, ecode, KSI_getErrorString(ecode));
	errbuf[sizeof(errbuf)-1] = '\0';
	reportErr(ctx, errbuf);
}

void
rsksisetErrFunc(rsksictx ctx, void (*func)(void*, uchar *), void *usrptr)
{
	ctx->usrptr = usrptr;
	ctx->errFunc = func;
}

void
rsksisetLogFunc(rsksictx ctx, void (*func)(void*, uchar *), void *usrptr)
{
	ctx->usrptr = usrptr;
	ctx->logFunc = func;
}

static ksifile
rsksifileConstruct(rsksictx ctx) {
	ksifile ksi = NULL;
	if ((ksi = calloc(1, sizeof (struct ksifile_s))) == NULL)
		goto done;
	ksi->ctx = ctx;
	ksi->hashAlg = ctx->hashAlg;
	ksi->blockTimeLimit = ctx->blockTimeLimit;
	ksi->blockSizeLimit = (1 << ctx->blockLevelLimit) - 1;
	ksi->bKeepRecordHashes = ctx->bKeepRecordHashes;
	ksi->bKeepTreeHashes = ctx->bKeepTreeHashes;
	ksi->lastLeaf[0] = ctx->hashAlg;

done:
	return ksi;
}

/* return the actual length in to-be-written octets of an integer */
static uint8_t
tlvGetIntSize(uint64_t val) {
	uint8_t n = 0;
	while (val != 0) {
		val >>= 8;
		n++;
	}
	return n;
}

static int
tlvWriteOctetString(FILE *f, const uint8_t *data, uint16_t len) {
	if (fwrite(data, len, 1, f) != 1)
		return RSGTE_IO;
	return 0;
}

static int
tlvWriteHeader8(FILE *f, int flags, uint8_t tlvtype, int len) {
	unsigned char buf[2];
	assert((flags & RSGT_TYPE_MASK) == 0);
	assert((tlvtype & RSGT_TYPE_MASK) == tlvtype);
	buf[0] = (flags & ~RSGT_FLAG_TLV16) | tlvtype;
	buf[1] = len & 0xff;

	return tlvWriteOctetString(f, buf, 2);
} 

static int
tlvWriteHeader16(FILE *f, int flags, uint16_t tlvtype, uint16_t len)
{
	uint16_t typ;
	unsigned char buf[4];
	assert((flags & RSGT_TYPE_MASK) == 0);
	assert((tlvtype >> 8 & RSGT_TYPE_MASK) == (tlvtype >> 8));
	typ = ((flags | RSGT_FLAG_TLV16) << 8) | tlvtype;

	buf[0] = typ >> 8;
	buf[1] = typ & 0xff;
	buf[2] = (len >> 8) & 0xff;
	buf[3] = len & 0xff;

	return tlvWriteOctetString(f, buf, 4);
}

static int
tlvGetHeaderSize(uint16_t tag, size_t size) {
	if (tag <= RSGT_TYPE_MASK && size <= 0xff)
		return 2;
	if ((tag >> 8) <= RSGT_TYPE_MASK && size <= 0xffff)
		return 4;
	return 0;
}

static int
tlvWriteHeader(FILE *f, int flags, uint16_t tlvtype, uint16_t len) {
	int headersize = tlvGetHeaderSize(tlvtype, flags);
	if (headersize == 2)
		return tlvWriteHeader8(f, flags, tlvtype, len);
	else if (headersize == 4)
		return tlvWriteHeader16(f, flags, tlvtype, len);
	else
		return 0;
}

static int
tlvWriteOctetStringTLV(FILE *f, int flags, uint16_t tlvtype, const uint8_t *data, uint16_t len) {
	if (tlvWriteHeader(f, flags, tlvtype, len) != 0)
		return RSGTE_IO;

	if (fwrite(data, len, 1, f) != 1)
		return RSGTE_IO;

	return 0;
}

static int
tlvWriteInt64TLV(FILE *f, int flags, uint16_t tlvtype, uint64_t val) {
	unsigned char buf[8];
	uint8_t count = tlvGetIntSize(val);

	if (tlvWriteHeader(f, flags, tlvtype, count) != 0)
		return RSGTE_IO;

	uint64_t nTmp = val;
	for (int i = count - 1; i >= 0; i--) {
		buf[i] = 0xFF & nTmp;
		nTmp = nTmp >> 8;
	}

	if (fwrite(buf, count, 1, f) != 1)
		return RSGTE_IO;

	return 0;
}

static int
tlvWriteHashKSI(ksifile ksi, uint16_t tlvtype, KSI_DataHash *rec) {
	int r;
	const unsigned char *imprint;
	size_t imprint_len;
	r = KSI_DataHash_getImprint(rec, &imprint, &imprint_len);
	if (r != KSI_OK) {
		reportKSIAPIErr(ksi->ctx, ksi, "KSI_DataHash_getImprint", r);
		return r;
	}

	return tlvWriteOctetStringTLV(ksi->blockFile, 0, tlvtype, imprint, imprint_len);
}

static int
tlvWriteBlockHdrKSI(ksifile ksi) {
	unsigned tlvlen;
	uint8_t hash_algo = ksi->hashAlg;
	int r;

	tlvlen  = 2 + 1 /* hash algo TLV */ +
		2 + KSI_getHashLength(ksi->hashAlg) /* iv */ +
		2 + KSI_getHashLength(ksi->lastLeaf[0]) + 1;
	/* last hash */;

	/* write top-level TLV object block-hdr */
	CHKr(tlvWriteHeader(ksi->blockFile, 0x00, 0x0901, tlvlen));

	/* hash-algo */
	CHKr(tlvWriteOctetStringTLV(ksi->blockFile, 0x00, 0x01, &hash_algo, 1));

	/* block-iv */
	CHKr(tlvWriteOctetStringTLV(ksi->blockFile, 0x00, 0x02,
		ksi->IV, KSI_getHashLength(ksi->hashAlg)));

	/* last-hash */
	CHKr(tlvWriteOctetStringTLV(ksi->blockFile, 0x00, 0x03,
		ksi->lastLeaf, KSI_getHashLength(ksi->lastLeaf[0]) + 1));
done:
	return r;
}

static int
tlvWriteKSISigLS12(FILE *outfile, size_t record_count, uchar *der, uint16_t lenDer) {
	int r = 0;
	int totalSize = 2 + tlvGetIntSize(record_count) + 4 + lenDer;

	CHKr(tlvWriteHeader(outfile, 0x00, 0x0904, totalSize));
	CHKr(tlvWriteInt64TLV(outfile, 0x00, 0x01, record_count));
	CHKr(tlvWriteOctetStringTLV(outfile, 0x00, 0x0905, der, lenDer));
done:
	return r;
}

static int
tlvWriteNoSigLS12(FILE *outfile, size_t record_count, const KSI_DataHash *hash, const char *errorText) {
	int r = 0;
	int totalSize = 0;
	int noSigSize = 0;
	const unsigned char *imprint = NULL;
	size_t imprintLen = 0;

	KSI_DataHash_getImprint(hash, &imprint, &imprintLen);

	noSigSize = 2 + imprintLen + (errorText ? (2 + strlen(errorText) + 1) : 0);
	totalSize = 2 + tlvGetIntSize(record_count) + 2 + noSigSize;

	CHKr(tlvWriteHeader(outfile, 0x00, 0x0904, totalSize));
	CHKr(tlvWriteInt64TLV(outfile, 0x00, 0x01, record_count));
	CHKr(tlvWriteHeader(outfile, 0x00, 0x02, noSigSize));
	CHKr(tlvWriteOctetStringTLV(outfile, 0x00, 0x01, imprint, imprintLen));
	if (errorText)
		CHKr(tlvWriteOctetStringTLV(outfile, 0x00, 0x02, (uint8_t*) errorText, strlen(errorText) + 1));
done:
	return r;
}

static int
tlvCreateMetadata(ksifile ksi, uint64_t record_index, const char *key, 
		const char *value, unsigned char *buffer, size_t *len) {
	int r = 0;
	KSI_TlvElement *metadata = NULL, *attrib_tlv = NULL;
	KSI_Utf8String *key_tlv = NULL, *value_tlv = NULL;
	KSI_Integer *index_tlv = NULL;

	CHKr(KSI_TlvElement_new(&metadata));
	metadata->ftlv.tag = 0x0911;

	CHKr(KSI_Integer_new(ksi->ctx->ksi_ctx, record_index, &index_tlv));
	CHKr(KSI_TlvElement_setInteger(metadata, 0x01, index_tlv));

	CHKr(KSI_TlvElement_new(&attrib_tlv));
	attrib_tlv->ftlv.tag = 0x02;

	CHKr(KSI_Utf8String_new(ksi->ctx->ksi_ctx, key, strlen(key) + 1, &key_tlv));
	CHKr(KSI_TlvElement_setUtf8String(attrib_tlv, 0x01, key_tlv));

	CHKr(KSI_Utf8String_new(ksi->ctx->ksi_ctx, value, strlen(value) + 1, &value_tlv));
	CHKr(KSI_TlvElement_setUtf8String(attrib_tlv, 0x02, value_tlv));

	CHKr(KSI_TlvElement_setElement(metadata, attrib_tlv));

	CHKr(KSI_TlvElement_serialize(metadata, buffer, 0xFFFF, len, 0));

done:
	if (metadata) KSI_TlvElement_free(metadata);
	if (attrib_tlv) KSI_TlvElement_free(attrib_tlv);
	if (key_tlv) KSI_Utf8String_free(key_tlv);
	if (value_tlv) KSI_Utf8String_free(value_tlv);
	if (index_tlv) KSI_Integer_free(index_tlv);

	return r;
}

/* support for old platforms - graceful degrade */
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
/* read rsyslog log state file; if we cannot access it or the
 * contents looks invalid, we flag it as non-present (and thus
 * begin a new hash chain).
 * The context is initialized accordingly.
 */
static bool
ksiReadStateFile(ksifile ksi) {
	int fd = -1;
	struct rsksistatefile sf;
	bool ret = false;

	fd = open((char*)ksi->statefilename, O_RDONLY|O_NOCTTY|O_CLOEXEC, 0600);
	if (fd == -1)
		goto done;

	if (read(fd, &sf, sizeof (sf)) != sizeof (sf)) goto done;
	if (strncmp(sf.hdr, "KSISTAT10", 9)) goto done;

	if (KSI_getHashLength(sf.hashID) != sf.lenHash ||
		KSI_getHashLength(sf.hashID) > KSI_MAX_IMPRINT_LEN - 1)
		goto done;

	if (read(fd, ksi->lastLeaf + 1, sf.lenHash) != sf.lenHash)
		goto done;

	ksi->lastLeaf[0] = sf.hashID;
	ret = true;

done:
	if (!ret) {
		memset(ksi->lastLeaf, 0, sizeof (ksi->lastLeaf));
		ksi->lastLeaf[0] = ksi->hashAlg;
	}

	if (fd != -1)
		close(fd);
	return ret;
}

/* persist all information that we need to re-open and append
 * to a log signature file.
 */
static void
ksiWwriteStateFile(ksifile ksi)
{
	int fd;
	struct rsksistatefile sf;

	fd = open((char*)ksi->statefilename,
		       O_WRONLY|O_CREAT|O_TRUNC|O_NOCTTY|O_CLOEXEC, ksi->ctx->fCreateMode);
	if(fd == -1)
		goto done;
	if (ksi->ctx->fileUID != (uid_t) - 1 || ksi->ctx->fileGID != (gid_t) - 1) {
		/* we need to set owner/group */
		if (fchown(fd, ksi->ctx->fileUID, ksi->ctx->fileGID) != 0) {
			report(ksi->ctx, "lmsig_ksi: chown for file '%s' failed: %s",
				ksi->statefilename, strerror(errno));
		}
	}

	memcpy(sf.hdr, "KSISTAT10", 9);
	sf.hashID = ksi->hashAlg;
	sf.lenHash = KSI_getHashLength(ksi->lastLeaf[0]);
	/* if the write fails, we cannot do anything against that. We check
	 * the condition just to keep the compiler happy.
	 */
	if(write(fd, &sf, sizeof(sf))){};
	if (write(fd, ksi->lastLeaf + 1, sf.lenHash)) {
	};
	close(fd);
done:
	return;
}


static int
ksiCloseSigFile(ksifile ksi) {
	fclose(ksi->blockFile);
	ksi->blockFile = NULL;
	if (ksi->ctx->syncMode == LOGSIG_ASYNCHRONOUS)
		add_queue_item(ksi->ctx, QITEM_CLOSE_FILE, 0, 0, 0);

	ksiWwriteStateFile(ksi);
	return 0;
}

int mkpath(char* path, mode_t mode, uid_t uid, gid_t gid) {

	for (char *p = strchr(path + 1, '/'); p; p = strchr(p + 1, '/')) {
		*p = '\0';
		if (mkdir(path, mode) == -1) {
			if (errno != EEXIST) {
				*p = '/';
				return -1;
			}
			if (chown(p, uid, gid)) {
			}
		}
		*p = '/';
	}
	return 0;
}

static FILE*
ksiCreateFile(rsksictx ctx, const char *path, uid_t uid, gid_t gid, int mode, bool lockit, const char* header) {
	int fd = -1;
	struct stat stat_st;
	FILE *f = NULL;
	struct flock lock = {F_WRLCK, SEEK_SET, 0, 0, 0};

	if (mkpath((char*) path, ctx->fDirCreateMode, ctx->dirUID, ctx->dirGID) != 0) {
		report(ctx, "ksiCreateFile: mkpath failed for %s", path);
		goto done;
	}

	fd = open(path, O_RDWR | O_APPEND | O_NOCTTY | O_CLOEXEC, 0600);
	if (fd == -1) {
		fd = open(path, O_RDWR | O_CREAT | O_NOCTTY | O_CLOEXEC, mode);
		if (fd == -1) {
			report(ctx, "creating file '%s' failed: %s", path, strerror(errno));
			goto done;
		}

		if (uid != (uid_t) - 1 || gid != (gid_t) - 1) {
			if (fchown(fd, uid, gid) != 0) {
				report(ctx, "lmsig_ksi: chown for file '%s' failed: %s",
					path, strerror(errno));
			}
		}
	}

	if (lockit && fcntl(fd, F_SETLK, &lock) != 0)
		report(ctx, "fcntl error: %s", strerror(errno));

	f = fdopen(fd, "a");
	if (f == NULL) {
		report(ctx, "fdopen for '%s' failed: %s", path, strerror(errno));
		goto done;
	}

	setvbuf(f, NULL, _IOFBF, KSI_BUF_SIZE);

	if (fstat(fd, &stat_st) == -1) {
		reportErr(ctx, "ksiOpenSigFile: can not stat file");
		goto done;
	}

	if (stat_st.st_size == 0 && header != NULL)
		fwrite(header, strlen(header), 1, f);

done:
	return f;
}

/* note: if file exists, the last hash for chaining must
 * be read from file.
 */
static int
ksiOpenSigFile(ksifile ksi) {
	int r = 0;
	const char *header;
	FILE* signatureFile = NULL;

	if (ksi->ctx->syncMode == LOGSIG_ASYNCHRONOUS)
		header = LS12_BLOCKFILE_HEADER;
	else
		header = LS12_FILE_HEADER;

	ksi->blockFile = ksiCreateFile(ksi->ctx, (char*) ksi->blockfilename, ksi->ctx->fileUID,
		ksi->ctx->fileGID, ksi->ctx->fCreateMode, true, header);

	if (ksi->blockFile == NULL) {
		r = RSGTE_IO;
		goto done;
	}

	/* create the file for ksi signatures if needed */
	if (ksi->ctx->syncMode == LOGSIG_ASYNCHRONOUS) {
		signatureFile = ksiCreateFile(ksi->ctx, (char*) ksi->ksifilename, ksi->ctx->fileUID,
			ksi->ctx->fileGID, ksi->ctx->fCreateMode, true, LS12_SIGFILE_HEADER);

		if (signatureFile == NULL) {
			r = RSGTE_IO;
			goto done;
		}

		add_queue_item(ksi->ctx, QITEM_NEW_FILE, signatureFile, time(NULL) + ksi->blockTimeLimit, 0);
	}

	/* we now need to obtain the last previous hash, so that
	 * we can continue the hash chain. We do not check for error
	 * as a state file error can be recovered by graceful degredation.
	 */
	ksiReadStateFile(ksi);
done:	return r;
}

/*
 * As of some Linux and security expert I spoke to, /dev/urandom
 * provides very strong random numbers, even if it runs out of
 * entropy. As far as he knew, this is save for all applications
 * (and he had good proof that I currently am not permitted to
 * reproduce). -- rgerhards, 2013-03-04
 */
static void
seedIVKSI(ksifile ksi)
{
	int hashlen;
	int fd;
	char *rnd_device = ksi->ctx->random_source ? ksi->ctx->random_source : "/dev/urandom";

	hashlen = KSI_getHashLength(ksi->hashAlg);
	ksi->IV = malloc(hashlen); /* do NOT zero-out! */
	/* if we cannot obtain data from /dev/urandom, we use whatever
	 * is present at the current memory location as random data. Of
	 * course, this is very weak and we should consider a different
	 * option, especially when not running under Linux (for Linux,
	 * unavailability of /dev/urandom is just a theoretic thing, it
	 * will always work...).  -- TODO -- rgerhards, 2013-03-06
	 */
	if ((fd = open(rnd_device, O_RDONLY)) > 0) {
		if(read(fd, ksi->IV, hashlen)) {}; /* keep compiler happy */
		close(fd);
	}
}

void create_signer_thread(rsksictx ctx) {
	if (!ctx->thread_started) {
		if (pthread_mutex_init(&ctx->module_lock, 0))
			report(ctx, "pthread_mutex_init: %s", strerror(errno));
		ctx->signer_queue = ProtectedQueue_new(10);
		if (pthread_create(&ctx->signer_thread, NULL, signer_thread, ctx))
			report(ctx, "pthread_mutex_init: %s", strerror(errno));
		ctx->thread_started = true;
	}
}

rsksictx
rsksiCtxNew(void) {
	rsksictx ctx;
	ctx = calloc(1, sizeof (struct rsksictx_s));
	KSI_CTX_new(&ctx->ksi_ctx); // TODO: error check (probably via a generic macro?)
	ctx->hasher = NULL;
	ctx->hashAlg = KSI_HASHALG_SHA2_256;
	ctx->blockTimeLimit = 0;
	ctx->bKeepTreeHashes = false;
	ctx->bKeepRecordHashes = true;
	ctx->errFunc = NULL;
	ctx->usrptr = NULL;
	ctx->fileUID = -1;
	ctx->fileGID = -1;
	ctx->dirUID = -1;
	ctx->dirGID = -1;
	ctx->fCreateMode = 0644;
	ctx->fDirCreateMode = 0700;
	ctx->syncMode = LOGSIG_SYNCHRONOUS;
	ctx->thread_started = false;
	ctx->disabled = false;

	/*if (pthread_mutex_init(&ctx->module_lock, 0))
		report(ctx, "pthread_mutex_init: %s", strerror(errno));
	ctx->signer_queue = ProtectedQueue_new(10);*/

	/* Creating a thread this way works only in daemon mode but not when being run
	 interactively when not forked */
	/*ret = pthread_atfork(NULL, NULL, create_signer_thread);
	if (ret != 0)
		report(ctx, "pthread_atfork error: %s", strerror(ret));*/

	return ctx;
}

/* either returns ksifile object or NULL if something went wrong */
ksifile
rsksiCtxOpenFile(rsksictx ctx, unsigned char *logfn)
{
	ksifile ksi;
	char fn[MAXFNAME+1];

	if (ctx->disabled)
		return NULL;

	pthread_mutex_lock(&ctx->module_lock);

	/* The thread cannot be be created in rsksiCtxNew because in daemon mode the
	 process forks after rsksiCtxNew and the thread disappears */
	if (!ctx->thread_started)
		create_signer_thread(ctx);

	if ((ksi = rsksifileConstruct(ctx)) == NULL)
		goto done;

	snprintf(fn, sizeof (fn), "%s.ksistate", logfn);
	fn[MAXFNAME] = '\0'; /* be on safe side */
	ksi->statefilename = (uchar*) strdup(fn);

	if (ctx->syncMode == LOGSIG_ASYNCHRONOUS) {
		/* filename for blocks of hashes*/
		snprintf(fn, sizeof (fn), "%s%s", logfn, blockFileSuffix);
		fn[MAXFNAME] = '\0'; /* be on safe side */
		ksi->blockfilename = (uchar*) strdup(fn);

		/* filename for KSI signatures*/
		snprintf(fn, sizeof (fn), "%s%s", logfn, sigFileSuffix);
		fn[MAXFNAME] = '\0'; /* be on safe side */
		ksi->ksifilename = (uchar*) strdup(fn);
	} else if (ctx->syncMode == LOGSIG_SYNCHRONOUS) {
		snprintf(fn, sizeof (fn), "%s%s", logfn, ls12FileSuffix);
		fn[MAXFNAME] = '\0'; /* be on safe side */
		ksi->blockfilename = (uchar*) strdup(fn);
	}

	if (ksiOpenSigFile(ksi) != 0) {
		reportErr(ctx, "signature file open failed");
		/* Free memory */
		free(ksi);
		ksi = NULL;
	}

done:
	ctx->ksi = ksi;
	pthread_mutex_unlock(&ctx->module_lock);
	return ksi;
}
 

/* returns 0 on succes, 1 if algo is unknown, 2 is algo has been remove
 * because it is now considered insecure
 */
int
rsksiSetHashFunction(rsksictx ctx, char *algName) {
	int r, id = KSI_getHashAlgorithmByName(algName);
	if (id == KSI_HASHALG_INVALID) {
		report(ctx, "Hash function '%s' unknown - using default", algName);
		ctx->hashAlg = KSI_HASHALG_SHA2_256;
	} else {
		ctx->hashAlg = id;
	}

	if ((r = KSI_DataHasher_open(ctx->ksi_ctx, ctx->hashAlg, &ctx->hasher)) != KSI_OK) {
		reportKSIAPIErr(ctx, NULL, "KSI_DataHasher_open", r);
		ctx->disabled = true;
	}

	return 0;
}

int
rsksifileDestruct(ksifile ksi) {
	int r = 0;
	rsksictx ctx = NULL;
	if (ksi == NULL)
		return RSGTE_INTERNAL;

	pthread_mutex_lock(&ksi->ctx->module_lock);

	ctx = ksi->ctx;

	if (!ksi->disabled && ksi->bInBlk) {
		sigblkAddMetadata(ksi, blockCloseReason, "Block closed due to file closure.");
		r = sigblkFinishKSI(ksi);
	}
	if(!ksi->disabled)
		r = ksiCloseSigFile(ksi);
	free(ksi->blockfilename);
	free(ksi->statefilename);
	free(ksi->ksifilename);
	free(ksi->IV);
	ctx->ksi = NULL;
	free(ksi);

	pthread_mutex_unlock(&ctx->module_lock);
	return r;
}

void
rsksiCtxDel(rsksictx ctx) {
	if (ctx == NULL)
		return;

	if (ctx->thread_started) {
		add_queue_item(ctx, QITEM_QUIT, NULL, 0, 0);
		pthread_join(ctx->signer_thread, NULL);
		ProtectedQueue_free(ctx->signer_queue);
		pthread_mutex_destroy(&ctx->module_lock);
	}

	free(ctx->aggregatorUri);
	free(ctx->aggregatorId);
	free(ctx->aggregatorKey);

	if (ctx->random_source)
		free(ctx->random_source);

	KSI_DataHasher_free(ctx->hasher);
	KSI_CTX_free(ctx->ksi_ctx);
	free(ctx);
}

/* new sigblk is initialized, but maybe in existing ctx */
void
sigblkInitKSI(ksifile ksi)
{
	if(ksi == NULL) goto done;
	seedIVKSI(ksi);
	memset(ksi->roots, 0, sizeof (ksi->roots));
	ksi->nRoots = 0;
	ksi->nRecords = 0;
	ksi->bInBlk = 1;
	ksi->blockStarted = time(NULL); //TODO: maybe milli/nanoseconds should be used

done:
	return;
}

int
sigblkCreateMask(ksifile ksi, KSI_DataHash **m) {
	int r = 0;

	CHKr(KSI_DataHasher_reset(ksi->ctx->hasher));
	CHKr(KSI_DataHasher_add(ksi->ctx->hasher, ksi->lastLeaf, KSI_getHashLength(ksi->lastLeaf[0]) + 1));
	CHKr(KSI_DataHasher_add(ksi->ctx->hasher, ksi->IV, KSI_getHashLength(ksi->hashAlg)));
	CHKr(KSI_DataHasher_close(ksi->ctx->hasher, m));

done:
	if (r != KSI_OK) {
		reportKSIAPIErr(ksi->ctx, ksi, "KSI_DataHasher", r);
		r = RSGTE_HASH_CREATE;
	}
	return r;
}
int
sigblkCreateHash(ksifile ksi, KSI_DataHash **out, const uchar *rec, const size_t len) {
	int r = 0;

	CHKr(KSI_DataHasher_reset(ksi->ctx->hasher));
	CHKr(KSI_DataHasher_add(ksi->ctx->hasher, rec, len));
	CHKr(KSI_DataHasher_close(ksi->ctx->hasher, out));

done:
	if (r != KSI_OK) {
		reportKSIAPIErr(ksi->ctx, ksi, "KSI_DataHasher", r);
		r = RSGTE_HASH_CREATE;
	}

	return r;
}


int
sigblkHashTwoNodes(ksifile ksi, KSI_DataHash **out, KSI_DataHash *left, KSI_DataHash *right,
          uint8_t level) {
	int r = 0;

	CHKr(KSI_DataHasher_reset(ksi->ctx->hasher));
	CHKr(KSI_DataHasher_addImprint(ksi->ctx->hasher, left));
	CHKr(KSI_DataHasher_addImprint(ksi->ctx->hasher, right));
	CHKr(KSI_DataHasher_add(ksi->ctx->hasher, &level, 1));
	CHKr(KSI_DataHasher_close(ksi->ctx->hasher, out));

done:
	if (r != KSI_OK) {
		reportKSIAPIErr(ksi->ctx, ksi, "KSI_DataHash_create", r);
		r = RSGTE_HASH_CREATE;
	}

	return r;
}
int
sigblkAddMetadata(ksifile ksi, const char *key, const char *value) {
	unsigned char buffer[0xFFFF];
	size_t encoded_size = 0;
	int ret = 0;

	tlvCreateMetadata(ksi, ksi->nRecords, key, value, buffer, &encoded_size);
	sigblkAddLeaf(ksi, buffer, encoded_size, true);

	return ret;
}

int
sigblkAddRecordKSI(ksifile ksi, const uchar *rec, const size_t len) {
	int ret = 0;
	if (ksi == NULL || ksi->disabled)
		return 0;

	pthread_mutex_lock(&ksi->ctx->module_lock);

	if ((ret = sigblkAddLeaf(ksi, rec, len, false)) != 0)
		goto done;

	if (ksi->nRecords == ksi->blockSizeLimit) {
		sigblkFinishKSI(ksi);
		sigblkInitKSI(ksi);
	}

done:
	pthread_mutex_unlock(&ksi->ctx->module_lock);
	return ret;
}


int
sigblkAddLeaf(ksifile ksi, const uchar *leafData, const size_t leafLength, bool metadata) {

	KSI_DataHash *mask, *leafHash, *treeNode, *tmpTreeNode;
	uint8_t j;
	const unsigned char *pTmp;
	size_t len;

	int r = 0;

	if (ksi == NULL || ksi->disabled) goto done;
	CHKr(sigblkCreateMask(ksi, &mask));
	CHKr(sigblkCreateHash(ksi, &leafHash, leafData, leafLength));

	if(ksi->nRecords == 0) 
		tlvWriteBlockHdrKSI(ksi);

	/* metadata record has to be written into the block file too*/
	if (metadata)
		tlvWriteOctetString(ksi->blockFile, leafData, leafLength);

	if (ksi->bKeepRecordHashes)
		tlvWriteHashKSI(ksi, 0x0902, leafHash);

	/* normal leaf and metadata record are hashed in different order */
	if (!metadata) { /* hash leaf */
		if ((r = sigblkHashTwoNodes(ksi, &treeNode, mask, leafHash, 1)) != 0) goto done;
	} else {
		if ((r = sigblkHashTwoNodes(ksi, &treeNode, leafHash, mask, 1)) != 0) goto done;
	}

	/* persists x here if Merkle tree needs to be persisted! */
	if(ksi->bKeepTreeHashes)
		tlvWriteHashKSI(ksi, 0x0903, treeNode);

	KSI_DataHash_getImprint(treeNode, &pTmp, &len);
	memcpy(ksi->lastLeaf, pTmp, len);

	for(j = 0 ; j < ksi->nRoots ; ++j) {
		if (ksi->roots[j] == NULL) {
			ksi->roots[j] = treeNode;
			treeNode = NULL;
			break;
		} else if (treeNode != NULL) {
			/* hash interim node */
			tmpTreeNode = treeNode;
			r = sigblkHashTwoNodes(ksi, &treeNode, ksi->roots[j], tmpTreeNode, j + 2);
			KSI_DataHash_free(ksi->roots[j]);
			ksi->roots[j] = NULL;
			KSI_DataHash_free(tmpTreeNode);
			if (r != 0) goto done;
			if(ksi->bKeepTreeHashes)
				tlvWriteHashKSI(ksi, 0x0903, treeNode);
		}
	}
	if (treeNode != NULL) {
		/* new level, append "at the top" */
		ksi->roots[ksi->nRoots] = treeNode;
		++ksi->nRoots;
		assert(ksi->nRoots < MAX_ROOTS);
		treeNode = NULL;
	}
	++ksi->nRecords;

	/* cleanup (x is cleared as part of the roots array) */
	KSI_DataHash_free(mask);
	KSI_DataHash_free(leafHash);

done:
	return r;
}

int
sigblkCheckTimeOut(rsksictx ctx) {
	int ret = 0;
	time_t now;
	char buf[KSI_BUF_SIZE];

	pthread_mutex_lock(&ctx->module_lock);

	if (!ctx->ksi || ctx->disabled || !ctx->blockTimeLimit || !ctx->ksi->bInBlk)
		goto done;

	now = time(NULL);

	if (ctx->ksi->blockStarted + ctx->blockTimeLimit > now)
		goto done;

	snprintf(buf, KSI_BUF_SIZE, "Block closed due to reaching time limit %d", ctx->blockTimeLimit);

	sigblkAddMetadata(ctx->ksi, blockCloseReason, buf);
	sigblkFinishKSI(ctx->ksi);
	sigblkInitKSI(ctx->ksi);

done:
	pthread_mutex_unlock(&ctx->module_lock);
	return ret;
}

static int
sigblkSign(ksifile ksi, KSI_DataHash *hash, int level)
{
	unsigned char *der = NULL;
	size_t lenDer = 0;
	int r = KSI_OK;
	int ret = 0;
	KSI_Signature *sig = NULL;

	/* Sign the root hash. */
	r = KSI_Signature_signAggregated(ksi->ctx->ksi_ctx, hash, level, &sig);
	if (r != KSI_OK) {
		reportKSIAPIErr(ksi->ctx, ksi, "KSI_Signature_createAggregated", r);
		ret = 1;
		goto signing_done;
	}

	/* Serialize Signature. */
	r = KSI_Signature_serialize(sig, &der, &lenDer);
	if (r != KSI_OK) {
		reportKSIAPIErr(ksi->ctx, ksi, "KSI_Signature_serialize", r);
		ret = 1;
		goto signing_done;
		lenDer = 0;
	}

signing_done:
	/* if signing failed the signature will be written as zero size */
	if (r == KSI_OK) {
		r = tlvWriteKSISigLS12(ksi->blockFile, ksi->nRecords, der, lenDer);
		if (r != KSI_OK) {
			reportKSIAPIErr(ksi->ctx, ksi, "tlvWriteKSISigLS12", r);
			ret = 1;
		}
	} else
		r = tlvWriteNoSigLS12(ksi->blockFile, ksi->nRecords, hash, KSI_getErrorString(r));

	if (r != KSI_OK) {
		reportKSIAPIErr(ksi->ctx, ksi, "tlvWriteBlockSigKSI", r);
		ret = 1;
	}

	if (sig != NULL)
		KSI_Signature_free(sig);
	if (der != NULL)
		KSI_free(der);
	return ret;
}

int
sigblkFinishKSI(ksifile ksi)
{
	KSI_DataHash *root, *rootDel;
	int8_t j;
	int ret = 0;
	int level = 0;

	if (ksi->nRecords == 0)
		goto done;

	root = NULL;
	for(j = 0 ; j < ksi->nRoots ; ++j) {
		if(root == NULL) {
			root = ksi->roots[j];
			ksi->roots[j] = NULL;
		} else if (ksi->roots[j] != NULL) {
			rootDel = root;
			ret = sigblkHashTwoNodes(ksi, &root, ksi->roots[j], rootDel, j + 2);
			KSI_DataHash_free(ksi->roots[j]);
			ksi->roots[j] = NULL;
			KSI_DataHash_free(rootDel);
			if(ret != 0) goto done; /* checks hash_node_ksi() result! */
		}
	}

	level = j;

	//in case of async mode we append the root hash to signer queue
	if (ksi->ctx->syncMode == LOGSIG_ASYNCHRONOUS) {
		ret = tlvWriteNoSigLS12(ksi->blockFile, ksi->nRecords, root, NULL);
		if (ret != KSI_OK) {
			reportKSIAPIErr(ksi->ctx, ksi, "tlvWriteNoSigLS12", ret);
			ret = 1;
		}

		add_queue_item(ksi->ctx, QITEM_SIGNATURE_REQUEST, root, ksi->nRecords, level);
	} else {
		sigblkSign(ksi, root, level);
		KSI_DataHash_free(root); //otherwise delete it
	}

done:
	ksi->bInBlk = 0;
	return ret;
}

int
rsksiSetAggregator(rsksictx ctx, char *uri, char *loginid, char *key) {
	int r;

	if (!uri || !loginid || !key) {
		ctx->disabled = true;
		return KSI_INVALID_ARGUMENT;
	}


	r = KSI_CTX_setAggregator(ctx->ksi_ctx, uri, loginid, key);
	if(r != KSI_OK) {
		ctx->disabled = true;
		reportKSIAPIErr(ctx, NULL, "KSI_CTX_setAggregator", r);
		return KSI_INVALID_ARGUMENT;
	}

	ctx->aggregatorUri = strdup(uri);
	ctx->aggregatorId = strdup(loginid);
	ctx->aggregatorKey = strdup(key);

	return r;
}

bool add_queue_item(rsksictx ctx, QITEM_type type, void *arg, uint64_t intarg1, uint64_t intarg2) {
	QueueItem *qi = (QueueItem*) malloc(sizeof (QueueItem));
	if (!qi) {
		ctx->disabled = true;
		return false;
	}

	qi->arg = arg;
	qi->type = type;
	qi->intarg1 = intarg1;
	qi->intarg2 = intarg2;
	if (ProtectedQueue_addItem(ctx->signer_queue, qi) == false) {
		ctx->disabled = true;
		free(qi);
		return false;
	}
	return true;
}

//This version of signing thread discards all the requests except last one (no aggregation/pipelining used)

void process_requests(rsksictx ctx, KSI_CTX *ksi_ctx, FILE* outfile) {
	QueueItem *item = NULL;
	QueueItem *lastItem = NULL;
	unsigned char *der = NULL;
	size_t lenDer = 0;
	int r = KSI_OK;
	int ret = 0;
	KSI_Signature *sig = NULL;

	while (ProtectedQueue_peekFront(ctx->signer_queue, (void**) &item) && item->type == QITEM_SIGNATURE_REQUEST) {
		if (lastItem != NULL) {
			if (outfile)
				tlvWriteNoSigLS12(outfile, lastItem->intarg1, lastItem->arg, 
						"Signature request dropped for block, signer queue full");
			report(ctx, "Signature request dropped for block, signer queue full");
			KSI_DataHash_free(lastItem->arg);
			free(lastItem);
		}

		ProtectedQueue_popFront(ctx->signer_queue, (void**) &item);
		lastItem = item;
	}

	if (!outfile)
		goto signing_failed;

	r = KSI_Signature_signAggregated(ksi_ctx, lastItem->arg, lastItem->intarg2, &sig);
	if (r != KSI_OK) {
		reportKSIAPIErr(ctx, NULL, "KSI_Signature_createAggregated", r);
		ret = 1;
		goto signing_failed;
	}

	/* Serialize Signature. */
	r = KSI_Signature_serialize(sig, &der, &lenDer);
	if (r != KSI_OK) {
		reportKSIAPIErr(ctx, NULL, "KSI_Signature_serialize", r);
		ret = 1;
		goto signing_failed;
		lenDer = 0;
	}

signing_failed:

	if (outfile) {
		if (r == KSI_OK)
			r = tlvWriteKSISigLS12(outfile, lastItem->intarg1, der, lenDer);
		else
			r = tlvWriteNoSigLS12(outfile, lastItem->intarg1, lastItem->arg, KSI_getErrorString(r));
	}

	if (r != KSI_OK)
		reportKSIAPIErr(ctx, NULL, "tlvWriteKSISigLS12", r);

	if (lastItem != NULL) {
		KSI_DataHash_free(lastItem->arg);
		free(lastItem);
	}

	if (sig != NULL)
		KSI_Signature_free(sig);
	if (der != NULL)
		KSI_free(der);
}

void *signer_thread(void *arg) {

	rsksictx ctx = (rsksictx) arg;
	QueueItem *item = NULL;
	FILE* ksiFile = NULL;
	time_t timeout;
	KSI_CTX *ksi_ctx;

	ctx->thread_started = true;

	KSI_CTX_new(&ksi_ctx);
	KSI_CTX_setAggregator(ksi_ctx, ctx->aggregatorUri, ctx->aggregatorId, ctx->aggregatorKey);

	while (true) {
		timeout = 1;

		/* Wait for a work item or timeout*/
		ProtectedQueue_waitForItem(ctx->signer_queue, NULL, timeout * 1000);

		/* Check for block time limit*/
		sigblkCheckTimeOut(ctx);

		/* in case there are no items go around*/
		if (ProtectedQueue_count(ctx->signer_queue) == 0)
			continue;

		/* drain all consecutive signature requests from the queue and add
		 * them to aggregation request */
		while (ProtectedQueue_peekFront(ctx->signer_queue, (void**) &item)
			&& item->type == QITEM_SIGNATURE_REQUEST) {
			process_requests(ctx, ksi_ctx, ksiFile);
			continue;
		}

		/* Handle other types of work items */
		if (ProtectedQueue_popFront(ctx->signer_queue, (void**) &item) != 0) {
			if (item->type == QITEM_CLOSE_FILE) {
				fclose(ksiFile);
				ksiFile = NULL;
			} else if (item->type == QITEM_NEW_FILE) {
				ksiFile = (FILE*) item->arg;
			} else if (item->type == QITEM_QUIT) {
				if (ksiFile)
					fclose(ksiFile);
				free(item);
				break;
			}
			free(item);
		}
	}

	KSI_CTX_free(ksi_ctx);
	ctx->thread_started = false;
	return NULL;
}
