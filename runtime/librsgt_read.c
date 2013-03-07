/* librsgt_read.c - rsyslog's guardtime support library
 * This includes functions used for reading signature (and 
 * other related) files.
 *
 * This part of the library uses C stdio and expects that the
 * caller will open and close the file to be read itself.
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
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

#include "librsgt.h"

typedef unsigned char uchar;
#ifndef VERSION
#define VERSION "no-version"
#endif
#define MAXFNAME 1024

static int rsgt_read_debug = 0;

/* macro to obtain next char from file including error tracking */
#define NEXTC	if((c = fgetc(fp)) == EOF) { \
			r = RSGTE_IO; \
			goto done; \
		}

/* check return state of operation and abort, if non-OK */
#define CHKr(code) if((r = code) != 0) goto done

/**
 * Read a header from a binary file.
 * @param[in] fp file pointer for processing
 * @param[in] hdr buffer for the header. Must be 9 bytes 
 * 		  (8 for header + NUL byte)
 * @returns 0 if ok, something else otherwise
 */
int
rsgt_tlvrdHeader(FILE *fp, uchar *hdr)
{
	int r;
	if(fread(hdr, 8, 1, fp) != 1) {
		r = RSGTE_IO;
		goto done;
	}
	hdr[8] = '\0';
	r = 0;
done:	return r;
}

/* read type & length */
static int
rsgt_tlvrdTL(FILE *fp, uint16_t *tlvtype, uint16_t *tlvlen)
{
	int r = 1;
	int c;

	NEXTC;
	*tlvtype = c & 0x1f;
	if(c & 0x20) { /* tlv16? */
		NEXTC;
		*tlvtype = (*tlvtype << 8) | c;
		NEXTC;
		*tlvlen = c << 8;
		NEXTC;
		*tlvlen |= c;
	} else {
		NEXTC;
		*tlvlen = c;
	}
	if(rsgt_read_debug)
		printf("read tlvtype %4.4x, len %u\n", (unsigned) *tlvtype,
			(unsigned) *tlvlen);
	r = 0;
done:	return r;
}

static int
rsgt_tlvrdOctetString(FILE *fp, uint8_t **data, size_t len)
{
	size_t i;
	int c, r = 1;
	if((*data = (uint8_t*)malloc(len)) == NULL) {r=RSGTE_OOM;goto done;}
	for(i = 0 ; i < len ; ++i) {
		NEXTC;
		(*data)[i] = c;
	}
	r = 0;
done:	return r;
}
static int
rsgt_tlvrdHASH_ALGO(FILE *fp, uint8_t *hashAlg)
{
	int r = 1;
	int c;
	uint16_t tlvtype, tlvlen;

	CHKr(rsgt_tlvrdTL(fp, &tlvtype, &tlvlen));
	if(!(tlvtype == 0x00 && tlvlen == 1)) {
		r = RSGTE_FMT;
		goto done;
	}
	NEXTC;
	*hashAlg = c;
	r = 0;
done:	return r;
}
static int
rsgt_tlvrdBLOCK_IV(FILE *fp, uint8_t **iv)
{
	int r = 1;
	uint16_t tlvtype, tlvlen;

	CHKr(rsgt_tlvrdTL(fp, &tlvtype, &tlvlen));
	if(!(tlvtype == 0x01)) {
		r = RSGTE_INVLTYP;
		goto done;
	}
	CHKr(rsgt_tlvrdOctetString(fp, iv, tlvlen));
	r = 0;
done:	return r;
}
static int
rsgt_tlvrdLAST_HASH(FILE *fp, imprint_t *imp)
{
	int r = 1;
	int c;
	uint16_t tlvtype, tlvlen;

	CHKr(rsgt_tlvrdTL(fp, &tlvtype, &tlvlen));
	if(!(tlvtype == 0x02)) { r = RSGTE_INVLTYP; goto done; }
	NEXTC;
	imp->hashID = c;
	imp->len = tlvlen - 1;
	CHKr(rsgt_tlvrdOctetString(fp, &imp->data, tlvlen-1));
	r = 0;
done:	return r;
}
static int
rsgt_tlvrdREC_COUNT(FILE *fp, uint64_t *cnt, size_t *lenInt)
{
	int r = 1;
	int i;
	int c;
	uint64_t val;
	uint16_t tlvtype, tlvlen;

	if((r = rsgt_tlvrdTL(fp, &tlvtype, &tlvlen)) != 0) goto done;
	if(!(tlvtype == 0x03 && tlvlen <= 8)) { r = RSGTE_INVLTYP; goto done; }
	*lenInt = tlvlen;
	val = 0;
	for(i = 0 ; i < tlvlen ; ++i) {
		NEXTC;
		val = (val << 8) + c;
	}
	*cnt = val;
	r = 0;
done:	return r;
}
static int
rsgt_tlvrdSIG(FILE *fp, block_sig_t *bs)
{
	int r = 1;
	uint16_t tlvtype, tlvlen;

	CHKr(rsgt_tlvrdTL(fp, &tlvtype, &tlvlen));
	if(!(tlvtype == 0x0906)) { r = RSGTE_INVLTYP; goto done; }
	bs->sig.der.len = tlvlen;
	bs->sigID = SIGID_RFC3161;
	CHKr(rsgt_tlvrdOctetString(fp, &(bs->sig.der.data), tlvlen));
	r = 0;
done:	return r;
}

