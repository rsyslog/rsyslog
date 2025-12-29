/* persourceratelimit.c
 *
 * Implementation of per-source rate limiting.
 *
 * Copyright 2025 Adiscon GmbH.
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
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <ctype.h>
#ifdef HAVE_LIBYAML
#include <yaml.h>
#endif
#include "rsyslog.h"
#include "persourceratelimit.h"
#include "errmsg.h"
#include "hashtable.h"
#include "hashtable_itr.h"
#include "srUtils.h"
#include "statsobj.h"
#include "obj.h"

DEFobjCurrIf(statsobj);
DEFobjStaticHelpers;


/* Destructor for sender state */
static void freeSenderState(void *pPtr) {
    perSourceSenderState_t *pState = (perSourceSenderState_t *)pPtr;
    if (pState) {
        pthread_mutex_destroy(&pState->mut);
        free(pState);
    }
}

/* Destructor for override */
static void freeOverride(void *pPtr) {
    perSourceOverride_t *pOverride = (perSourceOverride_t *)pPtr;
    if (pOverride) {
        free(pOverride);
    }
}

rsRetVal perSourceRateLimiterNew(perSourceRateLimiter_t **ppThis) {
    perSourceRateLimiter_t *pThis;
    DEFiRet;
    CHKiRet(objGetObjInterface(&obj));

    CHKmalloc(pThis = (perSourceRateLimiter_t *)calloc(1, sizeof(perSourceRateLimiter_t)));
    pThis->defaultMax = 0; /* 0 means unlimited/disabled by default until loaded */
    pThis->defaultWindow = 1;

    STATSCOUNTER_INIT(pThis->ctrAllowed, pThis->mutCtrAllowed);
    STATSCOUNTER_INIT(pThis->ctrDropped, pThis->mutCtrDropped);

    if (pthread_rwlock_init(&pThis->rwlock, NULL) != 0) {
        ABORT_FINALIZE(RS_RET_ERR);
    }
    
    /* Create hashtables */
    pThis->htSenders = create_hashtable(100, hash_from_string, key_equals_string, freeSenderState);
    pThis->htOverrides = create_hashtable(20, hash_from_string, key_equals_string, freeOverride);

    if (pThis->htSenders == NULL || pThis->htOverrides == NULL) {
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }

    *ppThis = pThis;

finalize_it:
    if (iRet != RS_RET_OK) {
        if (pThis) {
            if (pThis->htSenders) hashtable_destroy(pThis->htSenders, 1);
            if (pThis->htOverrides) hashtable_destroy(pThis->htOverrides, 1);
            pthread_rwlock_destroy(&pThis->rwlock);
            free(pThis);
        }
    }
    RETiRet;
}

rsRetVal perSourceRateLimiterConstructFinalize(perSourceRateLimiter_t *pThis) {
    DEFiRet;
    uchar *statsName;
    
    if (objUse(statsobj, CORE_COMPONENT) != RS_RET_OK) {
        FINALIZE;
    }

    if (pThis->pszOrigin == NULL) {
        statsName = (uchar*)"perSourceRateLimiter";
    } else {
        statsName = pThis->pszOrigin;
    }

    CHKiRet(statsobj.Construct(&(pThis->stats)));
    CHKiRet(statsobj.SetName(pThis->stats, statsName));
    CHKiRet(statsobj.AddCounter(pThis->stats, (uchar*)"allowed",
        ctrType_IntCtr, CTR_FLAG_NONE, (void*)&pThis->ctrAllowed));
    CHKiRet(statsobj.AddCounter(pThis->stats, (uchar*)"dropped",
        ctrType_IntCtr, CTR_FLAG_NONE, (void*)&pThis->ctrDropped));
    CHKiRet(statsobj.ConstructFinalize(pThis->stats));

finalize_it:
    RETiRet;
}

rsRetVal perSourceRateLimiterDestruct(perSourceRateLimiter_t **ppThis) {
    perSourceRateLimiter_t *pThis;
    DEFiRet;

    if (ppThis == NULL || *ppThis == NULL) FINALIZE;
    pThis = *ppThis;

    pthread_rwlock_wrlock(&pThis->rwlock);
    hashtable_destroy(pThis->htSenders, 1);
    hashtable_destroy(pThis->htOverrides, 1);
    pthread_rwlock_unlock(&pThis->rwlock);
    
    if (pThis->stats != NULL) statsobj.Destruct(&(pThis->stats));
    if (pThis->pszOrigin != NULL) free(pThis->pszOrigin);
    if (pThis->pszPolicyFile != NULL) free(pThis->pszPolicyFile);
    pthread_rwlock_destroy(&pThis->rwlock);
    free(pThis);
    *ppThis = NULL;

finalize_it:
    RETiRet;
}

rsRetVal perSourceRateLimiterSetPolicyFile(perSourceRateLimiter_t *pThis, const uchar *pszFileName) {
    DEFiRet;
    if (pThis->pszPolicyFile) free(pThis->pszPolicyFile);
    CHKmalloc(pThis->pszPolicyFile = (uchar*)strdup((const char*)pszFileName));
finalize_it:
    RETiRet;
}

