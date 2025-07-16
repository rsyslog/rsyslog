/* This test checks runtime initialization and exit. Other than that, it
 * also serves as the most simplistic sample of how a test can be coded.
 *
 * Part of the testbench for rsyslog.
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <glob.h>
#include <sys/stat.h>

#include "rsyslog.h"
#include "testbench.h"
#include "ctok.h"
#include "expr.h"

rsconf_t *ourConf;
MODULE_TYPE_TESTBENCH
/* define addtional objects we need for our tests */
DEFobjCurrIf(expr) DEFobjCurrIf(ctok) DEFobjCurrIf(ctok_token) DEFobjCurrIf(vmprg)


    BEGINInit CODESTARTInit;
pErrObj = "expr";
CHKiRet(objUse(expr, CORE_COMPONENT));
pErrObj = "ctok";
CHKiRet(objUse(ctok, CORE_COMPONENT));
pErrObj = "ctok_token";
CHKiRet(objUse(ctok_token, CORE_COMPONENT));
pErrObj = "vmprg";
CHKiRet(objUse(vmprg, CORE_COMPONENT));
ENDInit

BEGINExit
    CODESTARTExit;
ENDExit


/* perform a single test. This involves compiling the test script,
 * checking the result of the compilation (iRet) and a check of the
 * generated program (via a simple strcmp). The resulting program
 * check is only done if the test should not detect a syntax error
 * (for obvious reasons, there is no point in checking the result of
 * a failed compilation).
 * rgerhards, 2008-07--07
 */
static rsRetVal PerformTest(cstr_t *pstrIn, rsRetVal iRetExpected, cstr_t *pstrOut) {
    cstr_t *pstrPrg = NULL;
    ctok_t *tok = NULL;
    ctok_token_t *pToken = NULL;
    expr_t *pExpr;
    rsRetVal localRet;
    DEFiRet;

    /* we first need a tokenizer... */
    CHKiRet(ctok.Construct(&tok));
    CHKiRet(ctok.Setpp(tok, rsCStrGetSzStr(pstrIn)));
    CHKiRet(ctok.ConstructFinalize(tok));

    /* now construct our expression */
    CHKiRet(expr.Construct(&pExpr));
    CHKiRet(expr.ConstructFinalize(pExpr));

    /* ready to go... */
    localRet = expr.Parse(pExpr, tok);

    /* check if we expected an error */
    if (localRet != iRetExpected) {
        printf("Error in compile return code. Expected %d, received %d\n", iRetExpected, localRet);
        CHKiRet(rsCStrConstruct(&pstrPrg));
        CHKiRet(vmprg.Obj2Str(pExpr->pVmprg, pstrPrg));
        printf("generated vmprg:\n%s\n", rsCStrGetSzStr(pstrPrg));
        ABORT_FINALIZE(iRetExpected == RS_RET_OK ? localRet : RS_RET_ERR);
    }

    if (iRetExpected != RS_RET_OK) FINALIZE; /* if we tested an error case, we are done */

    /* OK, we got a compiled program, so now let's compare that */

    CHKiRet(rsCStrConstruct(&pstrPrg));
    CHKiRet(vmprg.Obj2Str(pExpr->pVmprg, pstrPrg));

    if (strcmp((char *)rsCStrGetSzStr(pstrPrg), (char *)rsCStrGetSzStr(pstrOut))) {
        printf("error: compiled program different from expected result!\n");
        printf("generated vmprg (%d bytes):\n%s\n", (int)strlen((char *)rsCStrGetSzStr(pstrPrg)),
               rsCStrGetSzStr(pstrPrg));
        printf("expected (%d bytes):\n%s\n", (int)strlen((char *)rsCStrGetSzStr(pstrOut)), rsCStrGetSzStr(pstrOut));
        ABORT_FINALIZE(RS_RET_ERR);
    }

finalize_it:
    /* we are done, so we now need to restore things */
    if (pToken != NULL) ctok_token.Destruct(&pToken); /* no longer needed */
    if (pstrPrg != NULL) rsCStrDestruct(&pstrPrg);
    if (tok != NULL) ctok.Destruct(&tok);
    RETiRet;
}


/* a helper macro to generate some often-used code... */
#define CHKEOF                                                             \
    if (feof(fp)) {                                                        \
        printf("error: unexpected end of control file %s\n", pszFileName); \
        ABORT_FINALIZE(RS_RET_ERR);                                        \
    }
/* process a single test file
 * Note that we do not do a real parser here. The effort is not
 * justified by what we need to do. So it is a quick shot.
 * rgerhards, 2008-07-07
 */
