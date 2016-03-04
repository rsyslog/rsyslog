/* librsgt_common.h - rsyslog's guardtime common defines
 *
 * Copyright 2015 Adiscon GmbH.
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
#ifndef INCLUDED_LIBRSGTCM_H
#define INCLUDED_LIBRSGTCM_H

/* Max number of roots inside the forest. This permits blocks of up to
 * 2^MAX_ROOTS records. We assume that 64 is sufficient for all use
 * cases ;) [and 64 is not really a waste of memory, so we do not even
 * try to work with reallocs and such...]
 */
#define MAX_ROOTS 64
#define LOGSIGHDR "LOGSIG11"

/* Context for gt calls. This primarily serves as a container for the
 * config settings. The actual file-specific data is kept in gtfile.
 */
typedef struct imprint_s imprint_t;
typedef struct block_hdr_s block_hdr_t;
typedef struct block_sig_s block_sig_t;
typedef struct tlvrecord_s tlvrecord_t;
typedef struct block_hashchain_s block_hashchain_t;
typedef struct block_hashstep_s block_hashstep_t;

struct tlvrecord_s {
	uint16_t tlvtype;
	uint16_t tlvlen;
	uint8_t hdr[4]; /* the raw header (as persisted to file) */
	uint8_t lenHdr; /* length of raw header */
	uint8_t data[64*1024];	/* the actual data part (of length tlvlen) */
};

struct imprint_s {
	uint8_t hashID;
	size_t len;
	uint8_t *data;
};

#define SIGID_RFC3161 0
struct block_hdr_s {
 	uint8_t hashID;
 	uint8_t *iv;
 	imprint_t lastHash;
};

struct block_sig_s {
	uint8_t sigID; /* what type of *signature*? */
	uint64_t recCount;
	struct {
		struct {
			uint8_t *data;
			size_t len; /* must be size_t due to GT API! */
		} der;
	} sig;
};

struct block_hashstep_s {
	uint8_t direction;	/* left-link or right-link */
	uint8_t level_corr;
	imprint_t sib_hash;
};

struct block_hashchain_s {
 	imprint_t rec_hash;
	uint64_t stepCount; /* Helper to count left & right links */
	block_hashstep_t *hashsteps[MAX_ROOTS]; /* Using MAX_ROOTS here as well */
	uint8_t direction;	/* left-link or right-link */
	uint8_t level;		/* default 0 */
};


static inline char *
sigTypeName(uint8_t sigID)
{
	switch(sigID) {
	case SIGID_RFC3161:
		return "RFC3161";
	default:return "[unknown]";
	}
}

/* Flags and record types for TLV handling */
#define RSGT_FLAG_NONCRIT 0x20
#define RSGT_FLAG_FORWARD 0x40
#define RSGT_TYPE_MASK 0x1f
#define RSGT_FLAG_TLV16 0x80

/* check return state of operation and abort, if non-OK */
#define CHKr(code) if((r = code) != 0) goto done
/* check return state of operation and jump to donedecode, if non-OK */
#define CHKrDecode(code) if((r = code) != 0) goto donedecode

#endif  /* #ifndef INCLUDED_LIBRSGTCM_H */