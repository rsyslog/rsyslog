/* ommail.c
 *
 * This is an implementation of a mail sending output module. It supports
 * direct SMTP, that is talking to a SMTP server, and delivery through a
 * sendmail-compatible local submission program. Please note that the SMTP
 * protocol implementation is a very bare one. We support RFC821/822
 * messages, without any authentication and any other nice features (no MIME,
 * no nothing). It is assumed that proper firewalling and/or SMTP server
 * configuration is used together with this module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * File begun on 2008-04-04 by RGerhards
 *
 * Copyright 2008-2014 Adiscon GmbH.
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
#include <syslog.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/wait.h>
#if defined(HAVE_LINUX_CLOSE_RANGE_H)
    #include <linux/close_range.h>
#endif
#include "conf.h"
#include "syslogd-types.h"
#include "srUtils.h"
#include "cfsysline.h"
#include "module-template.h"
#include "errmsg.h"
#include "datetime.h"
#include "glbl.h"
#include "parserif.h"

MODULE_TYPE_OUTPUT;
MODULE_TYPE_NOKEEP;
MODULE_CNFNAME("ommail")

/* internal structures
 */
DEF_OMOD_STATIC_DATA;
DEFobjCurrIf(glbl) DEFobjCurrIf(datetime)

#define DEFAULT_SENDMAIL_BINARY "/usr/sbin/sendmail"

    typedef enum {
        OMMailModeSMTP = 0,
        OMMailModeSendmail = 1
    } ommailMode_t;

typedef struct sendmailStatus_s {
    int waitStatus;
    int execErrno;
} sendmailStatus_t;

/* we add a little support for multiple recipients. We do this via a
 * singly-linked list, enqueued from the top. -- rgerhards, 2008-08-04
 */
typedef struct toRcpt_s toRcpt_t;
struct toRcpt_s {
    uchar *pszTo;
    toRcpt_t *pNext;
};

typedef struct _instanceData {
    uchar *tplName; /* format template to use */
    uchar *constSubject; /* if non-NULL, constant string to be used as subject */
    uchar *pszFrom;
    toRcpt_t *lstRcpt;
    ommailMode_t mode;
    sbool bHaveSubject; /* is a subject configured? (if so, it is the second string provided by rsyslog core) */
    sbool bEnableBody; /* is a body configured? (if so, it is the second string provided by rsyslog core) */
    struct {
        struct {
            uchar *pszSrv;
            uchar *pszSrvPort;
        } smtp;
        struct {
            uchar *szBinary;
        } sendmail;
    } md; /* mode-specific data */
} instanceData;

typedef struct wrkrInstanceData {
    instanceData *pData;
    union {
        struct {
            char RcvBuf[1024]; /* buffer for receiving server responses */
            size_t lenRcvBuf;
            size_t iRcvBuf; /* current index into the rcvBuf (buf empty if iRcvBuf == lenRcvBuf) */
            int sock; /* socket to this server (most important when we do multiple msgs per mail) */
        } smtp;
    } md; /* mode-specific data */
} wrkrInstanceData_t;

typedef struct configSettings_s {
    toRcpt_t *lstRcpt;
    uchar *pszSrv;
    uchar *pszSrvPort;
    uchar *pszFrom;
    uchar *pszSubject;
    int bEnableBody; /* should a mail body be generated? (set to 0 eg for SMS gateways) */
} configSettings_t;
static configSettings_t cs;

/* tables for interfacing with the v6 config system */
/* action (instance) parameters */
static struct cnfparamdescr actpdescr[] = {{"server", eCmdHdlrGetWord, 0},
                                           {"port", eCmdHdlrGetWord, 0},
                                           {"mailfrom", eCmdHdlrGetWord, CNFPARAM_REQUIRED},
                                           {"mailto", eCmdHdlrArray, CNFPARAM_REQUIRED},
                                           {"subject.template", eCmdHdlrGetWord, 0},
                                           {"subject.text", eCmdHdlrString, 0},
                                           {"body.enable", eCmdHdlrBinary, 0},
                                           {"template", eCmdHdlrGetWord, 0},
                                           {"mode", eCmdHdlrGetWord, 0},
                                           {"sendmail.binary", eCmdHdlrString, 0}};
static struct cnfparamblk actpblk = {CNFPARAMBLK_VERSION, sizeof(actpdescr) / sizeof(struct cnfparamdescr), actpdescr};


BEGINinitConfVars /* (re)set config variables to default values */
    CODESTARTinitConfVars;
    cs.lstRcpt = NULL;
    cs.pszSrv = NULL;
    cs.pszSrvPort = NULL;
    cs.pszFrom = NULL;
    cs.pszSubject = NULL;
    cs.bEnableBody = 1; /* should a mail body be generated? (set to 0 eg for SMS gateways) */
ENDinitConfVars

/* forward definitions (as few as possible) */
static rsRetVal Send(int sock, const char *msg, size_t len);
static rsRetVal readResponse(wrkrInstanceData_t *pWrkrData, int *piState, int iExpected);
typedef rsRetVal (*mailWriteFunc_t)(void *ctx, const char *msg, size_t len);


/* helpers for handling the recipient lists */

/* destroy a complete recipient list */
static void lstRcptDestruct(toRcpt_t *pRoot) {
    toRcpt_t *pDel;

    while (pRoot != NULL) {
        pDel = pRoot;
        pRoot = pRoot->pNext;
        /* ready to disalloc */
        free(pDel->pszTo);
        free(pDel);
    }
}


/* This function adds a recipient to the specified list.
 * The recipient address storage is handed over -- the caller must NOT delete it.
 */
static rsRetVal addRcpt(toRcpt_t **ppLstRcpt, uchar *newRcpt) {
    DEFiRet;
    toRcpt_t *pNew = NULL;

    CHKmalloc(pNew = calloc(1, sizeof(toRcpt_t)));

    pNew->pszTo = newRcpt;
    pNew->pNext = *ppLstRcpt;
    *ppLstRcpt = pNew;

    DBGPRINTF("ommail::addRcpt adds recipient %s\n", newRcpt);

finalize_it:
    if (iRet != RS_RET_OK) {
        free(pNew);
        free(newRcpt); /* in any case, this is no longer needed */
    }

    RETiRet;
}

