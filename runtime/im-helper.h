/* SPDX-License-Identifier: Apache-2.0 */
/* im-helper.h
 * This file contains helper constructs that save time writing input modules. It
 * assumes some common field names and plumbing. It is intended to be used together
 * with module-template.h
 *
 * File begun on 2011-05-04 by RGerhards
 *
 * Copyright 2011-2016 Rainer Gerhards and Adiscon GmbH.
 *
 * Licensed under the Apache License, Version 2.0. See COPYING.ASL20.
 */
#ifndef IM_HELPER_H_INCLUDED
#define IM_HELPER_H_INCLUDED 1

#include "parserif.h"
#include "ruleset.h"

/* The following function provides a complete implementation to check a
 * ruleset and set the actual ruleset pointer. The macro assumes that
 * standard field names are used. A functon std_checkRuleset_genErrMsg()
 * must be defined to generate error messages in case the ruleset cannot
 * be found.
 */
static inline void std_checkRuleset_genErrMsg(modConfData_t *modConf, instanceConf_t *inst);
static inline rsRetVal std_checkRuleset(modConfData_t *modConf, instanceConf_t *inst) {
    ruleset_t *pRuleset;
    rsRetVal localRet;
    DEFiRet;

    inst->pBindRuleset = NULL; /* assume default ruleset */

    if (inst->pszBindRuleset == NULL) FINALIZE;

    localRet = ruleset.GetRuleset(modConf->pConf, &pRuleset, inst->pszBindRuleset);
    if (localRet == RS_RET_NOT_FOUND) {
        if (modConf->pConf->globals.bRequireAllInputs) {
            LogError(0, RS_RET_NOT_FOUND, "input-bound ruleset '%s' not found; strict input startup will stop",
                     inst->pszBindRuleset);
            modConf->pConf->globals.bInputRulesetMissing = 1;
        } else {
            std_checkRuleset_genErrMsg(modConf, inst);
        }
    }
    CHKiRet(localRet);
    inst->pBindRuleset = pRuleset;
    if (!rulesetHasQueue(pRuleset)) {
        parser_warnmsg(
            "input-bound ruleset '%s' has no dedicated non-direct queue; "
            "input processing will share the main queue or run synchronously",
            (const char *)inst->pszBindRuleset);
    }

finalize_it:
    RETiRet;
}

#endif /* #ifndef IM_HELPER_H_INCLUDED */
