/* timezones.c
 * Support for timezones in RainerScript.
 *
 * Copyright 2022 Attila Lakatos and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
 *		 -or-
 *		 see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <stdint.h>
#include <errno.h>

#include "rsyslog.h"
#include "unicode-helper.h"
#include "errmsg.h"
#include "parserif.h"
#include "rainerscript.h"
#include "srUtils.h"
#include "rsconf.h"


static struct cnfparamdescr timezonecnfparamdescr[] = {{"id", eCmdHdlrString, CNFPARAM_REQUIRED},
                                                       {"offset", eCmdHdlrGetWord, CNFPARAM_REQUIRED}};
static struct cnfparamblk timezonepblk = {
    CNFPARAMBLK_VERSION, sizeof(timezonecnfparamdescr) / sizeof(struct cnfparamdescr), timezonecnfparamdescr};

/* Note: this function is NOT thread-safe!
 * This is currently not needed as used only during
 * initialization.
 */
static rsRetVal addTimezoneInfo(rsconf_t *cnf, uchar *tzid, char offsMode, int8_t offsHour, int8_t offsMin) {
    DEFiRet;
    tzinfo_t *newti;
    CHKmalloc(newti = realloc(cnf->timezones.tzinfos, (cnf->timezones.ntzinfos + 1) * sizeof(tzinfo_t)));
    if ((newti[cnf->timezones.ntzinfos].id = strdup((char *)tzid)) == NULL) {
        free(newti);
        DBGPRINTF("addTimezoneInfo: strdup failed with OOM\n");
        ABORT_FINALIZE(RS_RET_OUT_OF_MEMORY);
    }
    newti[cnf->timezones.ntzinfos].offsMode = offsMode;
    newti[cnf->timezones.ntzinfos].offsHour = offsHour;
    newti[cnf->timezones.ntzinfos].offsMin = offsMin;
    ++cnf->timezones.ntzinfos, cnf->timezones.tzinfos = newti;
finalize_it:
    RETiRet;
}

void glblProcessTimezone(struct cnfobj *o) {
    struct cnfparamvals *pvals;
    uchar *id = NULL;
    uchar *offset = NULL;
    char offsMode;
    int8_t offsHour;
    int8_t offsMin;
    int i;

    pvals = nvlstGetParams(o->nvlst, &timezonepblk, NULL);
    if (pvals == NULL) {
        LogError(0, RS_RET_MISSING_CNFPARAMS,
                 "error processing timezone "
                 "config parameters");
        goto done;
    }
    if (Debug) {
        dbgprintf("timezone param blk after glblProcessTimezone:\n");
        cnfparamsPrint(&timezonepblk, pvals);
    }

    for (i = 0; i < timezonepblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(timezonepblk.descr[i].name, "id")) {
            id = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else if (!strcmp(timezonepblk.descr[i].name, "offset")) {
            offset = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL);
        } else {
            dbgprintf(
                "glblProcessTimezone: program error, non-handled "
                "param '%s'\n",
                timezonepblk.descr[i].name);
        }
    }

    /* note: the following two checks for NULL are not strictly necessary
     * as these are required parameters for the config block. But we keep
     * them to make the clang static analyzer happy, which also helps
     * guard against logic errors.
     */
    if (offset == NULL) {
        parser_errmsg("offset parameter missing (logic error?), timezone config ignored");
        goto done;
    }
    if (id == NULL) {
        parser_errmsg("id parameter missing (logic error?), timezone config ignored");
        goto done;
    }

    if (strlen((char *)offset) != 6 || !(offset[0] == '-' || offset[0] == '+') ||
        !(isdigit(offset[1]) && isdigit(offset[2])) || offset[3] != ':' ||
        !(isdigit(offset[4]) && isdigit(offset[5]))) {
        parser_errmsg("timezone offset has invalid format. Must be +/-hh:mm, e.g. \"-07:00\".");
        goto done;
    }

    offsHour = (offset[1] - '0') * 10 + offset[2] - '0';
    offsMin = (offset[4] - '0') * 10 + offset[5] - '0';
    offsMode = offset[0];

    if (offsHour > 12 || offsMin > 59) {
        parser_errmsg("timezone offset outside of supported range (hours 0..12, minutes 0..59)");
        goto done;
    }

    addTimezoneInfo(loadConf, id, offsMode, offsHour, offsMin);

done:
    cnfparamvalsDestruct(pvals, &timezonepblk);
    free(id);
    free(offset);
}

/* comparison function for qsort() and string array compare
 * this is for the string lookup table type
 */
static int qs_arrcmp_tzinfo(const void *s1, const void *s2) {
    return strcmp(((tzinfo_t *)s1)->id, ((tzinfo_t *)s2)->id);
}

void sortTimezones(rsconf_t *cnf) {
    if (cnf->timezones.ntzinfos > 0) {
        qsort(cnf->timezones.tzinfos, cnf->timezones.ntzinfos, sizeof(tzinfo_t), qs_arrcmp_tzinfo);
    }
}

void displayTimezones(rsconf_t *cnf) {
    if (!Debug) return;
    for (int i = 0; i < cnf->timezones.ntzinfos; ++i)
        dbgprintf("tzinfo: '%s':%c%2.2d:%2.2d\n", cnf->timezones.tzinfos[i].id, cnf->timezones.tzinfos[i].offsMode,
                  cnf->timezones.tzinfos[i].offsHour, cnf->timezones.tzinfos[i].offsMin);
}

static int bs_arrcmp_tzinfo(const void *s1, const void *s2) {
    return strcmp((char *)s1, (char *)((tzinfo_t *)s2)->id);
}

/* returns matching timezone info or NULL if no entry exists */
tzinfo_t *glblFindTimezone(rsconf_t *cnf, char *id) {
    return (tzinfo_t *)bsearch(id, cnf->timezones.tzinfos, cnf->timezones.ntzinfos, sizeof(tzinfo_t), bs_arrcmp_tzinfo);
}

static void freeTimezone(tzinfo_t *tzinfo) {
    free(tzinfo->id);
}

void freeTimezones(rsconf_t *cnf) {
    for (int i = 0; i < cnf->timezones.ntzinfos; ++i) freeTimezone(&cnf->timezones.tzinfos[i]);
    if (cnf->timezones.ntzinfos > 0) free(cnf->timezones.tzinfos);
    cnf->timezones.tzinfos = NULL;
}
