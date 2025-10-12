/* header for ratelimit.c
 *
 * Copyright 2012-2016 Adiscon GmbH.
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

#include <pthread.h>
#include <time.h>

#include "typedefs.h"
#include "rsyslog.h"

/* forward declaration for the configuration type exposed by this header */
struct ratelimit_config_s;

typedef struct ratelimit_config_s ratelimit_config_t;

#define RATELIMIT_SEVERITY_UNSET (-1)

/**
 * Immutable configuration parameters for a rate limit policy.
 *
 * Instances of this structure are populated either from a
 * :rainerscript:`ratelimit()` object or from inline configuration values
 * that are promoted to a shared configuration entry.
 */
typedef struct ratelimit_config_spec_s {
    unsigned int interval; /**< Rate limiting interval in seconds (0 disables). */
    unsigned int burst; /**< Maximum messages allowed inside @ref interval. */
    int severity; /**< Optional severity threshold, use @ref RATELIMIT_SEVERITY_UNSET for default. */
} ratelimit_config_spec_t;

struct cnfobj;

/**
 * Runtime state for a rate limited context.
 *
 * The structure holds the immutable configuration reference together with the
 * mutable counters required to apply rate limiting and repeat suppression at
 * runtime.
 */
struct ratelimit_s {
    const ratelimit_config_t *cfg; /**< Backing configuration. */
    unsigned int interval_override; /**< Legacy inline interval value. */
    unsigned int burst_override; /**< Legacy inline burst value. */
    intTiny severity_override; /**< Legacy inline severity threshold. */
    sbool has_override; /**< Indicates inline configuration is active. */
    char *name; /**< Instance name used for diagnostics. */
    unsigned done; /**< Messages processed inside the current interval. */
    unsigned missed; /**< Messages suppressed inside the current interval. */
    time_t begin; /**< Interval start timestamp. */
    unsigned nsupp; /**< Messages suppressed for repeat reduction. */
    smsg_t *pMsg; /**< Retained message for repeat reporting. */
    sbool bThreadSafe; /**< Guard ratelimiter with mutex operations. */
    sbool bNoTimeCache; /**< Re-evaluate time via time(2) instead of cached timestamp. */
    pthread_mutex_t mut; /**< Mutex used when @ref bThreadSafe or @ref bNoTimeCache is set. */
};

/**
 * Initialise a configuration specification with default values.
 *
 * @param spec Pointer to the specification that shall be initialised. The
 *     fields are reset to an "disabled" rate limit (interval and burst set to
 *     zero) and the severity threshold is marked as unset.
 */
void ratelimitConfigSpecInit(ratelimit_config_spec_t *spec);

/**
 * Prepare the ratelimit configuration store embedded inside @ref rsconf_t.
 *
 * @param cnf Active configuration object whose store shall be initialised.
 */
void ratelimitStoreInit(rsconf_t *cnf);

/**
 * Release all rate limit configurations that belong to @ref rsconf_t.
 *
 * @param cnf Active configuration object whose store shall be cleared.
 */
void ratelimitStoreDestruct(rsconf_t *cnf);

/**
 * Create a named configuration entry and remember it inside the configuration
 * store.
 *
 * @param cnf      Configuration that owns the store.
 * @param name     Unique name of the new configuration entry.
 * @param spec     Values that shall be stored for the entry.
 * @param cfg      Receives the created configuration on success.
 *
 * @retval RS_RET_OK on success.
 * @retval RS_RET_INVALID_VALUE if the name already exists.
 */
rsRetVal ratelimitConfigCreateNamed(rsconf_t *cnf,
                                    const char *name,
                                    const ratelimit_config_spec_t *spec,
                                    ratelimit_config_t **cfg);

/**
 * Create an anonymous configuration entry that is still stored inside the
 * configuration store.
 *
 * Ad-hoc entries are useful for backward compatibility when inline
 * ``ratelimit.*`` parameters are used. They are assigned a unique synthetic
 * name so that the object can later be referenced again (for example by
 * dynamically created worker contexts).
 *
 * @param cnf      Configuration that owns the store.
 * @param hint     Optional hint used to derive the synthetic name.
 * @param spec     Values that shall be stored for the entry.
 * @param cfg      Receives the created configuration on success.
 */
rsRetVal ratelimitConfigCreateAdHoc(rsconf_t *cnf,
                                    const char *hint,
                                    const ratelimit_config_spec_t *spec,
                                    ratelimit_config_t **cfg);

/**
 * Look up a configuration by name.
 *
 * @param cnf  Configuration that owns the store.
 * @param name Name of the configuration to look up.
 * @param cfg  Receives the configuration pointer on success.
 */
rsRetVal ratelimitConfigLookup(const rsconf_t *cnf, const char *name, ratelimit_config_t **cfg);

/**
 * Retrieve the name associated with a configuration entry.
 */
const char *ratelimitConfigName(const ratelimit_config_t *cfg);

