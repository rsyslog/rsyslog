/* The command handler for the "rsp" command (done at the client).
 *
 * Copyright 2008 by Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of librelp.
 *
 * Librelp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Librelp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Librelp.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 *
 * If the terms of the GPL are unsuitable for your needs, you may obtain
 * a commercial license from Adiscon. Contact sales@adiscon.com for further
 * details.
 *
 * ALL CONTRIBUTORS PLEASE NOTE that by sending contributions, you assign
 * your copyright to Adiscon GmbH, Germany. This is necessary to permit the
 * dual-licensing set forth here. Our apologies for this inconvenience, but
 * we sincerely believe that the dual-licensing model helps us provide great
 * free software while at the same time obtaining some funding for further
 * development.
 */
#include "config.h"
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <sys/types.h>
#include "librelp.h"
#include "relp.h"
#include "cmdif.h"


/* read the response header. The frame's pData read pointer is moved to the
 * first byte of the response data buffer (if any).
 * rgerhards, 2008-03-24
 */
static relpRetVal
readRspHdr(relpFrame_t *pFrame, int *pCode, unsigned char *pText)
{
	int iText;
	unsigned char c;
	relpRetVal localRet;

	ENTER_RELPFUNC;

	/* code - it must be exactly three digits */
	CHKRet(relpFrameGetNextC(pFrame, &c));
	if(isdigit(c)) *pCode = c - '0'; else ABORT_FINALIZE(RELP_RET_INVALID_RSPHDR);
	CHKRet(relpFrameGetNextC(pFrame, &c));
	if(isdigit(c)) *pCode = *pCode * 10 + c - '0'; else ABORT_FINALIZE(RELP_RET_INVALID_RSPHDR);
	CHKRet(relpFrameGetNextC(pFrame, &c));
	if(isdigit(c)) *pCode = *pCode * 10 + c - '0'; else ABORT_FINALIZE(RELP_RET_INVALID_RSPHDR);

	CHKRet(relpFrameGetNextC(pFrame, &c));
	if(c != ' ') ABORT_FINALIZE(RELP_RET_INVALID_RSPHDR);

	/* now come the message. It is terminated after 80 chars or when an LF
	 * is found (the LF is the delemiter to response data) or there is no
	 * more data.
	 */
	for(iText = 0 ; iText < 80 ; ++iText) {
		localRet = relpFrameGetNextC(pFrame, &c);
		if(localRet == RELP_RET_END_OF_DATA)
			break;
		else if(localRet != RELP_RET_OK)
			ABORT_FINALIZE(localRet);
		if(c == '\n')
			break;
		/* we got a regular char which we need to copy */
		pText[iText] = c;
	}
	pText[iText] = '\0'; /* make it a C-string */

finalize_it:
	LEAVE_RELPFUNC;
}


/* process a response
 * rgerhards, 2008-03-17
 */
BEGINcommand(S, Rsp)
	relpSendbuf_t *pSendbuf;
	unsigned char bufText[81]; /* max 80 chars as of RELP spec! */
	int rspCode;

	ENTER_RELPFUNC;
	CHKRet(readRspHdr(pFrame, &rspCode, bufText));
	pSess->pEngine->dbgprint("in rsp command handler, txnr %d, code %d, text '%s'\n",
				 pFrame->txnr, rspCode, bufText);
	CHKRet(relpSessGetUnacked(pSess, &pSendbuf, pFrame->txnr));

	if(rspCode == 200) {
		if(pSendbuf->rspHdlr != NULL) {
			CHKRet(pSendbuf->rspHdlr(pSess, pFrame));
		}
		CHKRet(relpSendbufDestruct(&pSendbuf));
	} else {
		iRet = RELP_RET_RSP_STATE_ERR;
		/* TODO: add a hock to a logger function of the caller */
		relpSendbufDestruct(&pSendbuf); /* don't set error code */
	}

finalize_it:
ENDcommand
