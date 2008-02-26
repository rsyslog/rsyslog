/* imgssapi.c
 * This is the implementation of the GSSAPI input module.
 *
 * IMPORTANT: Currently this module does not really exist. It shares code
 * from imtcp, which has everything (controlled via preprocessor
 * defines). This file has been created in order to facilitate moving
 * gssapi into its real own module.
 *
 * NOTE: read comments in module-template.h to understand how this file
 *       works!
 *
 * Copyright 2007 Rainer Gerhards and Adiscon GmbH.
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
#include <gssapi/gssapi.h>
#include "rsyslog.h"
#include "syslogd.h"
#include "cfsysline.h"
#include "module-template.h"
#include "net.h"
#include "srUtils.h"
#include "gss-misc.h"

/* some forward definitions - they may go away when we no longer include imtcp.c */
static rsRetVal addGSSListener(void __attribute__((unused)) *pVal, uchar *pNewVal);
int TCPSessGSSAccept(int fd);
int TCPSessGSSInit(void);
void TCPSessGSSClose(int iSess);
void TCPSessGSSDeinit(void);
int TCPSessGSSRecv(int iSess, void *buf, size_t buf_len);

/* static data */
static gss_cred_id_t gss_server_creds = GSS_C_NO_CREDENTIAL;

/* config variables */
static char *gss_listen_service_name = NULL;
static int bPermitPlainTcp = 0; /* plain tcp syslog allowed on GSSAPI port? */

/* ################################################################################ *
 * #                      THE FOLLOWING LINE IS VITALLY IMPORTANT                 # *
 * #------------------------------------------------------------------------------# *
 * # It includes imtcp.c, which has many common code with this module. Over time, # *
 * # we will move this into separate clases, but for the time being it permits    # *
 * # use to use a somewhat clean build process and keep the files focussed.       # *
 * ################################################################################ */

#include "../imtcp/imtcp.c"

/* ################################################################################ *
 * #                                END IMPORTANT PART                            # *
 * ################################################################################ */

static rsRetVal addGSSListener(void __attribute__((unused)) *pVal, uchar *pNewVal)
{
	if (!bEnableTCP)
		configureTCPListen((char *) pNewVal);
	bEnableTCP |= ALLOWEDMETHOD_GSS;

	return RS_RET_OK;
}


/* returns 0 if all went OK, -1 if it failed */
int TCPSessGSSInit(void)
{
	gss_buffer_desc name_buf;
	gss_name_t server_name;
	OM_uint32 maj_stat, min_stat;

	if (gss_server_creds != GSS_C_NO_CREDENTIAL)
		return 0;

	name_buf.value = (gss_listen_service_name == NULL) ? "host" : gss_listen_service_name;
	name_buf.length = strlen(name_buf.value) + 1;
	maj_stat = gss_import_name(&min_stat, &name_buf, GSS_C_NT_HOSTBASED_SERVICE, &server_name);
	if (maj_stat != GSS_S_COMPLETE) {
		display_status("importing name", maj_stat, min_stat);
		return -1;
	}

	maj_stat = gss_acquire_cred(&min_stat, server_name, 0,
				    GSS_C_NULL_OID_SET, GSS_C_ACCEPT,
				    &gss_server_creds, NULL, NULL);
	if (maj_stat != GSS_S_COMPLETE) {
		display_status("acquiring credentials", maj_stat, min_stat);
		return -1;
	}

	gss_release_name(&min_stat, &server_name);
	dbgprintf("GSS-API initialized\n");
	return 0;
}