/**
 * Return the interval (seconds) associated with a configuration entry.
 */
unsigned int ratelimitConfigGetInterval(const ratelimit_config_t *cfg);

/**
 * Return the burst size associated with a configuration entry.
 */
unsigned int ratelimitConfigGetBurst(const ratelimit_config_t *cfg);

/**
 * Return the severity threshold associated with a configuration entry.
 */
int ratelimitConfigGetSeverity(const ratelimit_config_t *cfg);

/**
 * Resolve a configuration reference by name.
 *
 * This helper is typically used by the RainerScript ratelimit object handler
 * itself.
 */
rsRetVal ratelimitResolveConfig(rsconf_t *cnf,
                                const char *hint,
                                const char *name,
                                sbool legacyParamsSpecified,
                                const ratelimit_config_spec_t *spec,
                                ratelimit_config_t **cfg_out);

/**
 * Resolve inline rate limit parameters against the configuration store.
 *
 * When a ``ratelimit.name`` reference is present the referenced configuration
 * is returned. Otherwise the inline values are promoted to an ad-hoc
 * configuration and stored inside the configuration object so that worker code
 * can re-use them.
 *
 * @param cnf      Configuration that owns the store.
 * @param hint     Human readable hint used when synthesising the ad-hoc name.
 * @param name     Optional name that should be resolved.
 * @param legacyParamsSpecified Indicates whether inline values were provided.
 * @param interval In/out interval value.
 * @param burst    In/out burst value.
 * @param severity In/out severity value (may be ``NULL`` if the caller does
 *                 not support a severity threshold).
 * @param cfg_out  Receives the resolved configuration pointer on success.
 */
rsRetVal ratelimitResolveFromValues(rsconf_t *cnf,
                                    const char *hint,
                                    const char *name,
                                    sbool legacyParamsSpecified,
                                    unsigned int *interval,
                                    unsigned int *burst,
                                    int *severity,
                                    ratelimit_config_t **cfg_out);

/**
 * Handle a declarative ``ratelimit()`` object during configuration loading.
 */
rsRetVal ratelimitProcessCnf(struct cnfobj *o);

/* rate limiter instance operations */
/**
 * Create a new ratelimiter that is configured via explicit parameters.
 *
 * This helper is intended for callers that do not have access to an
 * ::rsconf_t/registry context (for example early bootstrap code or tooling).
 * When a configuration object *is* available, prefer ::ratelimitNewFromConfig
 * so the shared registry entry can be reused by future instances.
 *
 * @param ppThis      Receives the constructed instance.
 * @param modname     Name of the owner (typically the module).
 * @param dynname     Optional dynamic component appended to the diagnostic
 *                    name.
 */
rsRetVal ratelimitNew(ratelimit_t **ppThis, const char *modname, const char *dynname);

/**
 * Create a new ratelimiter that uses an immutable configuration entry.
 *
 * @param ppThis        Receives the constructed instance.
 * @param cfg           Configuration entry to use.
 * @param instance_name Optional diagnostic name used for logging.
 */
rsRetVal ratelimitNewFromConfig(ratelimit_t **ppThis, const ratelimit_config_t *cfg, const char *instance_name);

/** Enable thread-safe operation for the given ratelimiter. */
void ratelimitSetThreadSafe(ratelimit_t *ratelimit);

/**
 * Configure the ratelimiter with inline settings.
 */
void ratelimitSetLinuxLike(ratelimit_t *ratelimit, unsigned int interval, unsigned int burst);

/**
 * Disable usage of cached timestamps. Used for sources that supply
 * externally provided timestamps (e.g. imjournal).
 */
void ratelimitSetNoTimeCache(ratelimit_t *ratelimit);

/**
 * Set the severity threshold for inline ratelimiters.
 */
void ratelimitSetSeverity(ratelimit_t *ratelimit, intTiny severity);

/**
 * Apply rate limiting logic that operates purely on message counts.
 */
rsRetVal ratelimitMsgCount(ratelimit_t *ratelimit, time_t tt, const char *const appname);

/**
 * Apply rate limiting and repeat suppression to a message.
 */
rsRetVal ratelimitMsg(ratelimit_t *ratelimit, smsg_t *pMsg, smsg_t **ppRep);

/**
 * Add a message to a ratelimiter-aware multisubmit batch.
 */
rsRetVal ratelimitAddMsg(ratelimit_t *ratelimit, multi_submit_t *pMultiSub, smsg_t *pMsg);

/** Destroy a ratelimiter instance. */
void ratelimitDestruct(ratelimit_t *pThis);

/**
 * Determine whether runtime checks are active for the given ratelimiter.
 */
int ratelimitChecked(ratelimit_t *ratelimit);

/** Initialise the ratelimit module (object system hook). */
rsRetVal ratelimitModInit(void);

/** Deinitialise the ratelimit module (object system hook). */
void ratelimitModExit(void);

#endif /* #ifndef INCLUDED_RATELIMIT_H */