static int
rsgt_tlvrdBLOCK_SIG(FILE *fp, block_sig_t **blocksig, uint16_t tlvlen)
{
	int r = 1;
	size_t lenInt = 0;
	uint16_t sizeRead;
	block_sig_t *bs;
	if((bs = calloc(1, sizeof(block_sig_t))) == NULL) {
		r = RSGTE_OOM;
		goto done;
	}
	CHKr(rsgt_tlvrdHASH_ALGO(fp, &(bs->hashID)));
	CHKr(rsgt_tlvrdBLOCK_IV(fp, &(bs->iv)));
	CHKr(rsgt_tlvrdLAST_HASH(fp, &(bs->lastHash)));
	CHKr(rsgt_tlvrdREC_COUNT(fp, &(bs->recCount), &lenInt));
	CHKr(rsgt_tlvrdSIG(fp, bs));
	sizeRead = 2 + 1 /* hash algo TLV */ +
	 	   2 + getIVLen(bs) /* iv */ +
		   2 + 1 + bs->lastHash.len /* last hash */ +
		   2 + lenInt /* rec-count */ +
		   4 + bs->sig.der.len /* rfc-3161 */;
	if(sizeRead != tlvlen) {
		printf("lenght record error!\n");
		r = RSGTE_LEN;
		goto done;
	}
	*blocksig = bs;
	r = 0;
done:	return r;
}


/**
 * Read the next "object" from file. This usually is
 * a single TLV, but may be something larger, for
 * example in case of a block-sig TLV record.
 * Unknown type records are ignored (or run aborted
 * if we are not permitted to skip).
 *
 * @param[in] fp file pointer for processing
 * @param[out] tlvtype type of tlv record (top-level for
 * 		    structured objects.
 * @param[out] tlvlen length of the tlv record value
 * @param[out] obj pointer to object; This is a proper
 * 		   tlv record structure, which must be casted
 * 		   by the caller according to the reported type.
 * 		   The object must be freed by the caller (TODO: better way?)
 *
 * @returns 0 if ok, something else otherwise
 */
int
rsgt_tlvrd(FILE *fp, uint16_t *tlvtype, uint16_t *tlvlen, void *obj)
{
	int r = 1;

	if((r = rsgt_tlvrdTL(fp, tlvtype, tlvlen)) != 0) goto done;
	switch(*tlvtype) {
		case 0x0902:
			r = rsgt_tlvrdBLOCK_SIG(fp, obj, *tlvlen);
			if(r != 0) goto done;
			break;
	}
	r = 0;
done:	return r;
}


/* if verbose==0, only the first and last two octets are shown,
 * otherwise everything.
 */
static void
outputHexBlob(uint8_t *blob, uint16_t len, uint8_t verbose)
{
	unsigned i;
	if(verbose || len <= 6) {
		for(i = 0 ; i < len ; ++i)
			printf("%2.2x", blob[i]);
	} else {
		printf("%2.2x%2.2x[...]%2.2x%2.2x",
			blob[0], blob[1],
			blob[len-2], blob[len-2]);
	}
}

/* return if a blob is all zero */
static inline int
blobIsZero(uint8_t *blob, uint16_t len)
{
	int i;
	for(i = 0 ; i < len ; ++i)
		if(blob[i] != 0)
			return 0;
	return 1;
}
/**
 * Output a human-readable representation of a block_sig_t
 * to proviced file pointer. This function is mainly inteded for
 * debugging purposes or dumping tlv files.
 *
 * @param[in] fp file pointer to send output to
 * @param[in] bsig ponter to block_sig_t to output
 * @param[in] verbose if 0, abbreviate blob hexdump, else complete
 */
void
rsgt_printBLOCK_SIG(FILE *fp, block_sig_t *bs, uint8_t verbose)
{
	fprintf(fp, "Block Signature Record [0x0902]:\n");
	fprintf(fp, "\tPrevious Block Hash:\n");
	fprintf(fp, "\t   Algorithm..: %s\n", hashAlgName(bs->lastHash.hashID));
	fprintf(fp, "\t   Hash.......: ");
		outputHexBlob(bs->lastHash.data, bs->lastHash.len, verbose);
		fputc('\n', fp);
	if(blobIsZero(bs->lastHash.data, bs->lastHash.len))
		fprintf(fp, "\t   NOTE: New Hash Chain Start!\n");
	fprintf(fp, "\tHash Algorithm: %s\n", hashAlgName(bs->hashID));
	fprintf(fp, "\tIV............: ");
		outputHexBlob(bs->iv, getIVLen(bs), verbose);
		fputc('\n', fp);
	fprintf(fp, "\tRecord Count..: %llu\n", bs->recCount);
	fprintf(fp, "\tSignature Type: %s\n", sigTypeName(bs->sigID));
	fprintf(fp, "\tSignature Len.: %u\n", bs->sig.der.len);
	fprintf(fp, "\tSignature.....: ");
		outputHexBlob(bs->sig.der.data, bs->sig.der.len, verbose);
		fputc('\n', fp);
}


/**
 * Output a human-readable representation of a tlv object.
 *
 * @param[in] fp file pointer to send output to
 * @param[in] tlvtype type of tlv object (record)
 * @param[in] verbose if 0, abbreviate blob hexdump, else complete
 */
void
rsgt_tlvprint(FILE *fp, uint16_t tlvtype, void *obj, uint8_t verbose)
{
	switch(tlvtype) {
	case 0x0902:
		rsgt_printBLOCK_SIG(fp, obj, verbose);
		break;
	default:fprintf(fp, "unknown tlv record %4.4x\n", tlvtype);
		break;
	}
}