/* This function is called when a new recipient email address is to be
 * added. rgerhards, 2008-08-04
 */
static rsRetVal legacyConfAddRcpt(void __attribute__((unused)) * pVal, uchar *pNewVal) {
    return addRcpt(&cs.lstRcpt, pNewVal);
}


/* output the recipient list to the mail server
 * iStatusToCheck < 0 means no checking should happen
 */
static rsRetVal WriteRcpts(wrkrInstanceData_t *pWrkrData, uchar *pszOp, size_t lenOp, int iStatusToCheck) {
    toRcpt_t *pRcpt;
    int iState;
    DEFiRet;

    assert(lenOp != 0);

    for (pRcpt = pWrkrData->pData->lstRcpt; pRcpt != NULL; pRcpt = pRcpt->pNext) {
        DBGPRINTF("Sending '%s: <%s>'\n", pszOp, pRcpt->pszTo);
        CHKiRet(Send(pWrkrData->md.smtp.sock, (char *)pszOp, lenOp));
        CHKiRet(Send(pWrkrData->md.smtp.sock, ":<", sizeof(":<") - 1));
        CHKiRet(Send(pWrkrData->md.smtp.sock, (char *)pRcpt->pszTo, strlen((char *)pRcpt->pszTo)));
        CHKiRet(Send(pWrkrData->md.smtp.sock, ">\r\n", sizeof(">\r\n") - 1));
        if (iStatusToCheck >= 0) CHKiRet(readResponse(pWrkrData, &iState, iStatusToCheck));
    }

finalize_it:
    RETiRet;
}


/* end helpers for handling the recipient lists */

BEGINcreateInstance
    CODESTARTcreateInstance;
    pData->mode = OMMailModeSMTP;
    pData->constSubject = NULL;
    pData->bEnableBody = 1;
ENDcreateInstance


BEGINcreateWrkrInstance
    CODESTARTcreateWrkrInstance;
    pWrkrData->md.smtp.sock = -1;
ENDcreateWrkrInstance


BEGINisCompatibleWithFeature
    CODESTARTisCompatibleWithFeature;
    if (eFeat == sFEATURERepeatedMsgReduction) iRet = RS_RET_OK;
ENDisCompatibleWithFeature


BEGINfreeInstance
    CODESTARTfreeInstance;
    free(pData->tplName);
    free(pData->constSubject);
    free(pData->pszFrom);
    lstRcptDestruct(pData->lstRcpt);
    free(pData->md.smtp.pszSrv);
    free(pData->md.smtp.pszSrvPort);
    free(pData->md.sendmail.szBinary);
ENDfreeInstance


BEGINfreeWrkrInstance
    CODESTARTfreeWrkrInstance;
ENDfreeWrkrInstance


BEGINdbgPrintInstInfo
    CODESTARTdbgPrintInstInfo;
    printf("mail"); /* TODO: extend! */
ENDdbgPrintInstInfo


/* TCP support code, should probably be moved to net.c or some place else... -- rgerhards, 2008-04-04 */

/* "receive" a character from the remote server. A single character
 * is returned. Returns RS_RET_NO_MORE_DATA if the server has closed
 * the connection and RS_RET_IO_ERROR if something goes wrong. This
 * is a blocking read.
 * rgerhards, 2008-04-04
 */
static rsRetVal getRcvChar(wrkrInstanceData_t *pWrkrData, char *pC) {
    DEFiRet;
    ssize_t lenBuf;

    if (pWrkrData->md.smtp.iRcvBuf == pWrkrData->md.smtp.lenRcvBuf) { /* buffer empty? */
        /* yes, we need to read the next server response */
        do {
            lenBuf = recv(pWrkrData->md.smtp.sock, pWrkrData->md.smtp.RcvBuf, sizeof(pWrkrData->md.smtp.RcvBuf), 0);
            if (lenBuf == 0) {
                ABORT_FINALIZE(RS_RET_NO_MORE_DATA);
            } else if (lenBuf < 0) {
                if (errno != EAGAIN) {
                    ABORT_FINALIZE(RS_RET_IO_ERROR);
                }
            } else {
                /* good read */
                pWrkrData->md.smtp.iRcvBuf = 0;
                pWrkrData->md.smtp.lenRcvBuf = lenBuf;
            }

        } while (lenBuf < 1);
    }

    /* when we reach this point, we have a non-empty buffer */
    *pC = pWrkrData->md.smtp.RcvBuf[pWrkrData->md.smtp.iRcvBuf++];

finalize_it:
    RETiRet;
}


/* close the mail server connection
 * rgerhards, 2008-04-08
 */
static rsRetVal serverDisconnect(wrkrInstanceData_t *pWrkrData) {
    DEFiRet;
    assert(pWrkrData != NULL);

    if (pWrkrData->md.smtp.sock != -1) {
        close(pWrkrData->md.smtp.sock);
        pWrkrData->md.smtp.sock = -1;
    }

    RETiRet;
}


/* open a connection to the mail server
 * rgerhards, 2008-04-04
 */
static rsRetVal serverConnect(wrkrInstanceData_t *pWrkrData) {
    struct addrinfo *res = NULL;
    struct addrinfo hints;
    const char *smtpPort;
    const char *smtpSrv;
    char errStr[1024];
    instanceData *pData;
    DEFiRet;

    pData = pWrkrData->pData;

    if (pData->md.smtp.pszSrv == NULL)
        smtpSrv = "127.0.0.1";
    else
        smtpSrv = (char *)pData->md.smtp.pszSrv;

    if (pData->md.smtp.pszSrvPort == NULL)
        smtpPort = "25";
    else
        smtpPort = (char *)pData->md.smtp.pszSrvPort;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; /* TODO: make configurable! */
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(smtpSrv, smtpPort, &hints, &res) != 0) {
        DBGPRINTF("error %d in getaddrinfo\n", errno);
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }

    if ((pWrkrData->md.smtp.sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
        DBGPRINTF("couldn't create send socket, reason %s", rs_strerror_r(errno, errStr, sizeof(errStr)));
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }

    if (connect(pWrkrData->md.smtp.sock, res->ai_addr, res->ai_addrlen) != 0) {
        DBGPRINTF("create tcp connection failed, reason %s", rs_strerror_r(errno, errStr, sizeof(errStr)));
        ABORT_FINALIZE(RS_RET_IO_ERROR);
    }

finalize_it:
    if (res != NULL) freeaddrinfo(res);

    if (iRet != RS_RET_OK) {
        if (pWrkrData->md.smtp.sock != -1) {
            close(pWrkrData->md.smtp.sock);
            pWrkrData->md.smtp.sock = -1;
        }
    }

    RETiRet;
}


