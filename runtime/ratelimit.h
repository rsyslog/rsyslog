/* header for ratelimit.c
 *
 * Copyright 2012-2025 Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
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
#ifndef INCLUDED_RATELIMIT_H
#define INCLUDED_RATELIMIT_H

#include <stddef.h>
#include "rsyslog.h"
#include "statsobj.h"

struct hashtable;
struct ratelimit_ps_state_s;
struct template;
typedef struct statsobj_s statsobj_t;

typedef struct ratelimit_shared_s {
    char *name;
    unsigned int interval;
    unsigned int burst;
    intTiny severity;
    char *policy_file;
    pthread_mutex_t mut;
    sbool per_source_enabled;
    char *per_source_policy_file;
    unsigned int per_source_default_max;
    unsigned int per_source_default_window;
    unsigned int per_source_max_states;
    unsigned int per_source_topn;
    sbool per_source_key_tpl_default;
    char *per_source_key_tpl_name;
    struct template *per_source_key_tpl;
    sbool per_source_key_needs_parsing;
    enum ratelimit_ps_key_mode {
        RL_PS_KEY_TPL = 0,
        RL_PS_KEY_FROMHOST_IP,
        RL_PS_KEY_FROMHOST,
        RL_PS_KEY_FROMHOST_IP_PORT,
        RL_PS_KEY_FROMHOST_PORT
    } per_source_key_mode;
    struct hashtable *per_source_overrides;
    struct hashtable *per_source_states;
    struct ratelimit_ps_state_s *per_source_lru_head;
    struct ratelimit_ps_state_s *per_source_lru_tail;
    pthread_mutex_t per_source_mut;
    statsobj_t *per_source_stats;
    STATSCOUNTER_DEF(ctrPerSourceAllowed, mutCtrPerSourceAllowed);
    STATSCOUNTER_DEF(ctrPerSourceDropped, mutCtrPerSourceDropped);
    ctr_t **per_source_top_ctrs;
    intctr_t *per_source_top_values;
    char **per_source_top_keys;
} ratelimit_shared_t;

struct ratelimit_s {
    char *name; /**< rate limiter name, e.g. for user messages */
    ratelimit_shared_t *pShared;
    sbool bOwnsShared; /**< if we own pShared (and need to free it) */

    unsigned done;
    unsigned missed;
    time_t begin;
    /* support for "last message repeated n times */
    unsigned nsupp; /**< nbr of msgs suppressed */
    smsg_t *pMsg;
    sbool bThreadSafe; /**< do we need to operate in Thread-Safe mode? */
    sbool bNoTimeCache; /**< if we shall not used cached reception time */
    pthread_mutex_t mut; /**< mutex if thread-safe operation desired */
};

typedef struct ratelimit_cfgs_s {
    struct hashtable *ht;
    pthread_rwlock_t lock;
} ratelimit_cfgs_t;

/* prototypes */
typedef struct rsconf_s rsconf_t;
rsRetVal ratelimitNew(ratelimit_t **ppThis, const char *modname, const char *dynname);
rsRetVal ratelimitNewFromConfig(
    ratelimit_t **ppThis, rsconf_t *conf, const char *configname, const char *modname, const char *dynname);
rsRetVal ratelimitAddConfig(rsconf_t *conf,
                            const char *name,
                            unsigned int interval,
                            unsigned int burst,
                            intTiny severity,
                            const char *policy_file,
                            sbool per_source_enabled,
                            const char *per_source_policy_file,
                            const char *per_source_key_tpl_name,
                            unsigned int per_source_max_states,
                            unsigned int per_source_topn);
void ratelimit_cfgsInit(ratelimit_cfgs_t *cfgs);
void ratelimit_cfgsDestruct(ratelimit_cfgs_t *cfgs);
void ratelimitSetThreadSafe(ratelimit_t *ratelimit);
void ratelimitSetLinuxLike(ratelimit_t *ratelimit, unsigned int interval, unsigned int burst);
void ratelimitSetNoTimeCache(ratelimit_t *ratelimit);
void ratelimitSetSeverity(ratelimit_t *ratelimit, intTiny severity);
rsRetVal ratelimitMsgCount(ratelimit_t *ratelimit, time_t tt, const char *const appname);
rsRetVal ATTR_NONNULL(1, 2, 3) ratelimitMsg(ratelimit_t *ratelimit, smsg_t *pMsg, smsg_t **ppRep);
rsRetVal ATTR_NONNULL(1, 3) ratelimitAddMsg(ratelimit_t *ratelimit, multi_submit_t *pMultiSub, smsg_t *pMsg);
rsRetVal ATTR_NONNULL(1, 3) ratelimitAddMsgPerSource(ratelimit_t *ratelimit,
                                                     multi_submit_t *pMultiSub,
                                                     smsg_t *pMsg,
                                                     const char *per_source_key,
                                                     size_t per_source_key_len,
                                                     time_t tt);
void ratelimitDestruct(ratelimit_t *pThis);
int ratelimitChecked(ratelimit_t *ratelimit);
rsRetVal ratelimitModInit(void);
void ratelimitModExit(void);
void ratelimitDoHUP(void);

#endif /* #ifndef INCLUDED_RATELIMIT_H */
