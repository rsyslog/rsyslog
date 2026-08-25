/**
 * @file omsendertrack.c
 * @brief Track and persist statistics for message senders.
 *
 * The omsendertrack module maintains a table of message senders and
 * periodically writes the collected statistics to a JSON file.  It is
 * currently a proof-of-concept as described in
 * <https://github.com/rsyslog/rsyslog/issues/5599>.  Not all functionality is
 * implemented yet -- for example support for reading a command file on HUP
 * is still missing.
 *
 * Note: there are TODO items in this module which will remain until the end
 *       of the PoC phase. This is expected and intended. However, they should
 *       no longer be present in the year 2026 or later.
 *
 * Copyright 2025 Adiscon GmbH.
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
#include "rsyslog.h"
#include "atomic.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "template.h"
#include "module-template.h"
#include "errmsg.h"
#include "hashtable.h"
#include "hashtable_itr.h"
// #include "cfsysline.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("omsendertrack")

// TODO CI: a) more tests, multiple hosts, b) make current tests really check the counted stats, not just abort ;-)
// TODO: HUP processing

/* static data */

/* internal structures
 */
DEF_OMOD_STATIC_DATA;

#define DEFAULT_INTERVAL 60 /* seconds */

/* module data structures */


typedef struct {
    const uchar *sender;
    uint64_t nMsgs;
    time_t lastSeen;
    time_t firstSeen;
    pthread_mutex_t mut;
} sender_stats_t;

static void destroySenderStats(void *const value) {
    sender_stats_t *const stat = (sender_stats_t *)value;

    pthread_mutex_destroy(&stat->mut);
    free(stat);
}


/* config variables */

/**
 * @brief Configuration and runtime data for an action instance.
 */
typedef struct _instanceData {
    int interval; /**< write interval in seconds */
    uchar *statefile; /**< path to the JSON state file */
    int bIgnoreInvalidStatefile; /**< recover from malformed persisted state */
    int bDisableStatefileWrites; /**< preserve a corrupt file when its backup failed */
    uchar *senderidTemplate; /**< template that defines sender ID */
    pthread_rwlock_t mutSenders; /**< protects the sender hash table */
    int mutSendersInitialized; /**< indicates sender hash table lock is initialized */
    struct hashtable *stats_senders; /**< hash table of sender_stats_t */
    int bShutdownBackgroundWriter; /**< tells bgwriter to terminate */
    DEF_ATOMIC_HELPER_MUT(mutShutdownBackgroundWriter);
    pthread_t bgw_tid; /**< thread ID of background writer */
    int bgw_initialized; /**< indicates thread started */
} instanceData;

/** Worker context passed to modules API functions. */
typedef struct wrkrInstanceData {
    instanceData *pData; /**< pointer back to action instance */
} wrkrInstanceData_t;

/**
 * @brief Defines the configuration parameters for an action() object instance.
 *
 * This structure is the standard interface for rsyslog action modules to
 * declare their supported configuration parameters. The rsyslog core
 * configuration engine uses this information to parse and apply directives
 * from `rsyslog.conf` to an instance of this action.
 *
 * Each entry maps a configuration directive string to its handler and properties.
 * Note that other module types (like inputs) use a similar, separate structure
 * for their specific parameters.
 */
static struct cnfparamdescr actpdescr[] = {
    /** @param interval at which the sender state file is written. */
    {"interval", eCmdHdlrPositiveInt, 0},
    /** @param statefile State file for the statistics object. */
    {"statefile", eCmdHdlrString, 1}, /* required */
    /** @param ignoreinvalidstatefile Continue with empty state after corrupt state file. */
    {"ignoreinvalidstatefile", eCmdHdlrBinary, 0},
    /**
     * @param cmdfile Compatibility parameter for unimplemented command-file processing.
     */
    {"cmdfile", eCmdHdlrString, 0},
    /** @param template Template to use for the output format. */
    {"senderid", eCmdHdlrGetWord, 0}};
static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};

struct modConfData_s {
    rsconf_t *pConf; /* our overall config object */
};

static modConfData_t *loadModConf = NULL; /* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL; /* modConf ptr to use for the current exec process */

/* forward references */
static rsRetVal initHashtable(instanceData *const pData);
static void *bgWriter(void *arg);


/**
 * Add a new sender statistics entry.
 *
 * @param pData      module instance data
 * @param sender     identifier of the sender
 * @param nMsgs      number of messages already seen
 * @param firstSeen  timestamp of the first message
 * @param lastSeen   timestamp of the last message
 * @retval RS_RET_OK on success
 */