/* returns 0 if all went OK, -1 if it failed */
int TCPSessGSSAccept(int fd)
{
	gss_buffer_desc send_tok, recv_tok;
	gss_name_t client;
	OM_uint32 maj_stat, min_stat, acc_sec_min_stat;
	int iSess;
	gss_ctx_id_t *context;
	OM_uint32 *sess_flags;
	int fdSess;
	char allowedMethods;

	if ((iSess = TCPSessAccept(fd)) == -1)
		return -1;

	allowedMethods = pTCPSessions[iSess].allowedMethods;
	if (allowedMethods & ALLOWEDMETHOD_GSS) {
		/* Buffer to store raw message in case that
		 * gss authentication fails halfway through.
		 */
		char buf[MAXLINE];
		int ret = 0;

		dbgprintf("GSS-API Trying to accept TCP session %d\n", iSess);

		fdSess = pTCPSessions[iSess].sock;
		if (allowedMethods & ALLOWEDMETHOD_TCP) {
			int len;
			fd_set  fds;
			struct timeval tv;
		
			do {
				FD_ZERO(&fds);
				FD_SET(fdSess, &fds);
				tv.tv_sec = 1;
				tv.tv_usec = 0;
				ret = select(fdSess + 1, &fds, NULL, NULL, &tv);
			} while (ret < 0 && errno == EINTR);
			if (ret < 0) {
				logerrorInt("TCP session %d will be closed, error ignored\n", iSess);
				TCPSessClose(iSess);
				return -1;
			} else if (ret == 0) {
				dbgprintf("GSS-API Reverting to plain TCP\n");
				pTCPSessions[iSess].allowedMethods = ALLOWEDMETHOD_TCP;
				return 0;
			}

			do {
				ret = recv(fdSess, buf, sizeof (buf), MSG_PEEK);
			} while (ret < 0 && errno == EINTR);
			if (ret <= 0) {
				if (ret == 0)
					dbgprintf("GSS-API Connection closed by peer\n");
				else
					logerrorInt("TCP session %d will be closed, error ignored\n", iSess);
				TCPSessClose(iSess);
				return -1;
			}

			if (ret < 4) {
				dbgprintf("GSS-API Reverting to plain TCP\n");
				pTCPSessions[iSess].allowedMethods = ALLOWEDMETHOD_TCP;
				return 0;
			} else if (ret == 4) {
				/* The client might has been interupted after sending
				 * the data length (4B), give him another chance.
				 */
				sleep(1);
				do {
					ret = recv(fdSess, buf, sizeof (buf), MSG_PEEK);
				} while (ret < 0 && errno == EINTR);
				if (ret <= 0) {
					if (ret == 0)
						dbgprintf("GSS-API Connection closed by peer\n");
					else
						logerrorInt("TCP session %d will be closed, error ignored\n", iSess);
					TCPSessClose(iSess);
					return -1;
				}
			}

			len = ntohl((buf[0] << 24)
				    | (buf[1] << 16)
				    | (buf[2] << 8)
				    | buf[3]);
			if ((ret - 4) < len || len == 0) {
				dbgprintf("GSS-API Reverting to plain TCP\n");
				pTCPSessions[iSess].allowedMethods = ALLOWEDMETHOD_TCP;
				return 0;
			}
		}

		context = &pTCPSessions[iSess].gss_context;
		*context = GSS_C_NO_CONTEXT;
		sess_flags = &pTCPSessions[iSess].gss_flags;
		do {
			if (recv_token(fdSess, &recv_tok) <= 0) {
				logerrorInt("TCP session %d will be closed, error ignored\n", iSess);
				TCPSessClose(iSess);
				return -1;
			}
			maj_stat = gss_accept_sec_context(&acc_sec_min_stat, context, gss_server_creds,
							  &recv_tok, GSS_C_NO_CHANNEL_BINDINGS, &client,
							  NULL, &send_tok, sess_flags, NULL, NULL);
			if (recv_tok.value) {
				free(recv_tok.value);
				recv_tok.value = NULL;
			}
			if (maj_stat != GSS_S_COMPLETE
			    && maj_stat != GSS_S_CONTINUE_NEEDED) {
				gss_release_buffer(&min_stat, &send_tok);
				if (*context != GSS_C_NO_CONTEXT)
					gss_delete_sec_context(&min_stat, context, GSS_C_NO_BUFFER);
				if ((allowedMethods & ALLOWEDMETHOD_TCP) && 
				    (GSS_ROUTINE_ERROR(maj_stat) == GSS_S_DEFECTIVE_TOKEN)) {
					dbgprintf("GSS-API Reverting to plain TCP\n");
					dbgprintf("tcp session socket with new data: #%d\n", fdSess);
					if(TCPSessDataRcvd(iSess, buf, ret) == 0) {
						logerrorInt("Tearing down TCP Session %d - see "
							    "previous messages for reason(s)\n",
							    iSess);
						TCPSessClose(iSess);
						return -1;
					}
					pTCPSessions[iSess].allowedMethods = ALLOWEDMETHOD_TCP;
					return 0;
				}
				display_status("accepting context", maj_stat,
					       acc_sec_min_stat);
				TCPSessClose(iSess);
				return -1;
			}
			if (send_tok.length != 0) {
				if (send_token(fdSess, &send_tok) < 0) {
					gss_release_buffer(&min_stat, &send_tok);
					logerrorInt("TCP session %d will be closed, error ignored\n", iSess);
					if (*context != GSS_C_NO_CONTEXT)
						gss_delete_sec_context(&min_stat, context, GSS_C_NO_BUFFER);
					TCPSessClose(iSess);
					return -1;
				}
				gss_release_buffer(&min_stat, &send_tok);
			}
		} while (maj_stat == GSS_S_CONTINUE_NEEDED);

		maj_stat = gss_display_name(&min_stat, client, &recv_tok, NULL);
		if (maj_stat != GSS_S_COMPLETE)
			display_status("displaying name", maj_stat, min_stat);
		else
			dbgprintf("GSS-API Accepted connection from: %s\n", (char*) recv_tok.value);
		gss_release_name(&min_stat, &client);
		gss_release_buffer(&min_stat, &recv_tok);

		dbgprintf("GSS-API Provided context flags:\n");
		display_ctx_flags(*sess_flags);
		pTCPSessions[iSess].allowedMethods = ALLOWEDMETHOD_GSS;
	}

	return 0;
}