static rsRetVal ProcessTestFile(uchar *pszFileName) {
    FILE *fp;
    char *lnptr = NULL;
    size_t lenLn;
    cstr_t *pstrIn = NULL;
    cstr_t *pstrOut = NULL;
    int iParse;
    rsRetVal iRetExpected;
    DEFiRet;

    if ((fp = fopen((char *)pszFileName, "r")) == NULL) {
        perror((char *)pszFileName);
        ABORT_FINALIZE(RS_RET_FILE_NOT_FOUND);
    }

    /* skip comments at start of file */

    getline(&lnptr, &lenLn, fp);
    while (!feof(fp)) {
        if (*lnptr == '#')
            getline(&lnptr, &lenLn, fp);
        else
            break; /* first non-comment */
    }
    CHKEOF;

    /* once we had a comment, the next line MUST be "result: <nbr>". Anything
     * after nbr is simply ignored.
     */
    if (sscanf(lnptr, "result: %d", &iParse) != 1) {
        printf("error in result line, scanf failed, line: '%s'\n", lnptr);
        ABORT_FINALIZE(RS_RET_ERR);
    }
    iRetExpected = iParse;
    getline(&lnptr, &lenLn, fp);
    CHKEOF;

    /* and now we look for "in:" (and again ignore the rest...) */
    if (strncmp(lnptr, "in:", 3)) {
        printf("error: expected 'in:'-line, but got: '%s'\n", lnptr);
        ABORT_FINALIZE(RS_RET_ERR);
    }
    /* if we reach this point, we need to read in the input script. It is
     * terminated by a line with three sole $ ($$$\n)
     */
    CHKiRet(rsCStrConstruct(&pstrIn));
    getline(&lnptr, &lenLn, fp);
    CHKEOF;
    while (strncmp(lnptr, "$$$\n", 4)) {
        CHKiRet(rsCStrAppendStr(pstrIn, (uchar *)lnptr));
        getline(&lnptr, &lenLn, fp);
        CHKEOF;
    }
    getline(&lnptr, &lenLn, fp);
    CHKEOF; /* skip $$$-line */

    /* and now we look for "out:" (and again ignore the rest...) */
    if (strncmp(lnptr, "out:", 4)) {
        printf("error: expected 'out:'-line, but got: '%s'\n", lnptr);
        ABORT_FINALIZE(RS_RET_ERR);
    }
    /* if we reach this point, we need to read in the expected program code. It is
     * terminated by a line with three sole $ ($$$\n)
     */
    CHKiRet(rsCStrConstruct(&pstrOut));
    getline(&lnptr, &lenLn, fp);
    CHKEOF;
    while (strncmp(lnptr, "$$$\n", 4)) {
        CHKiRet(rsCStrAppendStr(pstrOut, (uchar *)lnptr));
        getline(&lnptr, &lenLn, fp);
        CHKEOF;
    }

    /* un-comment for testing:
     * printf("iRet: %d, script: %s\n, out: %s\n", iRetExpected, rsCStrGetSzStr(pstrIn),rsCStrGetSzStr(pstrOut));
     */
    if (rsCStrGetSzStr(pstrIn) == NULL) {
        printf("error: input script is empty!\n");
        ABORT_FINALIZE(RS_RET_ERR);
    }
    if (rsCStrGetSzStr(pstrOut) == NULL && iRetExpected == RS_RET_OK) {
        printf("error: output script is empty!\n");
        ABORT_FINALIZE(RS_RET_ERR);
    }

    CHKiRet(PerformTest(pstrIn, iRetExpected, pstrOut));

finalize_it:
    if (pstrIn != NULL) rsCStrDestruct(&pstrIn);
    if (pstrOut != NULL) rsCStrDestruct(&pstrOut);
    RETiRet;
}


/* This test is parameterized. It search for test control files and
 * loads all that it finds. To add tests, simply create new .rstest
 * files.
 * rgerhards, 2008-07-07
 */
BEGINTest
    uchar *testFile;
    glob_t testFiles;
    size_t i = 0;
    struct stat fileInfo;
    CODESTARTTest;
    glob("*.rstest", GLOB_MARK, NULL, &testFiles);

    for (i = 0; i < testFiles.gl_pathc; i++) {
        testFile = (uchar *)testFiles.gl_pathv[i];

        if (stat((char *)testFile, &fileInfo) != 0)
            continue; /* continue with the next file if we can't stat() the file */

        /* all regular files are run through the test logic. Symlinks don't work. */
        if (S_ISREG(fileInfo.st_mode)) { /* config file */
            printf("processing RainerScript test file '%s'...\n", testFile);
            iRet = ProcessTestFile((uchar *)testFile);
            if (iRet != RS_RET_OK) {
                /* in this case, re-run with debugging on */
                printf("processing test case failed with %d, re-running with debug messages:\n", iRet);
                Debug = 1; /* these two are dirty, but we need them today... */
                debugging_on = 1;
                CHKiRet(ProcessTestFile((uchar *)testFile));
            }
        }
    }
    globfree(&testFiles);

finalize_it:
ENDTest