static rsRetVal addSender(instanceData *const pData,
                          const char *const sender,
                          const uint64_t nMsgs,
                          const time_t firstSeen,
                          const time_t lastSeen) {
    sender_stats_t *stat;
    DEFiRet;

    DBGPRINTF("addSender: Sender: %s, Messages: %" PRIu64 ", FirstSeen: %" PRIdMAX ", LastSeen: %" PRIdMAX "\n", sender,
              nMsgs, (intmax_t)firstSeen, (intmax_t)lastSeen);

    CHKmalloc(stat = calloc(1, sizeof(sender_stats_t)));
    CHKmalloc(stat->sender = (const uchar *)strdup((const char *)sender));
    stat->nMsgs = nMsgs;
    stat->firstSeen = firstSeen;
    stat->lastSeen = lastSeen;
    pthread_mutex_init(&stat->mut, NULL);
    if (hashtable_insert(pData->stats_senders, (void *)stat->sender, (void *)stat) == 0) {
        LogError(errno, RS_RET_INTERNAL_ERROR,
                 "omsendertrack error inserting sender '%s' into sender "
                 "hash table - entry lost",
                 sender);
        pthread_mutex_destroy(&stat->mut);
        free((void *)stat->sender);
        free(stat);
        stat = NULL;
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }

finalize_it:
    if (iRet != RS_RET_OK && stat != NULL) {
        free((void *)stat->sender);
        free(stat);
    }
    RETiRet;
}


typedef enum { STATEFILE_VALID, STATEFILE_MISSING, STATEFILE_EMPTY, STATEFILE_INVALID } statefileReadResult_t;