/* returns: ? */
int TCPSessGSSRecv(int iSess, void *buf, size_t buf_len)
{
	gss_buffer_desc xmit_buf, msg_buf;
	gss_ctx_id_t *context;
	OM_uint32 maj_stat, min_stat;
	int fdSess;
	int     conf_state;
	int state, len;

	fdSess = pTCPSessions[iSess].sock;
	if ((state = recv_token(fdSess, &xmit_buf)) <= 0)
		return state;

	context = &pTCPSessions[iSess].gss_context;
	maj_stat = gss_unwrap(&min_stat, *context, &xmit_buf, &msg_buf,
			      &conf_state, (gss_qop_t *) NULL);
	if (maj_stat != GSS_S_COMPLETE) {
		display_status("unsealing message", maj_stat, min_stat);
		if (xmit_buf.value) {
			free(xmit_buf.value);
			xmit_buf.value = 0;
		}
		return (-1);
	}
	if (xmit_buf.value) {
		free(xmit_buf.value);
		xmit_buf.value = 0;
	}

	len = msg_buf.length < buf_len ? msg_buf.length : buf_len;
	memcpy(buf, msg_buf.value, len);
	gss_release_buffer(&min_stat, &msg_buf);

	return len;
}


void TCPSessGSSClose(int iSess) {
	OM_uint32 maj_stat, min_stat;
	gss_ctx_id_t *context;

	if(iSess < 0 || iSess > iTCPSessMax) {
		errno = 0;
		logerror("internal error, trying to close an invalid TCP session!");
		return;
	}

	context = &pTCPSessions[iSess].gss_context;
	maj_stat = gss_delete_sec_context(&min_stat, context, GSS_C_NO_BUFFER);
	if (maj_stat != GSS_S_COMPLETE)
		display_status("deleting context", maj_stat, min_stat);
	*context = GSS_C_NO_CONTEXT;
	pTCPSessions[iSess].gss_flags = 0;
	pTCPSessions[iSess].allowedMethods = 0;

	TCPSessClose(iSess);
}


void TCPSessGSSDeinit(void) {
	OM_uint32 maj_stat, min_stat;

	maj_stat = gss_release_cred(&min_stat, &gss_server_creds);
	if (maj_stat != GSS_S_COMPLETE)
		display_status("releasing credentials", maj_stat, min_stat);
}

