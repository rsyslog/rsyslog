/* SPDX-License-Identifier: Apache-2.0
 * Private detached-ownership implementation shared with allocation-failure
 * tests; include after ruleset constructor/name/destructor implementations.
 */
/* Detached additions deliberately bypass ConstructFinalize/ProcessCnf. */
rsRetVal rulesetPrepareAdditionV1(const char *name, ruleset_t **shell, llElt_t **node) {
    DEFiRet;
    if (name == NULL || *name == '\0' || shell == NULL || *shell != NULL || node == NULL || *node != NULL)
        return RS_RET_PARAM_ERROR;
    CHKiRet(rulesetConstruct(shell));
    CHKiRet(rulesetSetName(*shell, (uchar *)name));
    CHKmalloc(*node = calloc(1, sizeof(**node)));
    CHKmalloc((*node)->pKey = strdup(name));
    (*node)->pData = *shell;
finalize_it:
    if (iRet != RS_RET_OK) rulesetDiscardAdditionV1(shell, node);
    RETiRet;
}
void rulesetDiscardAdditionV1(ruleset_t **shell, llElt_t **node) {
    if (node != NULL && *node != NULL) {
        free((*node)->pKey);
        free(*node);
        *node = NULL;
    }
    if (shell != NULL && *shell != NULL) rulesetDestruct(shell);
}
void rulesetPublishAdditionV1(rsconf_t *conf, llElt_t **node) {
    linkedList_t *const list = &conf->rulesets.llRulesets;
    /* Allocation, collision and integer capacity checks happened in prepare. */
    if (list->pLast == NULL)
        list->pRoot = *node;
    else
        list->pLast->pNext = *node;
    list->pLast = *node;
    ++list->iNumElts;
    *node = NULL;
}