static int isJsonWhitespace(const char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static int int64FitsTimeT(const int64_t value) {
    const time_t converted = (time_t)value;

    if ((time_t)-1 > (time_t)0 && value < 0) {
        return 0;
    }
    return (int64_t)converted == value;
}

static rsRetVal ATTR_NONNULL() checkStatefileParentDir(const char *const statefile) {
    int fd = -1;
    char *dir = NULL;
    const char *slash;
    DEFiRet;

    slash = strrchr(statefile, '/');
    if (slash == NULL) {
        CHKmalloc(dir = strdup("."));
    } else if (slash == statefile) {
        CHKmalloc(dir = strdup("/"));
    } else {
        const size_t len = (size_t)(slash - statefile);
        CHKmalloc(dir = strndup(statefile, len));
    }

    fd = open(dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd == -1) {
        LogError(errno, RS_RET_FILE_OPEN_ERROR, "omsendertrack: cannot access state file directory '%s'", dir);
        ABORT_FINALIZE(RS_RET_FILE_OPEN_ERROR);
    }

finalize_it:
    if (fd != -1) {
        close(fd);
    }
    free(dir);
    RETiRet;
}

static rsRetVal ATTR_NONNULL() fsyncStatefileParentDir(const char *const statefile) {
    int fd = -1;
    char *dir = NULL;
    const char *slash;
    DEFiRet;

    slash = strrchr(statefile, '/');
    if (slash == NULL) {
        CHKmalloc(dir = strdup("."));
    } else if (slash == statefile) {
        CHKmalloc(dir = strdup("/"));
    } else {
        const size_t len = (size_t)(slash - statefile);
        CHKmalloc(dir = strndup(statefile, len));
    }

    fd = open(dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd == -1) {
        LogError(errno, RS_RET_IO_ERROR, "omsendertrack: cannot open state file directory '%s' for fsync", dir);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    if (fsync(fd) == -1) {
        LogError(errno, RS_RET_IO_ERROR, "omsendertrack: cannot fsync state file directory '%s'", dir);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }

finalize_it:
    if (fd != -1) {
        close(fd);
    }
    free(dir);
    RETiRet;
}

static int ATTR_NONNULL() senderStateEntryIsValid(json_object *const entry) {
    json_object *jSender, *jMessages, *jFirstseen, *jLastseen;
    int64_t value;
    const char *sender;
    int senderLen;

    if (!json_object_is_type(entry, json_type_object) || !json_object_object_get_ex(entry, "sender", &jSender) ||
        !json_object_object_get_ex(entry, "messages", &jMessages) ||
        !json_object_object_get_ex(entry, "firstseen", &jFirstseen) ||
        !json_object_object_get_ex(entry, "lastseen", &jLastseen) || !json_object_is_type(jSender, json_type_string) ||
        !json_object_is_type(jMessages, json_type_int) || !json_object_is_type(jFirstseen, json_type_int) ||
        !json_object_is_type(jLastseen, json_type_int)) {
        return 0;
    }

    sender = json_object_get_string(jSender);
    senderLen = json_object_get_string_len(jSender);
    if (sender == NULL || senderLen < 0 || memchr(sender, '\0', (size_t)senderLen) != NULL) {
        return 0;
    }

    value = json_object_get_int64(jMessages);
    if (value < 0) {
        return 0;
    }
    value = json_object_get_int64(jFirstseen);
    if (!int64FitsTimeT(value)) {
        return 0;
    }
    value = json_object_get_int64(jLastseen);
    if (!int64FitsTimeT(value)) {
        return 0;
    }

    return 1;
}

static int ATTR_NONNULL() senderStateIsValid(json_object *const jsonTree) {
    struct hashtable *seenSenders = NULL;
    int valid = 0;

    if (!json_object_is_type(jsonTree, json_type_array)) {
        return 0;
    }

    seenSenders = create_hashtable(100, hash_from_string, key_equals_string, NULL);
    if (seenSenders == NULL) {
        return 0;
    }

    const size_t arrayLen = json_object_array_length(jsonTree);
    for (size_t i = 0; i < arrayLen; ++i) {
        json_object *const entry = json_object_array_get_idx(jsonTree, i);
        json_object *jSender;
        const char *sender;
        char *senderCopy;

        if (!senderStateEntryIsValid(entry)) {
            goto finalize_it;
        }
        json_object_object_get_ex(entry, "sender", &jSender);
        sender = json_object_get_string(jSender);
        if (hashtable_search(seenSenders, (void *)sender) != NULL) {
            goto finalize_it;
        }
        senderCopy = strdup(sender);
        if (senderCopy == NULL) {
            goto finalize_it;
        }
        if (hashtable_insert(seenSenders, senderCopy, (void *)1) == 0) {
            free(senderCopy);
            goto finalize_it;
        }
    }

    valid = 1;
finalize_it:
    hashtable_destroy(seenSenders, 0);
    return valid;
}

/**
 * Read sender statistics from the configured state file.
 *
 * @param pData     module instance data
 * @param[out] jsontree  parsed JSON tree or NULL on failure
 * @param[out] result classification of an otherwise readable state file
 * @retval RS_RET_OK on success
 */
static rsRetVal ATTR_NONNULL()
    readSenderStats(instanceData *const pData, json_object **const jsontree, statefileReadResult_t *const result) {
    int fd = -1;
    struct stat sb;
    char *contents = NULL;
    size_t offset = 0;
    json_tokener *tok = NULL;
    json_object *parsedJson = NULL;
    DEFiRet;

    *jsontree = NULL;
    *result = STATEFILE_VALID;
    fd = open((const char *)pData->statefile, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd == -1) {
        if (errno == ENOENT) {
            CHKiRet(checkStatefileParentDir((const char *)pData->statefile));
            *result = STATEFILE_MISSING;
            CHKmalloc(*jsontree = json_object_new_array());
            FINALIZE;
        }
        LogError(errno, RS_RET_FILE_OPEN_ERROR, "omsendertrack: cannot open state file '%s'", pData->statefile);
        ABORT_FINALIZE(RS_RET_FILE_OPEN_ERROR);
    }
    if (fstat(fd, &sb) == -1) {
        LogError(errno, RS_RET_IO_ERROR, "omsendertrack: cannot stat state file '%s'", pData->statefile);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    if (!S_ISREG(sb.st_mode)) {
        LogError(0, RS_RET_IO_ERROR, "omsendertrack: state file '%s' is not a regular file", pData->statefile);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    if (sb.st_size == 0) {
        *result = STATEFILE_EMPTY;
        CHKmalloc(*jsontree = json_object_new_array());
        FINALIZE;
    }
    if (sb.st_size < 0 || sb.st_size > INT_MAX) {
        *result = STATEFILE_INVALID;
        FINALIZE;
    }

    CHKmalloc(contents = malloc((size_t)sb.st_size));
    while (offset < (size_t)sb.st_size) {
        const ssize_t nread = read(fd, contents + offset, (size_t)sb.st_size - offset);
        if (nread < 0 && errno == EINTR) {
            continue;
        }
        if (nread <= 0) {
            LogError(errno, RS_RET_READ_ERR, "omsendertrack: cannot read complete state file '%s'", pData->statefile);
            ABORT_FINALIZE(RS_RET_READ_ERR);
        }
        offset += (size_t)nread;
    }

    CHKmalloc(tok = json_tokener_new());
    fjson_tokener_set_flags(tok, FJSON_TOKENER_STRICT);
    parsedJson = json_tokener_parse_ex(tok, contents, (int)sb.st_size);
    if (fjson_tokener_get_error(tok) != fjson_tokener_success || parsedJson == NULL) {
        *result = STATEFILE_INVALID;
        FINALIZE;
    }
    offset = (size_t)tok->char_offset;
    while (offset < (size_t)sb.st_size && isJsonWhitespace(contents[offset])) {
        ++offset;
    }
    if (offset != (size_t)sb.st_size || !senderStateIsValid(parsedJson)) {
        *result = STATEFILE_INVALID;
        FINALIZE;
    }

    *jsontree = parsedJson;
    parsedJson = NULL;
finalize_it:
    if (fd != -1) {
        close(fd);
    }
    if (tok != NULL) {
        json_tokener_free(tok);
    }
    if (parsedJson != NULL) {
        json_object_put(parsedJson);
    }
    free(contents);
    RETiRet;
}

static rsRetVal ATTR_NONNULL() quarantineStatefile(const char *const statefile) {
    char *quarantine = NULL;
    struct timespec ts;
    int attempt;
    DEFiRet;

    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        ts.tv_sec = time(NULL);
        ts.tv_nsec = 0;
    }
    for (attempt = 0; attempt < 100; ++attempt) {
        if (asprintf(&quarantine, "%s.corrupt.%ld.%ld.%d", statefile, (long)ts.tv_sec, (long)ts.tv_nsec, attempt) ==
            -1) {
            quarantine = NULL;
            ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
        }
        /* link() reserves the destination without replacing an older backup. */
        if (link(statefile, quarantine) == 0) {
            if (unlink(statefile) != 0) {
                const int unlinkErrno = errno;
                unlink(quarantine);
                LogError(unlinkErrno, RS_RET_IO_ERROR,
                         "omsendertrack: cannot finalize preservation of invalid state file '%s'", statefile);
                ABORT_FINALIZE(RS_RET_IO_ERROR);
            }
            CHKiRet(fsyncStatefileParentDir(statefile));
            LogError(0, NO_ERRCODE, "omsendertrack: moved invalid state file '%s' to '%s' and started with empty state",
                     statefile, quarantine);
            FINALIZE;
        }
        if (errno != EEXIST) {
            LogError(errno, RS_RET_IO_ERROR, "omsendertrack: cannot preserve invalid state file '%s' as '%s'",
                     statefile, quarantine);
            ABORT_FINALIZE(RS_RET_IO_ERROR);
        }
        free(quarantine);
        quarantine = NULL;
    }

    LogError(0, RS_RET_IO_ERROR, "omsendertrack: cannot find a unique backup name for invalid state file '%s'",
             statefile);
    ABORT_FINALIZE(RS_RET_IO_ERROR);

finalize_it:
    free(quarantine);
    RETiRet;
}

static rsRetVal
/**
 * Convert a JSON tree into the sender hash table.
 *
 * @param pData     module instance data
 * @param jsonTree  JSON array of sender information
 * @retval RS_RET_OK on success
 */
jsonToHashtable(instanceData *const pData, json_object *const jsonTree)
{
    DEFiRet;

    size_t array_len = json_object_array_length(jsonTree);
    for (size_t i = 0; i < array_len; i++) {
        json_object *entry = json_object_array_get_idx(jsonTree, i);

        /* The tree was fully validated before this conversion. */
        json_object *j_sender, *j_messages, *j_firstseen, *j_lastseen;
        json_object_object_get_ex(entry, "sender", &j_sender);
        json_object_object_get_ex(entry, "messages", &j_messages);
        json_object_object_get_ex(entry, "firstseen", &j_firstseen);
        json_object_object_get_ex(entry, "lastseen", &j_lastseen);
        CHKiRet(addSender(pData, json_object_get_string(j_sender), (uint64_t)json_object_get_int64(j_messages),
                          (time_t)json_object_get_int64(j_firstseen), (time_t)json_object_get_int64(j_lastseen)));
    }

finalize_it:
    RETiRet;
}

static rsRetVal
/**
 * Initialize the sender hash table and start the background writer.
 *
 * @param pData module instance data
 * @retval RS_RET_OK on success
 */
initHashtable(instanceData *const pData)
{
    DEFiRet;
    statefileReadResult_t readResult;
    json_object *jsonTree = NULL;

    if ((pData->stats_senders = create_hashtable(100, hash_from_string, key_equals_string, destroySenderStats)) ==
        NULL) {
        LogError(0, RS_RET_INTERNAL_ERROR,
                 "error trying to initialize hash-table "
                 "for sender table. Sender statistics and warnings are disabled.");
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);  // TODO: check status!
    }

    /* read existing data */
    CHKiRet(readSenderStats(pData, &jsonTree, &readResult));
    if (readResult == STATEFILE_EMPTY || readResult == STATEFILE_INVALID) {
        if (!pData->bIgnoreInvalidStatefile) {
            LogError(0, RS_RET_ERR,
                     "omsendertrack: state file '%s' is empty or invalid and IgnoreInvalidStatefile is off",
                     pData->statefile);
            ABORT_FINALIZE(RS_RET_ERR);
        }
        if (readResult == STATEFILE_INVALID && quarantineStatefile((const char *)pData->statefile) != RS_RET_OK) {
            pData->bDisableStatefileWrites = 1;
            LogError(0, NO_ERRCODE,
                     "omsendertrack: continuing with empty state, but state file writes are disabled to preserve '%s'",
                     pData->statefile);
        } else if (readResult == STATEFILE_EMPTY) {
            LogError(0, NO_ERRCODE, "omsendertrack: state file '%s' is empty; starting with empty state",
                     pData->statefile);
        }
        if (jsonTree == NULL) {
            CHKmalloc(jsonTree = json_object_new_array());
        }
    }
    CHKiRet(jsonToHashtable(pData, jsonTree));
    json_object_put(jsonTree);
    jsonTree = NULL;

    /* start background writer */
    pData->bgw_initialized = 0;
    if (pthread_rwlock_init(&pData->mutSenders, NULL) != 0) {
        LogError(0, RS_RET_ERR, "omsendertrack: cannot initialize sender-state lock");
        ABORT_FINALIZE(RS_RET_ERR);
    }
    pData->mutSendersInitialized = 1;
    if (pthread_create(&pData->bgw_tid, NULL, bgWriter, pData) != 0) {
        LogError(0, RS_RET_ERR,
                 "omsendertrack: cannot create background writing thread. "
                 "No interim files will be written!");
        ABORT_FINALIZE(RS_RET_ERR);
    }
    pData->bgw_initialized = 1;

finalize_it:
    if (jsonTree != NULL) {
        json_object_put(jsonTree);
    }
    RETiRet;
}


#if 0
static void ATTR_NONNULL()
doFunc_parse_json(struct cnffunc *__restrict__ const func,
	struct svar *__restrict__ const ret,
	void *const usrptr,
	wti_t *const pWti)
{
	int bMustFree;
	int bMustFree2;
	smsg_t *const pMsg = (smsg_t*)usrptr;
	struct json_object *json;

	int retVal;
	assert(jsontext != NULL);
	assert(container != NULL);
	assert(pMsg != NULL);

	struct json_tokener *const tokener = json_tokener_new();
	if(tokener == NULL) {
		retVal = 1;
		goto finalize_it;
	}
	json = json_tokener_parse_ex(tokener, jsontext, strlen(jsontext));
	if(json == NULL) {
		retVal = RS_SCRIPT_EINVAL;
	} else {
		size_t off = (*container == '$') ? 1 : 0;
		msgAddJSON(pMsg, (uchar*)container+off, json, 0, 0);
		retVal = RS_SCRIPT_EOK;
	}
	wtiSetScriptErrno(pWti, retVal);
	json_tokener_free(tokener);


finalize_it:

	if(bMustFree) {
		free(jsontext);
	}
	if(bMustFree2) {
		free(container);
	}
}
#endif


/* this function writes the actual sender stats. */
/**
 * Write all sender statistics to the FILE pointer.
 *
 * @param pData module instance data
 * @param fp    open FILE pointer to write JSON to
 * @retval RS_RET_OK on success
 */
static rsRetVal writeSenderStats(instanceData *const pData, FILE *const fp) {
    struct hashtable_itr *itr = NULL;
    sender_stats_t *stat;
    json_object *jSender = NULL;
    int bNeedEOL = 0;
    int lockHeld = 0;
    DEFiRet;

    dbgprintf("writeSenderStats() called, hashtable_count %d\n", hashtable_count(pData->stats_senders));
    if (fprintf(fp, "[\n") < 0) { /* begin JSON array */
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }

    pthread_rwlock_rdlock(&pData->mutSenders);
    lockHeld = 1;

    /* Iterator constructor only returns a valid iterator if
     * the hashtable is not empty
     */
    if (hashtable_count(pData->stats_senders) > 0) {
        itr = hashtable_iterator(pData->stats_senders);
        do {
            uint64_t nMsgs;
            time_t firstSeen;
            time_t lastSeen;

            stat = (sender_stats_t *)hashtable_iterator_value(itr);
            pthread_mutex_lock(&stat->mut);
            jSender = json_object_new_string((const char *)stat->sender);
            nMsgs = stat->nMsgs;
            firstSeen = stat->firstSeen;
            lastSeen = stat->lastSeen;
            pthread_mutex_unlock(&stat->mut);
            if (jSender == NULL) {
                ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
            }
            if (nMsgs > INT64_MAX) {
                LogError(0, RS_RET_IO_ERROR, "omsendertrack: sender message count exceeds the JSON state-file range");
                ABORT_FINALIZE(RS_RET_IO_ERROR);
            }
            if (fprintf(fp,
                        "%s{"
                        "\"sender\":%s"
                        ",\"messages\":%" PRIu64 ",\"firstseen\":%" PRIdMAX ",\"lastseen\":%" PRIdMAX "}",
                        (bNeedEOL ? ",\n" : ""), json_object_to_json_string_ext(jSender, JSON_C_TO_STRING_PLAIN), nMsgs,
                        (intmax_t)firstSeen, (intmax_t)lastSeen) < 0) {
                ABORT_FINALIZE(RS_RET_IO_ERROR);
            }
            json_object_put(jSender);
            jSender = NULL;
            bNeedEOL = 1;
        } while (hashtable_iterator_advance(itr));
    }

    if (fprintf(fp, "%s]\n", bNeedEOL ? "\n" : "") < 0) { /* end JSON array */
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }

finalize_it:
    if (jSender != NULL) {
        json_object_put(jSender);
    }
    free(itr);
    if (lockHeld) {
        pthread_rwlock_unlock(&pData->mutSenders);
    }
    RETiRet;
}

/**
 * Persist sender statistics to disk.
 *
 * A fully synchronized temporary file is renamed into place only after all
 * data has reached the filesystem. The parent directory is synchronized after
 * the rename so the name change itself survives a power loss.
 *
 * @param pData module instance data
 * @retval RS_RET_OK on success
 */
static ATTR_NONNULL() rsRetVal writeSenderInfo(instanceData *const pData) {
    DEFiRet;
    FILE *fp = NULL;
    char *tmpname = NULL;
    int renamed = 0;
    struct stat existingStateStat;
    int haveExistingState = 0;

    dbgprintf("writeSenderInfo, file %s\n", pData->statefile);
    if (pData->statefile == NULL) {
        LogError(0, RS_RET_INTERNAL_ERROR, "omsendertrack: statefile is not configured");
        ABORT_FINALIZE(RS_RET_INTERNAL_ERROR);
    }
    if (pData->bDisableStatefileWrites) {
        FINALIZE;
    }
    if (fstatat(AT_FDCWD, (const char *)pData->statefile, &existingStateStat, AT_SYMLINK_NOFOLLOW) == 0) {
        if (!S_ISREG(existingStateStat.st_mode)) {
            LogError(0, RS_RET_IO_ERROR, "omsendertrack: state file '%s' is not a regular file", pData->statefile);
            ABORT_FINALIZE(RS_RET_IO_ERROR);
        }
        haveExistingState = 1;
    } else if (errno != ENOENT) {
        LogError(errno, RS_RET_IO_ERROR, "omsendertrack: cannot inspect existing state file '%s'", pData->statefile);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    const size_t statefileLen = strlen((const char *)pData->statefile);
    CHKmalloc(tmpname = malloc(statefileLen + sizeof(".tmp.XXXXXX")));
    memcpy(tmpname, pData->statefile, statefileLen);
    memcpy(tmpname + statefileLen, ".tmp.XXXXXX", sizeof(".tmp.XXXXXX"));

    const int fd = mkstemp(tmpname);
    if (fd == -1) {
        LogError(errno, RS_RET_IO_ERROR, "omsendertrack: cannot create temporary state file '%s'", tmpname);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    if (haveExistingState && fchmod(fd, existingStateStat.st_mode & 0777) != 0) {
        LogError(errno, RS_RET_IO_ERROR, "omsendertrack: cannot preserve permissions of state file '%s'",
                 pData->statefile);
        close(fd);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
        LogError(errno, RS_RET_IO_ERROR, "omsendertrack: cannot set close-on-exec on '%s'", tmpname);
        close(fd);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    if ((fp = fdopen(fd, "w")) == NULL) {
        LogError(errno, RS_RET_IO_ERROR, "omsendertrack: cannot open temporary state file '%s'", tmpname);
        close(fd);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }

    CHKiRet(writeSenderStats(pData, fp));
    if (fflush(fp) == EOF || fsync(fileno(fp)) == -1) {
        LogError(errno, RS_RET_IO_ERROR, "omsendertrack: cannot flush temporary state file '%s'", tmpname);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    if (fclose(fp) == EOF) {
        fp = NULL;
        LogError(errno, RS_RET_IO_ERROR, "omsendertrack: cannot close temporary state file '%s'", tmpname);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    fp = NULL;

    if (rename(tmpname, (const char *)pData->statefile) != 0) {
        LogError(errno, RS_RET_IO_ERROR, "omsendertrack: cannot rename '%s' to configured state file '%s'", tmpname,
                 pData->statefile);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }
    renamed = 1;
    CHKiRet(fsyncStatefileParentDir((const char *)pData->statefile));

finalize_it:
    if (fp != NULL) {
        fclose(fp);
    }
    if (iRet != RS_RET_OK && !renamed && tmpname != NULL) {
        unlink(tmpname);
    }
    free(tmpname);
    RETiRet;
}


/**
 * Background thread periodically persisting sender statistics.
 *
 * @param arg pointer to instanceData
 * @return always NULL
 */
static void *bgWriter(void *arg) {
    instanceData *pData = (instanceData *)arg;

    /* block all signals except SIGTTIN and SIGSEGV */
    sigset_t sigSet;
    sigfillset(&sigSet);
    sigdelset(&sigSet, SIGTTIN);
    sigdelset(&sigSet, SIGSEGV);
    pthread_sigmask(SIG_BLOCK, &sigSet, NULL);
    assert(pData->statefile != NULL); /* parameter is mandatory, as such must be set */

    uchar thrdName[32];
    snprintf((char *)thrdName, sizeof(thrdName), "omsendertrack/bgw");  // TODO: instance-identifier?
    dbgSetThrdName(thrdName);
    dbgprintf("bgWriter started\n");

    /* set thread name - we ignore if it fails, has no harsh consequences... */
#if defined(HAVE_PRCTL) && defined(PR_SET_NAME)
    if (prctl(PR_SET_NAME, thrdName, 0, 0, 0) != 0) {
        DBGPRINTF("prctl failed, not setting thread name for '%s'\n", thrdName);
    }
#elif defined(HAVE_PTHREAD_SETNAME_NP)
    int r = pthread_setname_np(pthread_self(), (char *)thrdName);
    if (r != 0) {
        DBGPRINTF("pthread_setname_np failed, not setting thread name for '%s'\n", thrdName);
    }
#endif

    while (ATOMIC_LOAD_32BIT_RELAXED(&pData->bShutdownBackgroundWriter, &pData->mutShutdownBackgroundWriter) == 0) {
        srSleep(pData->interval, 0);
        if (ATOMIC_LOAD_32BIT_RELAXED(&pData->bShutdownBackgroundWriter, &pData->mutShutdownBackgroundWriter) == 1) {
            break;
        }
        dbgprintf("bgwriter writing report file\n");
        writeSenderInfo(pData);
    }

    dbgprintf("bgWriter finished\n");
    return NULL;
}


/**
 * Update statistics for a message sender.
 *
 * This function updates an existing entry or creates a new one
 * if the sender is seen for the first time.
 *
 * @param pData   module instance data
 * @param sender  identifier of the sender
 * @param lastSeen timestamp of the current message
 */
static rsRetVal recordSender(instanceData *const pData, const uchar *const sender, const time_t lastSeen) {
    sender_stats_t *stat;
    DEFiRet;
    int needUpdate = 1;

    assert(pData->stats_senders != NULL);

    pthread_rwlock_rdlock(&pData->mutSenders);
    stat = hashtable_search(pData->stats_senders, (void *)sender);
    if (stat == NULL) {
        /* we now need to write to the hash table */
        pthread_rwlock_unlock(&pData->mutSenders);
        pthread_rwlock_wrlock(&pData->mutSenders);

        // Re-check in case another writer added it
        stat = hashtable_search(pData->stats_senders, (void *)sender);
        if (stat == NULL) {
            DBGPRINTF("recordSender: sender '%s' not found, adding\n", sender);
            CHKiRet(addSender(pData, (const char *)sender, 1, lastSeen, lastSeen));
            needUpdate = 0;
        }
    }

    if (needUpdate) {
        /* this mutex is for the atomic update of a single sender record. We do NOT
         * expect much contention on it.
         */
        pthread_mutex_lock(&stat->mut);
        stat->nMsgs++;
        stat->lastSeen = lastSeen;
        pthread_mutex_unlock(&stat->mut);
    }

    DBGPRINTF("omsendertrack: recordSender: '%s', lastSeen %llu\n", sender, (long long unsigned)lastSeen);

finalize_it:
    pthread_rwlock_unlock(&pData->mutSenders);
    RETiRet;
}

BEGINinitConfVars
    CODESTARTinitConfVars;
ENDinitConfVars

BEGINcreateInstance
    CODESTARTcreateInstance;
ENDcreateInstance


BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
ENDcreateWrkrInstance


BEGINbeginCnfLoad
    CODESTARTbeginCnfLoad;
    loadModConf = pModConf;
    pModConf->pConf = pConf;
ENDbeginCnfLoad


BEGINendCnfLoad
    CODESTARTendCnfLoad;
    loadModConf = NULL; /* done loading */
ENDendCnfLoad

BEGINcheckCnf
    CODESTARTcheckCnf;
ENDcheckCnf

BEGINactivateCnf
    CODESTARTactivateCnf;
    runModConf = pModConf;
ENDactivateCnf

BEGINfreeCnf
    CODESTARTfreeCnf;
ENDfreeCnf


BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURERepeatedMsgReduction) iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
    CODESTARTfreeInstance;
    /* stop bgWriter */
    if (pData->bgw_initialized) {
        ATOMIC_STORE_32BIT_RELAXED(&pData->bShutdownBackgroundWriter, &pData->mutShutdownBackgroundWriter, 1);
        pthread_kill(pData->bgw_tid, SIGTTIN);

        /* wait until stopped */
        dbgprintf("waiting for bgWriter to finish\n");
        pthread_join(pData->bgw_tid, NULL);
    }

    assert(pData->statefile != NULL); /* parameter is mandatory, as such must be set */
    /* Do a final write only after successful sender-state initialization. */
    if (pData->mutSendersInitialized) {
        writeSenderInfo(pData);
    }

    /* destroy data structs */
    free((void *)pData->senderidTemplate);
    free((void *)pData->statefile);
    if (pData->mutSendersInitialized) {
        pthread_rwlock_destroy(&pData->mutSenders);
    }
    DESTROY_ATOMIC_HELPER_MUT(pData->mutShutdownBackgroundWriter);
    if (pData->stats_senders != NULL) {
        hashtable_destroy(pData->stats_senders, 1); /* 1 => free all values automatically */
    }
ENDfreeInstance


BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
ENDfreeWrkrInstance


BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
    dbgprintf("omsendertrack\n");
    dbgprintf("\tsenderid='%s'\n", pData->senderidTemplate);
ENDdbgPrintInstInfo


BEGINtryResume
    CODESTARTtryResume;
ENDtryResume

BEGINbeginTransaction
    CODESTARTbeginTransaction;
ENDbeginTransaction

BEGINcommitTransaction
    CODESTARTcommitTransaction;
    const time_t lastSeen = time(NULL); /* do only query once per TX - it's sufficiently precise */
    for (unsigned i = 0; i < nParams; ++i) {
        recordSender(pWrkrData->pData, actParam(pParams, 1, i, 0).param, lastSeen);
    }
ENDcommitTransaction


static rsRetVal setInstParamDefaults(instanceData *pData) {
    DEFiRet;
    pData->interval = DEFAULT_INTERVAL;
    pData->bIgnoreInvalidStatefile = 1;
    pData->bDisableStatefileWrites = 0;
    CHKmalloc(pData->senderidTemplate = (uchar *)strdup(" StdOmSenderTrack-senderid"));
finalize_it:
    RETiRet;
}


BEGINnewActInst
    struct cnfparamvals *pvals;
    int i;
    int bDestructPValsOnExit;
    uchar *tplToUse;
    CODESTARTnewActInst;
    DBGPRINTF("newActInst (omsendertrack)\n");

    bDestructPValsOnExit = 0;
    pvals = nvlstGetParams(lst, &actpblk, NULL);
    if (pvals == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS,
                 "omsendertrack: error reading "
                 "config parameters");
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }
    bDestructPValsOnExit = 1;

    if (Debug) {
        dbgprintf("action param blk in omsendertrack:\n");
        cnfparamsPrint(&actpblk, pvals);
    }

    CHKiRet(createInstance(&pData));
    INIT_ATOMIC_HELPER_MUT(pData->mutShutdownBackgroundWriter);
    CHKiRet(setInstParamDefaults(pData));

    for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) {
            continue;
        } else if (!strcmp(actpblk.descr[i].name, "interval")) {
            pData->interval = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "statefile")) {
            CHKmalloc(pData->statefile = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(actpblk.descr[i].name, "ignoreinvalidstatefile")) {
            pData->bIgnoreInvalidStatefile = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "senderid")) {
            free((void *)pData->senderidTemplate);  // free default template
            CHKmalloc(pData->senderidTemplate = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(actpblk.descr[i].name, "cmdfile")) {
            /* Accepted for compatibility. Command-file processing is not implemented. */
        } else {
            DBGPRINTF(
                "omsendertrack: program error, non-handled "
                "param '%s'\n",
                actpblk.descr[i].name);
        }
    }

    /* Enforce required param (defensive in addition to descriptor flag). */
    if (pData->statefile == NULL || ((const char *)pData->statefile)[0] == '\0') {
        LogError(0, RS_RET_CONF_RQRD_PARAM_MISSING, "omsendertrack: 'statefile' parameter is required");
        ABORT_FINALIZE(RS_RET_CONF_RQRD_PARAM_MISSING);
    }

    CODE_STD_STRING_REQUESTnewActInst(1);
    tplToUse = (uchar *)strdup((pData->senderidTemplate == NULL) ? " StdOmSenderTrack-senderid"
                                                                 : (char *)pData->senderidTemplate);
    CHKiRet(OMSRsetEntry(*ppOMSR, 0, tplToUse, OMSR_NO_RQD_TPL_OPTS));

    CHKiRet(initHashtable(pData));
    CODE_STD_FINALIZERnewActInst;
    if (bDestructPValsOnExit) cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINparseSelectorAct
    CODESTARTparseSelectorAct;
    CODE_STD_STRING_REQUESTparseSelectorAct(1)
        /* first check if this config line is actually for us */
        if (strncmp((char *)p, ":omsendertrack:", sizeof(":omsendertrack:") - 1)) {
        ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
    }

    /* ok, if we reach this point, we have something for us */
    p += sizeof(":omsendertrack:") - 1; /* eat indicator sequence  (-1 because of '\0'!) */
    CHKiRet(createInstance(&pData));
    INIT_ATOMIC_HELPER_MUT(pData->mutShutdownBackgroundWriter);

    /* check if a non-standard template is to be applied */
    if (*(p - 1) == ';') --p;
    CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


BEGINmodExit
    CODESTARTmodExit;
ENDmodExit


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_OMODTX_QUERIES;
    CODEqueryEtryPt_STD_OMOD8_QUERIES;
    CODEqueryEtryPt_STD_CONF2_CNFNAME_QUERIES;
    CODEqueryEtryPt_STD_CONF2_QUERIES;
    CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
ENDqueryEtryPt


BEGINmodInit()
    CODESTARTmodInit;
    INITLegCnfVars;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr
    /* old-style system not supported */
ENDmodInit