#ifdef HAVE_LIBYAML
static unsigned int parseDuration(const char *val) {
    unsigned int num = atoi(val);
    const char *p = val;
    while (*p && (*p >= '0' && *p <= '9')) p++;
    if (*p == 's') return num;
    if (*p == 'm') return num * 60;
    if (*p == 'h') return num * 3600;
    if (*p == 'd') return num * 86400;
    if (*p != '\0') {
        LogError(0, RS_RET_INVALID_VALUE, "perSourceRateLimiter: unknown duration suffix '%c' in '%s', ignoring suffix", *p, val);
    }
    return num;
}

static rsRetVal loadYamlPolicy(perSourceRateLimiter_t *pThis) {
    DEFiRet;
    FILE *fp = NULL;
    yaml_parser_t parser;
    yaml_event_t event;
    int done = 0;
    
    /* Parser state */
    int depth = 0;
    char *last_key = NULL;
    int section = 0; /* 0: none, 1: default, 2: overrides */
    
    /* Temporary override storage */
    char *curr_key = NULL;
    unsigned int curr_max = 0;
    unsigned int curr_window = 0;

    if (pThis->pszPolicyFile == NULL) FINALIZE;

    fp = fopen((const char*)pThis->pszPolicyFile, "r");
    if (fp == NULL) {
        LogError(errno, RS_RET_IO_ERROR, "perSourceRateLimiter: could not open policy file %s", pThis->pszPolicyFile);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }

    if (!yaml_parser_initialize(&parser)) {
        LogError(0, RS_RET_ERR, "perSourceRateLimiter: failed to initialize YAML parser");
        fclose(fp);
        ABORT_FINALIZE(RS_RET_ERR);
    }

    yaml_parser_set_input_file(&parser, fp);

    /* Clear existing overrides */
    hashtable_destroy(pThis->htOverrides, 1);
    pThis->htOverrides = create_hashtable(20, hash_from_string, key_equals_string, freeOverride);

    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) {
             LogError(0, RS_RET_ERR, "perSourceRateLimiter: YAML parser error %s at line %d", 
                      parser.problem ? parser.problem : "unknown", (int)parser.problem_mark.line);
             break;
        }

        switch (event.type) {
            case YAML_STREAM_END_EVENT:
                done = 1;
                break;
                
            case YAML_MAPPING_START_EVENT:
                if (last_key) {
                    if (depth == 1) {
                        if (strcmp(last_key, "default") == 0) section = 1;
                    }
                    free(last_key);
                    last_key = NULL;
                }
                depth++;
                break;
                
            case YAML_MAPPING_END_EVENT:
                if (section == 2 && depth == 3) { 
                    /* End of an override item */
                    if (curr_key && curr_max > 0) {
                        perSourceOverride_t *ovr = calloc(1, sizeof(perSourceOverride_t));
                        if (ovr == NULL) {
                            LogError(0, RS_RET_OUT_OF_MEMORY, "perSourceRateLimiter: "
                                     "failed to allocate override for %s", curr_key);
                            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                        }
                        char *key_copy = strdup(curr_key);
                        if (key_copy == NULL) {
                            free(ovr);
                            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
                        }
                        ovr->max = curr_max;
                        ovr->window = curr_window;
                        if (hashtable_insert(pThis->htOverrides, key_copy, ovr) == 0) {
                            free(key_copy);
                            free(ovr);
                            LogError(0, RS_RET_ERR, "perSourceRateLimiter: "
                                     "failed to insert override for %s", curr_key);
                        }
                    }
                    if (curr_key) free(curr_key);
                    curr_key = NULL;
                    curr_max = 0;
                    curr_window = 0;
                }
                depth--;
                if (depth == 0) section = 0;
                break;
                
            case YAML_SEQUENCE_START_EVENT:
                if (last_key) {
                    if (depth == 1 && strcmp(last_key, "overrides") == 0) section = 2;
                    free(last_key);
                    last_key = NULL;
                }
                depth++;
                break;
                
            case YAML_SEQUENCE_END_EVENT:
                depth--;
                break;
                
            case YAML_SCALAR_EVENT:
                {
                    char *val = (char *)event.data.scalar.value;
                    if (last_key == NULL) {
                        /* This is a key */
                        last_key = strdup(val);
                    } else {
                        /* This is a value */
                        if (section == 1) { /* default */
                            if (strcmp(last_key, "max") == 0) pThis->defaultMax = atoi(val);
                            else if (strcmp(last_key, "window") == 0) pThis->defaultWindow = parseDuration(val);
                        } else if (section == 2) { /* overrides */
                            if (strcmp(last_key, "key") == 0) {
                                if (curr_key) free(curr_key);
                                CHKmalloc(curr_key = strdup(val));
                            } else if (strcmp(last_key, "max") == 0) {
                                curr_max = atoi(val);
                            } else if (strcmp(last_key, "window") == 0) {
                                curr_window = parseDuration(val);
                            }
                        }
                        free(last_key);
                        last_key = NULL;
                    }
                }
                break;
            case YAML_NO_EVENT:
            case YAML_STREAM_START_EVENT:
            case YAML_DOCUMENT_START_EVENT:
            case YAML_DOCUMENT_END_EVENT:
            case YAML_ALIAS_EVENT:
                break;
            default:
                break;
        }
        
        yaml_event_delete(&event);
    }

    if (last_key) free(last_key);
    if (curr_key) free(curr_key);
    
    yaml_parser_delete(&parser);
    if (fp) fclose(fp);

