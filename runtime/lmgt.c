/* lmgt.c
 *
 * Regarding the online algorithm for Merkle tree signing. Expected 
 * calling sequence is:
 *
 * sigblkConstruct
 * for each signature block:
 *    sigblkInit
 *    for each record:
 *       sigblkAddRecord
 *    sigblkFinish
 * sigblkDestruct
 *
 * Obviously, the next call after sigblkFinsh must either be to 
 * sigblkInit or sigblkDestruct (if no more signature blocks are
 * to be emitted, e.g. on file close). sigblkDestruct saves state
 * information (most importantly last block hash) and sigblkConstruct
 * reads (or initilizes if not present) it.
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
#include "config.h"
#include <stdint.h>


/* Max number of roots inside the forest. This permits blocks of up to
 * 2^MAX_ROOTS records. We assume that 64 is sufficient for all use
 * cases ;) [and 64 is not really a waste of memory, so we do not even
 * try to work with reallocs and such...
 */
#define MAX_ROOTS 64

/* context for gt calls. All state data is kept here, this permits
 * multiple concurrent callers.
 */
struct gtctx_s {
	IV; /* initial value for blinding masks (where to do we get it from?) */
	x_prev = NULL; /* last leaf hash (maybe of previous block) --> preserve on term */
	int8_t nroots;
	/* algo engineering: roots structure is split into two arrays
	 * in order to improve cache hits.
	 */
	char roots_valid[MAX_ROOTS];
	hash_mem roots_hash[MAX_ROOTS];

};
typedef struct gtctx_s *gtctx;

void
sigblkInit(gtctx ctx)
{
	init ctx->IV;
	if(ctx->x_prev == NULL)
		alloc & zero-fill x_prev;
	memset(ctx->roots_valid, 0, sizeof(ctx->roots_valid)/sizeof(char));
	nroots = 0;
}


void
sigblkAddRecord(gtctx ctx, char *rec)
{
	auto x; /* current hash */
	hash_mem m, r, t;
	int8_t j;

	m = hash(concat(ctx->x_prev, IV));
	r = hash(canonicalize(rec));
	x = hash(concat(m, r, 0)); /* hash leaf */
	/* persists x here if Merkle tree needs to be persisted! */
	/* add x to the forest as new leaf, update roots list */
	t = x;
	for(j = 0 ; j < ctx->nRoots ; ++j) {
		if(ctx->roots_valid[j] == 0) {
			ctx->roots_hash[j] = t;
			ctx->roots_valid[j] = 1;
			t = NULL;
		} else if(t != NULL) {
			t = hash(concat(ctx->roots_hash[j], t, j+1)); /* hash interim node */
			ctx->roots_valid[j] = 0;
	}
	if(t != NULL) {
		ctx->roots_hash[ctx->nroots] = t;
		++ctx->roots_hash;
		assert(ctx->roots_hash < MAX_ROOTS);
		t = NULL;
	}
	ctx->x_prev = x; /* single var may be sufficient */
}

void
sigblkFinish(gtctx ctx)
{
	hash_mem root;
	int8_t j;

	root = NULL;
	for(j = 0 ; j < ctx->nRoots ; ++j) {
		if(root == NULL) {
			root = ctx->roots_hash[j];
			ctx->roots_valid[j] = 0; /* guess this is redundant with init, maybe del */
		} else if(ctx->roots_valid[j]) {
			root = hash(concat(ctx->roots_hash[j], root, j+1));
			ctx->roots_valid[j] = 0; /* guess this is redundant with init, maybe del */
		}
	}
	/* persist root value here (callback?) */
}