/* send text to the server, blocking send */
static rsRetVal Send(const int sock, const char *const __restrict__ msg, const size_t len) {
    DEFiRet;
    size_t offsBuf = 0;
    ssize_t lenSend;

    assert(msg != NULL);

    if (len == 0) /* it's valid, but does not make much sense ;) */
        FINALIZE;

    do {
        lenSend = send(sock, msg + offsBuf, len - offsBuf, 0);
        if (lenSend == -1) {
            if (errno != EAGAIN) {
                DBGPRINTF("message not (smtp/tcp)send, errno %d", errno);
                ABORT_FINALIZE(RS_RET_TCP_SEND_ERROR);
            }
        } else if (lenSend != (ssize_t)(len - offsBuf)) {
            offsBuf += lenSend; /* on to next round... */
        } else {
            FINALIZE;
        }
    } while (1);

finalize_it:
    RETiRet;
}


/* read response line from server
 */
static rsRetVal readResponseLn(wrkrInstanceData_t *pWrkrData,
                               char *pLn,
                               size_t lenLn,
                               size_t *const __restrict__ respLen) {
    DEFiRet;
    size_t i = 0;
    char c;

    assert(pWrkrData != NULL);
    assert(pLn != NULL);

    do {
        CHKiRet(getRcvChar(pWrkrData, &c));
        if (c == '\n') break;
        if (i < (lenLn - 1)) /* if line is too long, we simply discard the rest */
            pLn[i++] = c;
    } while (1);
    DBGPRINTF("smtp server response: %s\n", pLn);
    /* do not remove, this is helpful in troubleshooting SMTP probs! */

finalize_it:
    pLn[i] = '\0';
    *respLen = i;
    RETiRet;
}


/* read numerical response code from server and compare it to requried response code.
 * If they two don't match, return RS_RET_SMTP_ERROR.
 * rgerhards, 2008-04-07
 */
static rsRetVal readResponse(wrkrInstanceData_t *pWrkrData, int *piState, int iExpected) {
    DEFiRet;
    int bCont;
    char buf[128];
    size_t respLen;

    assert(pWrkrData != NULL);
    assert(piState != NULL);

    bCont = 1;
    do {
        CHKiRet(readResponseLn(pWrkrData, buf, sizeof(buf), &respLen));
        if (respLen < 4) /* we treat too-short responses as error */
            ABORT_FINALIZE(RS_RET_SMTP_ERROR);
        if (buf[3] != '-') { /* last or only response line? */
            bCont = 0;
            *piState = buf[0] - '0';
            *piState = *piState * 10 + buf[1] - '0';
            *piState = *piState * 10 + buf[2] - '0';
            if (*piState != iExpected) ABORT_FINALIZE(RS_RET_SMTP_ERROR);
        }
    } while (bCont);

finalize_it:
    RETiRet;
}


/* create a timestamp suitable for use with the Date: SMTP body header
 * rgerhards, 2008-04-08
 */
