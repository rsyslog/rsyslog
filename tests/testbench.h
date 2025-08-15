/* Defines for a rsyslog standard testbench application.
 *
 * Work begun 2008-06-13 by Rainer Gerhards (written from scratch)
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
#include <stdlib.h>

/* everything we need to begin a testbench */
#define MODULE_TYPE_TESTBENCH                                                           \
    /* definitions for objects we access */                                             \
    DEFobjCurrIf(obj)                                                                   \
                                                                                        \
        static rsRetVal                                                                 \
        doInit(void);                                                                   \
    static rsRetVal doTest(void);                                                       \
    static rsRetVal doExit(void);                                                       \
                                                                                        \
    /* Below is the driver, which is always the same */                                 \
    int main(int __attribute__((unused)) argc, char __attribute__((unused)) * argv[]) { \
        DEFiRet;                                                                        \
        CHKiRet(doInit());                                                              \
        CHKiRet(doTest());                                                              \
        CHKiRet(doExit());                                                              \
    finalize_it:                                                                        \
        if (iRet != RS_RET_OK) printf("test returns iRet %d\n", iRet);                  \
        RETiRet;                                                                        \
    }


/* Initialize everything (most importantly the runtime objects) for the test. The framework
 * initializes the global runtime, user must add those objects that it needs additionally.
 */
#define BEGINInit                                                                                  \
    static rsRetVal doInit(void) {                                                                 \
        DEFiRet;                                                                                   \
        char *pErrObj; /* tells us which object failed if that happens */                          \
        putenv("RSYSLOG_MODDIR=../runtime/.libs/"); /* this is a bit hackish... */                 \
                                                                                                   \
        dbgClassInit();                                                                            \
        /* Intialize the runtime system */                                                         \
        pErrObj = "rsyslog runtime"; /* set in case the runtime errors before setting an object */ \
        CHKiRet(rsrtInit(&pErrObj, &obj));

#define CODESTARTInit

#define ENDInit                                                           \
    finalize_it:                                                          \
    if (iRet != RS_RET_OK) {                                              \
        printf("failure occurred during init of object '%s'\n", pErrObj); \
    }                                                                     \
                                                                          \
    RETiRet;                                                              \
    }


/* Carry out the actual test...
 */
#define BEGINTest           \
    rsRetVal doTest(void) { \
        DEFiRet;

#define CODESTARTTest

#define ENDTest \
    RETiRet;    \
    }


/* De-init everything (most importantly the runtime objects) for the test.  */
#define BEGINExit           \
    rsRetVal doExit(void) { \
        DEFiRet;            \
        CHKiRet(rsrtExit());

#define CODESTARTExit

#define ENDExit  \
    finalize_it: \
    RETiRet;     \
    }