finalize_it:
    RETiRet;
}
#endif

rsRetVal perSourceRateLimiterReload(perSourceRateLimiter_t *pThis) {
    DEFiRet;
    
    pthread_rwlock_wrlock(&pThis->rwlock);
    
#ifdef HAVE_LIBYAML
    iRet = loadYamlPolicy(pThis);
#else
    LogError(0, RS_RET_ERR, "perSourceRateLimiter: libyaml not available, cannot load policy");
    iRet = RS_RET_ERR;
#endif

    /* Note: We do not clear htSenders. Existing senders keep their state.
       However, if the policy changed, their limits might change.
       The check function looks up the limit every time (or we could update them here).
       Since we look up overrides in Check(), we don't need to update senders here.
    */

    pthread_rwlock_unlock(&pThis->rwlock);
    RETiRet;
}

rsRetVal perSourceRateLimiterCheck(perSourceRateLimiter_t *pThis, const uchar *key, time_t ttNow) {
    DEFiRet;
    perSourceSenderState_t *pState = NULL;
    perSourceOverride_t *pOverride = NULL;
    unsigned int limitMax = pThis->defaultMax;
    unsigned int limitWindow = pThis->defaultWindow;
    
    if (key == NULL) ABORT_FINALIZE(RS_RET_OK); /* No key, no limit */

    pthread_rwlock_rdlock(&pThis->rwlock);
    
    /* Find sender state */
    pState = (perSourceSenderState_t*)hashtable_search(pThis->htSenders, (void*)key);
    
    if (pState == NULL) {
        /* Upgrade lock to write to insert new sender */
        pthread_rwlock_unlock(&pThis->rwlock);
        pthread_rwlock_wrlock(&pThis->rwlock);
        
        /* Double check */
        pState = (perSourceSenderState_t*)hashtable_search(pThis->htSenders, (void*)key);
        if (pState == NULL) {
            pState = (perSourceSenderState_t*)calloc(1, sizeof(perSourceSenderState_t));
            if (pState) {
                if (pthread_mutex_init(&pState->mut, NULL) != 0) {
                    free(pState);
                    pState = NULL;
                } else {
                    pState->windowStart = ttNow;
                    char *pKeyStore = strdup((const char*)key);
                    if (pKeyStore == NULL || hashtable_insert(pThis->htSenders, pKeyStore, pState) == 0) {
                        if (pKeyStore) free(pKeyStore);
                        pthread_mutex_destroy(&pState->mut);
                        free(pState);
                        pState = NULL;
                    }
                }
            }
        }
        
        /* Downgrade to read lock to allow other readers. */
        pthread_rwlock_unlock(&pThis->rwlock);
        pthread_rwlock_rdlock(&pThis->rwlock);
        
        /* After re-acquiring read lock, we must search again to ensure pState is still valid
           (though in this specific implementation, once added it's not removed during Check) */
        pState = (perSourceSenderState_t*)hashtable_search(pThis->htSenders, (void*)key);
    }
    
    if (pState == NULL) {
        pthread_rwlock_unlock(&pThis->rwlock);
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    
    /* Determine limits */
    /* We need to look up override. */
    pOverride = (perSourceOverride_t*)hashtable_search(pThis->htOverrides, (void*)key);
    if (pOverride) {
        limitMax = pOverride->max;
        limitWindow = pOverride->window;
    }
    
    /* Check limit */
    pthread_mutex_lock(&pState->mut);
    
    if (ttNow >= pState->windowStart + limitWindow) {
        /* New window */
        pState->windowStart = ttNow;
        pState->currentCount = 0;
    }
    
    if (pState->currentCount < limitMax) {
        pState->currentCount++;
        STATSCOUNTER_INC(pThis->ctrAllowed, pThis->mutCtrAllowed);
        iRet = RS_RET_OK;
    } else {
        iRet = RS_RET_RATE_LIMITED;
        STATSCOUNTER_INC(pThis->ctrDropped, pThis->mutCtrDropped);
    }
    
    pthread_mutex_unlock(&pState->mut);
    pthread_rwlock_unlock(&pThis->rwlock);

finalize_it:
    RETiRet;
}
rsRetVal perSourceRateLimiterSetOrigin(perSourceRateLimiter_t *pThis, const uchar *pszOrigin) {
    DEFiRet;
    if (pThis->pszOrigin != NULL) free(pThis->pszOrigin);
    CHKmalloc(pThis->pszOrigin = (uchar*)strdup((const char*)pszOrigin));
finalize_it:
    RETiRet;
}