static void mkSMTPTimestamp(uchar *pszBuf, size_t lenBuf) {
    time_t tCurr;
    struct tm tmCurr;
    static const char szDay[][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char szMonth[][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    datetime.GetTime(&tCurr);
    gmtime_r(&tCurr, &tmCurr);
    snprintf((char *)pszBuf, lenBuf, "Date: %s, %2d %s %4d %02d:%02d:%02d +0000\r\n", szDay[tmCurr.tm_wday],
             tmCurr.tm_mday, szMonth[tmCurr.tm_mon], 1900 + tmCurr.tm_year, tmCurr.tm_hour, tmCurr.tm_min,
             tmCurr.tm_sec);
}

static rsRetVal writeSMTP(void *ctx, const char *msg, size_t len) {
    wrkrInstanceData_t *const pWrkrData = (wrkrInstanceData_t *)ctx;
    return Send(pWrkrData->md.smtp.sock, msg, len);
}


static rsRetVal writePipe(void *ctx, const char *msg, size_t len) {
    DEFiRet;
    const int fd = *(int *)ctx;
    size_t offsBuf = 0;
    ssize_t lenWritten;

    assert(msg != NULL);

    while (offsBuf < len) {
        lenWritten = write(fd, msg + offsBuf, len - offsBuf);
        if (lenWritten <= 0) {
            if (lenWritten == -1 && errno == EINTR) continue;
            /* write() is not expected to return 0 for a non-empty pipe write. */
            ABORT_FINALIZE(RS_RET_ERR_WRITE_PIPE);
        }
        offsBuf += lenWritten;
    }

finalize_it:
    RETiRet;
}


static rsRetVal writeBytes(mailWriteFunc_t writeFunc, void *ctx, const char *msg, size_t len) {
    assert(writeFunc != NULL);
    assert(msg != NULL);
    return writeFunc(ctx, msg, len);
}


static rsRetVal writeStr(mailWriteFunc_t writeFunc, void *ctx, const char *msg) {
    return writeBytes(writeFunc, ctx, msg, strlen(msg));
}


/* output the recipient list in rfc2822 format to a generic sink */
static rsRetVal writeTos(mailWriteFunc_t writeFunc, void *ctx, const instanceData *pData) {
    toRcpt_t *pRcpt;
    int iTos;
    DEFiRet;

    CHKiRet(writeStr(writeFunc, ctx, "To: "));
    for (pRcpt = pData->lstRcpt, iTos = 0; pRcpt != NULL; pRcpt = pRcpt->pNext, iTos++) {
        if (iTos) CHKiRet(writeStr(writeFunc, ctx, ", "));
        CHKiRet(writeStr(writeFunc, ctx, "<"));
        CHKiRet(writeStr(writeFunc, ctx, (char *)pRcpt->pszTo));
        CHKiRet(writeStr(writeFunc, ctx, ">"));
    }
    CHKiRet(writeStr(writeFunc, ctx, "\r\n"));

finalize_it:
    RETiRet;
}


/* send body text to a generic sink. SMTP requires escaping a leading dot inside a line. */
static rsRetVal bodyWrite(mailWriteFunc_t writeFunc, void *ctx, char *msg, size_t len, sbool bEscapeDot) {
    DEFiRet;
    char szBuf[2048];
    size_t iSrc;
    size_t iBuf = 0;
    int bHadCR = 0;
    int bInStartOfLine = 1;

    assert(writeFunc != NULL);
    assert(msg != NULL);

    for (iSrc = 0; iSrc < len; ++iSrc) {
        const size_t needed = (bEscapeDot && bInStartOfLine && msg[iSrc] == '.') ? 2 : 1;
        if (iBuf + needed > sizeof(szBuf)) {
            CHKiRet(writeBytes(writeFunc, ctx, szBuf, iBuf));
            iBuf = 0;
        }
        if (bEscapeDot && bInStartOfLine && msg[iSrc] == '.') szBuf[iBuf++] = '.';
        szBuf[iBuf++] = msg[iSrc];
        switch (msg[iSrc]) {
            case '\r':
                bHadCR = 1;
                break;
            case '\n':
                if (bHadCR) bInStartOfLine = 1;
                bHadCR = 0;
                break;
            default:
                bInStartOfLine = 0;
                bHadCR = 0;
                break;
        }
    }

    if (iBuf > 0) CHKiRet(writeBytes(writeFunc, ctx, szBuf, iBuf));

finalize_it:
    RETiRet;
}


static rsRetVal writeMailMessage(
    const instanceData *pData, mailWriteFunc_t writeFunc, void *ctx, uchar *body, uchar *subject, sbool bEscapeDot) {
    DEFiRet;
    uchar szDateBuf[64];

    mkSMTPTimestamp(szDateBuf, sizeof(szDateBuf));
    CHKiRet(writeStr(writeFunc, ctx, (char *)szDateBuf));

    CHKiRet(writeStr(writeFunc, ctx, "From: <"));
    CHKiRet(writeStr(writeFunc, ctx, (char *)pData->pszFrom));
    CHKiRet(writeStr(writeFunc, ctx, ">\r\n"));

    CHKiRet(writeTos(writeFunc, ctx, pData));

    CHKiRet(writeStr(writeFunc, ctx, "Subject: "));
    CHKiRet(writeStr(writeFunc, ctx, (char *)subject));
    CHKiRet(writeStr(writeFunc, ctx, "\r\n"));

    CHKiRet(writeStr(writeFunc, ctx, "X-Mailer: rsyslog-ommail\r\n"));
    CHKiRet(writeStr(writeFunc, ctx, "\r\n"));

    if (pData->bEnableBody) CHKiRet(bodyWrite(writeFunc, ctx, (char *)body, strlen((char *)body), bEscapeDot));

finalize_it:
    RETiRet;
}


/* send a message via SMTP
 * rgerhards, 2008-04-04
 */
static rsRetVal sendSMTP(wrkrInstanceData_t *pWrkrData, uchar *body, uchar *subject) {
    DEFiRet;
    int iState; /* SMTP state */
    instanceData *pData;

    pData = pWrkrData->pData;

    CHKiRet(serverConnect(pWrkrData));
    CHKiRet(readResponse(pWrkrData, &iState, 220));

    CHKiRet(Send(pWrkrData->md.smtp.sock, "HELO ", 5));
    CHKiRet(Send(pWrkrData->md.smtp.sock, (char *)glbl.GetLocalHostName(), strlen((char *)glbl.GetLocalHostName())));
    CHKiRet(Send(pWrkrData->md.smtp.sock, "\r\n", sizeof("\r\n") - 1));
    CHKiRet(readResponse(pWrkrData, &iState, 250));

    CHKiRet(Send(pWrkrData->md.smtp.sock, "MAIL FROM:<", sizeof("MAIL FROM:<") - 1));
    CHKiRet(Send(pWrkrData->md.smtp.sock, (char *)pData->pszFrom, strlen((char *)pData->pszFrom)));
    CHKiRet(Send(pWrkrData->md.smtp.sock, ">\r\n", sizeof(">\r\n") - 1));
    CHKiRet(readResponse(pWrkrData, &iState, 250));

    CHKiRet(WriteRcpts(pWrkrData, (uchar *)"RCPT TO", sizeof("RCPT TO") - 1, 250));

    CHKiRet(Send(pWrkrData->md.smtp.sock, "DATA\r\n", sizeof("DATA\r\n") - 1));
    CHKiRet(readResponse(pWrkrData, &iState, 354));

    CHKiRet(writeMailMessage(pData, writeSMTP, pWrkrData, body, subject, 1));

    /* end of data, back to envelope transaction */
    CHKiRet(Send(pWrkrData->md.smtp.sock, "\r\n.\r\n", sizeof("\r\n.\r\n") - 1));
    CHKiRet(readResponse(pWrkrData, &iState, 250));

    CHKiRet(Send(pWrkrData->md.smtp.sock, "QUIT\r\n", sizeof("QUIT\r\n") - 1));
    CHKiRet(readResponse(pWrkrData, &iState, 221));

    /* we are finished, a new connection is created for each request, so let's close it now */
    CHKiRet(serverDisconnect(pWrkrData));

finalize_it:
    RETiRet;
}

static int countRcpts(const instanceData *pData) {
    int nRcpt = 0;
    toRcpt_t *pRcpt;

    for (pRcpt = pData->lstRcpt; pRcpt != NULL; pRcpt = pRcpt->pNext) ++nRcpt;
    return nRcpt;
}


static rsRetVal buildSendmailArgv(const instanceData *pData, char ***argv) {
    DEFiRet;
    char **aParams = NULL;
    toRcpt_t *pRcpt;
    int i = 0;

    CHKmalloc(aParams = calloc(countRcpts(pData) + 6, sizeof(char *)));
    aParams[i++] = (char *)pData->md.sendmail.szBinary;
    aParams[i++] = (char *)"-i";
    aParams[i++] = (char *)"-f";
    aParams[i++] = (char *)pData->pszFrom;
    aParams[i++] = (char *)"--";
    for (pRcpt = pData->lstRcpt; pRcpt != NULL; pRcpt = pRcpt->pNext) {
        aParams[i++] = (char *)pRcpt->pszTo;
    }
    aParams[i] = NULL;
    *argv = aParams;

finalize_it:
    if (iRet != RS_RET_OK) free(aParams);
    RETiRet;
}


static void setCloexecIfPossible(int fd) {
    int flags;

    if (fd == -1) return;
    flags = fcntl(fd, F_GETFD);
    if (flags != -1) {
        fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    }
}


static int closeOpenFilesViaProc(const char *procdir, int fdKeep) {
    DIR *dir;
    struct dirent *entry;
    int dirFd;

    dir = opendir(procdir);
    if (dir == NULL) return 1;

    dirFd = dirfd(dir);
    while ((entry = readdir(dir)) != NULL) {
        char *end;
        long fd;

        errno = 0;
        fd = strtol(entry->d_name, &end, 10);
        if (errno == 0 && end != entry->d_name && *end == '\0' && fd > STDERR_FILENO && fd != dirFd && fd != fdKeep) {
            close((int)fd);
        }
    }

    closedir(dir);
    return 0;
}


static void closeUnneededExecFds(int fdKeep) {
    if (closeOpenFilesViaProc("/proc/self/fd", fdKeep) == 0) return;
    if (closeOpenFilesViaProc("/proc/fd", fdKeep) == 0) return;
#if defined(HAVE_CLOSE_RANGE) && defined(CLOSE_RANGE_CLOEXEC)
    close_range(STDERR_FILENO + 1, ~0U, CLOSE_RANGE_CLOEXEC);
#else
    (void)fdKeep;
#endif
}


static void writeExecFailure(int fdExecErr, int err) {
    size_t offs = 0;
    ssize_t nWritten;
    const char *buf = (const char *)&err;

    while (offs < sizeof(err)) {
        nWritten = write(fdExecErr, buf + offs, sizeof(err) - offs);
        if (nWritten <= 0) {
            if (nWritten == -1 && errno == EINTR) continue;
            break;
        }
        offs += nWritten;
    }
}


static __attribute__((noreturn)) void execSendmailBinary(int fdStdin, int fdExecErr, char **argv) {
    int fdDevNull = -1;
    int sigNum;
    struct sigaction sigAct;
    sigset_t sigSet;
    int err;

    if (dup2(fdStdin, STDIN_FILENO) == -1) goto failed;

    fdDevNull = open("/dev/null", O_WRONLY | O_CLOEXEC);
    if (fdDevNull == -1) goto failed;
    if (dup2(fdDevNull, STDOUT_FILENO) == -1) goto failed;
    if (dup2(fdDevNull, STDERR_FILENO) == -1) goto failed;

    closeUnneededExecFds(fdExecErr);

    memset(&sigAct, 0, sizeof(sigAct));
    sigemptyset(&sigAct.sa_mask);
    sigAct.sa_handler = SIG_DFL;
    for (sigNum = 1; sigNum < NSIG; ++sigNum) sigaction(sigNum, &sigAct, NULL);

    sigAct.sa_handler = SIG_IGN;
    sigaction(SIGINT, &sigAct, NULL);
    sigemptyset(&sigSet);
    sigprocmask(SIG_SETMASK, &sigSet, NULL);
    alarm(0);

    execv(argv[0], argv);

failed:
    err = errno;
    writeExecFailure(fdExecErr, err);
    _exit(127);
}


static void sendmailSupervisor(int fdStdin, int fdStatus, char **argv) __attribute__((noreturn));
static void writeSupervisorStatus(int fdStatus, sendmailStatus_t status) {
    size_t offs = 0;
    ssize_t nWritten;
    const char *buf = (const char *)&status;

    while (offs < sizeof(status)) {
        nWritten = write(fdStatus, buf + offs, sizeof(status) - offs);
        if (nWritten <= 0) {
            if (nWritten == -1 && errno == EINTR) continue;
            /* write() is not expected to return 0 for this status pipe. */
            break;
        }
        offs += nWritten;
    }
}


static int readExecErrno(int fd) {
    int err = 0;
    size_t offs = 0;
    ssize_t nRead;
    char *buf = (char *)&err;

    while (offs < sizeof(err)) {
        nRead = read(fd, buf + offs, sizeof(err) - offs);
        if (nRead == -1) {
            if (errno == EINTR) continue;
            return errno;
        }
        if (nRead == 0) return 0;
        offs += nRead;
    }
    return err;
}


static void sendmailSupervisor(int fdStdin, int fdStatus, char **argv) {
    sendmailStatus_t status = {-1, 0};
    int pipeExecErr[2] = {-1, -1};
    pid_t cpid;
    pid_t ret;

    if (pipe(pipeExecErr) == -1) {
        writeSupervisorStatus(fdStatus, status);
        _exit(1);
    }
    setCloexecIfPossible(pipeExecErr[0]);
    setCloexecIfPossible(pipeExecErr[1]);

    cpid = fork();
    if (cpid == -1) {
        writeSupervisorStatus(fdStatus, status);
        _exit(1);
    }

    if (cpid == 0) {
        close(pipeExecErr[0]);
        execSendmailBinary(fdStdin, pipeExecErr[1], argv);
    }

    close(pipeExecErr[1]);
    close(fdStdin);
    status.execErrno = readExecErrno(pipeExecErr[0]);
    close(pipeExecErr[0]);
    do {
        ret = waitpid(cpid, &status.waitStatus, 0);
    } while (ret == -1 && errno == EINTR);

    if (ret == -1) status.waitStatus = -1;
    writeSupervisorStatus(fdStatus, status);
    close(fdStatus);
    _exit(0);
}


static rsRetVal readSendmailStatus(int fd, sendmailStatus_t *status) {
    DEFiRet;
    size_t offs = 0;
    ssize_t nRead;
    char *buf = (char *)status;

    while (offs < sizeof(*status)) {
        nRead = read(fd, buf + offs, sizeof(*status) - offs);
        if (nRead == -1) {
            if (errno == EINTR) continue;
            ABORT_FINALIZE(RS_RET_READ_ERR);
        }
        if (nRead == 0) ABORT_FINALIZE(RS_RET_READ_ERR);
        offs += nRead;
    }

finalize_it:
    RETiRet;
}


static rsRetVal checkSendmailStatus(const instanceData *pData, const sendmailStatus_t *status) {
    DEFiRet;

    if (status->execErrno != 0) {
        char errStr[1024];
        rs_strerror_r(status->execErrno, errStr, sizeof(errStr));
        LogMsg(0, NO_ERRCODE, LOG_WARNING, "ommail: failed to execute sendmail binary '%s': %s",
               pData->md.sendmail.szBinary, errStr);
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    }

    if (status->waitStatus == -1) {
        LogMsg(0, NO_ERRCODE, LOG_WARNING, "ommail: could not obtain status from sendmail binary '%s'",
               pData->md.sendmail.szBinary);
        ABORT_FINALIZE(RS_RET_SYS_ERR);
    }

    if (WIFEXITED(status->waitStatus)) {
        if (WEXITSTATUS(status->waitStatus) != 0) {
            LogMsg(0, NO_ERRCODE, LOG_WARNING, "ommail: sendmail binary '%s' exited with status %d",
                   pData->md.sendmail.szBinary, WEXITSTATUS(status->waitStatus));
            ABORT_FINALIZE(RS_RET_SUSPENDED);
        }
    } else if (WIFSIGNALED(status->waitStatus)) {
        LogMsg(0, NO_ERRCODE, LOG_WARNING, "ommail: sendmail binary '%s' terminated by signal %d",
               pData->md.sendmail.szBinary, WTERMSIG(status->waitStatus));
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    } else {
        LogMsg(0, NO_ERRCODE, LOG_WARNING, "ommail: sendmail binary '%s' ended unexpectedly",
               pData->md.sendmail.szBinary);
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    }

finalize_it:
    RETiRet;
}


static rsRetVal sendSendmail(wrkrInstanceData_t *pWrkrData, uchar *body, uchar *subject) {
    DEFiRet;
    instanceData *pData = pWrkrData->pData;
    int pipeStdin[2] = {-1, -1};
    int pipeStatus[2] = {-1, -1};
    pid_t cpid = -1;
    char **argv = NULL;
    sendmailStatus_t status = {-1, 0};

    if (access((char *)pData->md.sendmail.szBinary, X_OK) != 0) {
        LogError(errno, RS_RET_NO_FILE_ACCESS, "ommail: sendmail.binary '%s' is not executable",
                 pData->md.sendmail.szBinary);
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    }

    CHKiRet(buildSendmailArgv(pData, &argv));

    if (pipe(pipeStdin) == -1) ABORT_FINALIZE(RS_RET_ERR_CREAT_PIPE);
    if (pipe(pipeStatus) == -1) ABORT_FINALIZE(RS_RET_ERR_CREAT_PIPE);
    setCloexecIfPossible(pipeStdin[0]);
    setCloexecIfPossible(pipeStdin[1]);
    setCloexecIfPossible(pipeStatus[0]);
    setCloexecIfPossible(pipeStatus[1]);

    cpid = fork();
    if (cpid == -1) ABORT_FINALIZE(RS_RET_ERR_FORK);

    if (cpid == 0) {
        close(pipeStdin[1]);
        close(pipeStatus[0]);
        sendmailSupervisor(pipeStdin[0], pipeStatus[1], argv);
    }

    close(pipeStdin[0]);
    pipeStdin[0] = -1;
    close(pipeStatus[1]);
    pipeStatus[1] = -1;

    iRet = writeMailMessage(pData, writePipe, &pipeStdin[1], body, subject, 0);
    if (iRet != RS_RET_OK) {
        LogError(errno, RS_RET_ERR_WRITE_PIPE, "ommail: error writing message to sendmail binary '%s'",
                 pData->md.sendmail.szBinary);
        close(pipeStdin[1]);
        pipeStdin[1] = -1;
        readSendmailStatus(pipeStatus[0], &status);
        checkSendmailStatus(pData, &status);
        ABORT_FINALIZE(iRet);
    }

    close(pipeStdin[1]);
    pipeStdin[1] = -1;

    CHKiRet(readSendmailStatus(pipeStatus[0], &status));
    close(pipeStatus[0]);
    pipeStatus[0] = -1;
    CHKiRet(checkSendmailStatus(pData, &status));

finalize_it:
    if (pipeStdin[0] != -1) close(pipeStdin[0]);
    if (pipeStdin[1] != -1) close(pipeStdin[1]);
    if (pipeStatus[0] != -1) close(pipeStatus[0]);
    if (pipeStatus[1] != -1) close(pipeStatus[1]);
    if (cpid != -1) {
        int supervisorStatus;
        while (waitpid(cpid, &supervisorStatus, 0) == -1 && errno == EINTR) {
            ;
        }
    }
    free(argv);
    RETiRet;
}


static rsRetVal checkSendmailBinary(const instanceData *pData) {
    DEFiRet;

    if (access((char *)pData->md.sendmail.szBinary, X_OK) != 0) {
        LogError(errno, RS_RET_NO_FILE_ACCESS, "ommail: sendmail.binary '%s' is not executable",
                 pData->md.sendmail.szBinary);
        ABORT_FINALIZE(RS_RET_SUSPENDED);
    }

finalize_it:
    RETiRet;
}


/* in tryResume we check if we can connect to the server in question. If that is OK,
 * we close the connection without doing any actual SMTP transaction. It will be
 * reopened during the actual send process. This may not be the best way to do it if
 * there is a problem inside the SMTP transaction. However, we can't find that out without
 * actually initiating something, and that would be bad. The logic here helps us
 * correctly recover from an unreachable/down mail server, which is probably the majority
 * of problem cases. For SMTP transaction problems, we will do lots of retries, but if it
 * is a temporary problem, it will be fixed anyhow. So I consider this implementation to
 * be clean enough, especially as I think other approaches have other weaknesses.
 * rgerhards, 2008-04-08
 */
BEGINtryResume
    CODESTARTtryResume;
    if (pWrkrData->pData->mode == OMMailModeSMTP) {
        CHKiRet(serverConnect(pWrkrData));
        CHKiRet(serverDisconnect(pWrkrData)); /* if we fail, we will never reach this line */
    } else {
        CHKiRet(checkSendmailBinary(pWrkrData->pData));
    }
finalize_it:
    if (iRet == RS_RET_IO_ERROR) iRet = RS_RET_SUSPENDED;
ENDtryResume


BEGINdoAction
    uchar *subject;
    const instanceData *const __restrict__ pData = pWrkrData->pData;
    CODESTARTdoAction;
    DBGPRINTF("ommail doAction()\n");

    if (pData->constSubject != NULL)
        subject = pData->constSubject;
    else if (pData->bHaveSubject)
        subject = ppString[1];
    else
        subject = (uchar *)"message from rsyslog";

    if (pData->mode == OMMailModeSMTP)
        iRet = sendSMTP(pWrkrData, ppString[0], subject);
    else
        iRet = sendSendmail(pWrkrData, ppString[0], subject);

    if (iRet != RS_RET_OK) {
        DBGPRINTF("error sending mail, suspending\n");
        iRet = RS_RET_SUSPENDED;
    }
ENDdoAction


static inline void setInstParamDefaults(instanceData *pData) {
    pData->tplName = NULL;
    pData->constSubject = NULL;
}

static rsRetVal setMode(instanceData *pData, struct cnfparamvals *pval) {
    DEFiRet;
    uchar *mode = NULL;

    CHKmalloc(mode = (uchar *)es_str2cstr(pval->val.d.estr, NULL));
    if (!strcmp((char *)mode, "smtp")) {
        pData->mode = OMMailModeSMTP;
    } else if (!strcmp((char *)mode, "sendmail")) {
        pData->mode = OMMailModeSendmail;
    } else {
        parser_errmsg("ommail: invalid mode '%s'; expected 'smtp' or 'sendmail'", mode);
        ABORT_FINALIZE(RS_RET_VALUE_NOT_SUPPORTED);
    }

finalize_it:
    free(mode);
    RETiRet;
}


BEGINnewActInst
    struct cnfparamvals *pvals;
    uchar *tplSubject = NULL;
    uchar *rcpt = NULL;
    int i, j;
    CODESTARTnewActInst;
    if ((pvals = nvlstGetParams(lst, &actpblk, NULL)) == NULL) {
        ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
    }

    CHKiRet(createInstance(&pData));
    setInstParamDefaults(pData);

    for (i = 0; i < actpblk.nParams; ++i) {
        if (!pvals[i].bUsed) continue;
        if (!strcmp(actpblk.descr[i].name, "server")) {
            CHKmalloc(pData->md.smtp.pszSrv = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(actpblk.descr[i].name, "port")) {
            CHKmalloc(pData->md.smtp.pszSrvPort = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(actpblk.descr[i].name, "mailfrom")) {
            CHKmalloc(pData->pszFrom = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(actpblk.descr[i].name, "mailto")) {
            for (j = 0; j < pvals[i].val.d.ar->nmemb; ++j) {
                CHKmalloc(rcpt = (uchar *)es_str2cstr(pvals[i].val.d.ar->arr[j], NULL));
                iRet = addRcpt(&(pData->lstRcpt), rcpt);
                rcpt = NULL;
                CHKiRet(iRet);
            }
        } else if (!strcmp(actpblk.descr[i].name, "subject.template")) {
            if (pData->constSubject != NULL) {
                parser_errmsg(
                    "ommail: only one of subject.template, subject.text "
                    "can be set");
                ABORT_FINALIZE(RS_RET_DUP_PARAM);
            }
            CHKmalloc(tplSubject = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(actpblk.descr[i].name, "subject.text")) {
            if (tplSubject != NULL) {
                parser_errmsg(
                    "ommail: only one of subject.template, subject.text "
                    "can be set");
                ABORT_FINALIZE(RS_RET_DUP_PARAM);
            }
            CHKmalloc(pData->constSubject = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(actpblk.descr[i].name, "body.enable")) {
            pData->bEnableBody = (int)pvals[i].val.d.n;
        } else if (!strcmp(actpblk.descr[i].name, "template")) {
            CHKmalloc(pData->tplName = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else if (!strcmp(actpblk.descr[i].name, "mode")) {
            CHKiRet(setMode(pData, &pvals[i]));
        } else if (!strcmp(actpblk.descr[i].name, "sendmail.binary")) {
            CHKmalloc(pData->md.sendmail.szBinary = (uchar *)es_str2cstr(pvals[i].val.d.estr, NULL));
        } else {
            DBGPRINTF(
                "ommail: program error, non-handled "
                "param '%s'\n",
                actpblk.descr[i].name);
        }
    }

    if (pData->mode == OMMailModeSMTP) {
        if (pData->md.smtp.pszSrv == NULL) {
            parser_errmsg("ommail: parameter 'server' required for mode='smtp'");
            ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
        }
        if (pData->md.smtp.pszSrvPort == NULL) {
            parser_errmsg("ommail: parameter 'port' required for mode='smtp'");
            ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
        }
    } else {
        if (pData->md.sendmail.szBinary == NULL) {
            CHKmalloc(pData->md.sendmail.szBinary = (uchar *)strdup(DEFAULT_SENDMAIL_BINARY));
        }
        CHKiRet(checkSendmailBinary(pData));
    }

    if (tplSubject == NULL) {
        /* if no subject is configured, we need just one template string */
        CODE_STD_STRING_REQUESTparseSelectorAct(1)
    } else {
        CODE_STD_STRING_REQUESTparseSelectorAct(2) pData->bHaveSubject = 1;
        /* NOTE: tplSubject memory is *handed over* down here below - do NOT free() */
        CHKiRet(OMSRsetEntry(*ppOMSR, 1, tplSubject, OMSR_NO_RQD_TPL_OPTS));
    }

    if (pData->tplName == NULL) {
        CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar *)strdup("RSYSLOG_FileFormat"), OMSR_NO_RQD_TPL_OPTS));
    } else {
        CHKiRet(OMSRsetEntry(*ppOMSR, 0, (uchar *)strdup((char *)pData->tplName), OMSR_NO_RQD_TPL_OPTS));
    }
    CODE_STD_FINALIZERnewActInst;
    free(rcpt);
    cnfparamvalsDestruct(pvals, &actpblk);
ENDnewActInst


BEGINparseSelectorAct
    CODESTARTparseSelectorAct;
    if (!strncmp((char *)p, ":ommail:", sizeof(":ommail:") - 1)) {
        p += sizeof(":ommail:") - 1; /* eat indicator sequence (-1 because of '\0'!) */
    } else {
        ABORT_FINALIZE(RS_RET_CONFLINE_UNPROCESSED);
    }

    /* ok, if we reach this point, we have something for us */
    if ((iRet = createInstance(&pData)) != RS_RET_OK) FINALIZE;

    /* TODO: check strdup() result */

    if (cs.pszFrom == NULL) {
        LogError(0, RS_RET_MAIL_NO_FROM, "no sender address given - specify $ActionMailFrom");
        ABORT_FINALIZE(RS_RET_MAIL_NO_FROM);
    }
    if (cs.lstRcpt == NULL) {
        LogError(0, RS_RET_MAIL_NO_TO, "no recipient address given - specify $ActionMailTo");
        ABORT_FINALIZE(RS_RET_MAIL_NO_TO);
    }

    CHKmalloc(pData->pszFrom = (uchar *)strdup((char *)cs.pszFrom));
    pData->lstRcpt = cs.lstRcpt; /* we "hand over" this memory */
    cs.lstRcpt = NULL; /* note: this is different from pre-3.21.2 versions! */

    if (cs.pszSubject == NULL) {
        /* if no subject is configured, we need just one template string */
        CODE_STD_STRING_REQUESTparseSelectorAct(1)
    } else {
        CODE_STD_STRING_REQUESTparseSelectorAct(2) pData->bHaveSubject = 1;
        CHKiRet(OMSRsetEntry(*ppOMSR, 1, (uchar *)strdup((char *)cs.pszSubject), OMSR_NO_RQD_TPL_OPTS));
    }
    if (cs.pszSrv != NULL) CHKmalloc(pData->md.smtp.pszSrv = (uchar *)strdup((char *)cs.pszSrv));
    if (cs.pszSrvPort != NULL) CHKmalloc(pData->md.smtp.pszSrvPort = (uchar *)strdup((char *)cs.pszSrvPort));
    pData->bEnableBody = cs.bEnableBody;

    /* process template */
    iRet = cflineParseTemplateName(&p, *ppOMSR, 0, OMSR_NO_RQD_TPL_OPTS, (uchar *)"RSYSLOG_FileFormat");
    CODE_STD_FINALIZERparseSelectorAct
ENDparseSelectorAct


/* Free string config variables and reset them to NULL (not necessarily the default!) */
static rsRetVal freeConfigVariables(void) {
    DEFiRet;

    free(cs.pszSrv);
    cs.pszSrv = NULL;
    free(cs.pszSrvPort);
    cs.pszSrvPort = NULL;
    free(cs.pszFrom);
    cs.pszFrom = NULL;
    lstRcptDestruct(cs.lstRcpt);
    cs.lstRcpt = NULL;

    RETiRet;
}


BEGINmodExit
    CODESTARTmodExit;
    /* cleanup our allocations */
    freeConfigVariables();

    /* release what we no longer need */
    objRelease(datetime, CORE_COMPONENT);
    objRelease(glbl, CORE_COMPONENT);
ENDmodExit


BEGINqueryEtryPt
    CODESTARTqueryEtryPt;
    CODEqueryEtryPt_STD_OMOD_QUERIES;
    CODEqueryEtryPt_STD_CONF2_OMOD_QUERIES;
    CODEqueryEtryPt_STD_OMOD8_QUERIES;
    CODEqueryEtryPt_STD_CONF2_CNFNAME_QUERIES;
ENDqueryEtryPt


/* Reset config variables for this module to default values.
 */
static rsRetVal resetConfigVariables(uchar __attribute__((unused)) * pp, void __attribute__((unused)) * pVal) {
    DEFiRet;
    cs.bEnableBody = 1;
    iRet = freeConfigVariables();
    RETiRet;
}


BEGINmodInit()
    CODESTARTmodInit;
    INITLegCnfVars;
    *ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
    CODEmodInit_QueryRegCFSLineHdlr
        /* tell which objects we need */
        CHKiRet(objUse(glbl, CORE_COMPONENT));
    CHKiRet(objUse(datetime, CORE_COMPONENT));

    DBGPRINTF("ommail version %s initializing\n", VERSION);

    CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionmailsmtpserver", 0, eCmdHdlrGetWord, NULL, &cs.pszSrv,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionmailsmtpport", 0, eCmdHdlrGetWord, NULL, &cs.pszSrvPort,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(
        omsdRegCFSLineHdlr((uchar *)"actionmailfrom", 0, eCmdHdlrGetWord, NULL, &cs.pszFrom, STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionmailto", 0, eCmdHdlrGetWord, legacyConfAddRcpt, NULL,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionmailsubject", 0, eCmdHdlrGetWord, NULL, &cs.pszSubject,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"actionmailenablebody", 0, eCmdHdlrBinary, NULL, &cs.bEnableBody,
                               STD_LOADABLE_MODULE_ID));
    CHKiRet(omsdRegCFSLineHdlr((uchar *)"resetconfigvariables", 1, eCmdHdlrCustomHandler, resetConfigVariables, NULL,
                               STD_LOADABLE_MODULE_ID));
ENDmodInit
