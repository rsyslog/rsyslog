/* SPDX-License-Identifier: Apache-2.0 */
#ifndef INCLUDED_RULESET_NAME_INDEX_H
#define INCLUDED_RULESET_NAME_INDEX_H
#include "rsyslog.h"
#include "rsconf.h"

typedef struct rulesetNameBlockV1_s rulesetNameBlockV1_t;
/* Startup-only baseline, before any worker/input activation. Unsupported
 * platforms retain the legacy immutable startup list and reject additions. */
int rulesetNameIndexSupportedV1(void);
rsRetVal rulesetNameIndexInitV1(rsconf_t *conf);
/* Lookup sets indexed when a baseline exists; a miss must NEVER fall back to
 * the owning list in that case. Blocks/names live until stopped-config teardown. */
rsRetVal rulesetNameIndexLookupV1(rsconf_t *conf, ruleset_t **out, const char *name, int *indexed);
/* Prepare copies names and borrows stable shells, without publishing them. */
rsRetVal rulesetNameIndexPrepareV1(rsconf_t *conf, ruleset_t *const *shells, size_t count, rulesetNameBlockV1_t **out);
void rulesetNameIndexDiscardV1(rulesetNameBlockV1_t **block);
/* Serialized control thread; all shells/roots/list nodes are ready. Infallible.
 * No reader can retain the old owning list; only immutable blocks are searched. */
void rulesetNameIndexPublishV1(rsconf_t *conf, rulesetNameBlockV1_t **block);
/* All execution/readers stopped; borrows no shell after return. */
void rulesetNameIndexDestroyV1(rsconf_t *conf);
#endif
