/**
 * The dtls input module, uses OpenSSL as library to implement DTLS.
 *
 * \author  Andre Lorbach <alorbach@adiscon.com>
 *
 * Copyright (C) 2023 Adiscon GmbH.
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

#include <signal.h>
#include <stdio.h>
#include <signal.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/errno.h>
#include <poll.h>
#include <assert.h>
#include <time.h>

// --- Include openssl headers as well
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(LIBRESSL_VERSION_NUMBER)
#	include <openssl/bioerr.h>
#endif
#ifndef OPENSSL_NO_ENGINE
#       include <openssl/engine.h>
#endif
// ---

#include "rsyslog.h"
#include "dirty.h"
#include "module-template.h"
#include "cfsysline.h"
#include "msg.h"
#include "errmsg.h"
#include "glbl.h"
#include "srUtils.h"
#include "msg.h"
#include "parser.h"
#include "datetime.h"
#include "net.h"
#include "net_ossl.h"
#include "prop.h"
#include "ruleset.h"
#include "statsobj.h"
#include "unicode-helper.h"

MODULE_TYPE_INPUT
MODULE_TYPE_NOKEEP
MODULE_CNFNAME("imdtls")

/* defines */
#define MAX_DTLS_CLIENTS 1024
#define MAX_DTLS_MSGSIZE 65536
#define DTLS_LISTEN_PORT "4433"
// 1800 seconds = 30 minutes
#define DTLS_DEFAULT_TIMEOUT 1800

/* Module static data */
DEF_OMOD_STATIC_DATA
DEFobjCurrIf(glbl)
DEFobjCurrIf(datetime)
DEFobjCurrIf(prop)
DEFobjCurrIf(ruleset)
DEFobjCurrIf(statsobj)
DEFobjCurrIf(net)
DEFobjCurrIf(net_ossl)

#define DTLS_MAX_RCVBUF 8192	/* Maximum DTLS packet is 8192 bytes which fits into Jumbo Frames. If not enabled
				*  message fragmentation (Ethernet MTU of ~ 1500 bytes) can occur.*/

/* config settings */
typedef struct configSettings_s {
	uchar *pszBindRuleset;		/* name of Ruleset to bind to */
} configSettings_t;
static configSettings_t cs;

struct dtlsClient_s {
	SSL* sslClient;			/* DTSL (SSL) Client */
	time_t lastActivityTime;	/* Last Activity Time */
	int clientfd;			/* ClientFD */
};

// Use typedef to define a type 'dtlsClient_t' based on 'struct dtlsClient_s'
typedef struct dtlsClient_s dtlsClient_t;

struct instanceConf_s {
	/* Network properties */
	uchar *pszBindAddr;		/* Listening IP Address */
	uchar *pszBindPort;		/* Port to bind socket to */
	int timeout;			/* Default timeout for DTLS Sessions */
	/* Common properties */
	uchar *pszBindRuleset;		/* name of ruleset to bind to */
	uchar *pszInputName;
	prop_t *pInputName;		/* InputName in property format for fast access */
	ruleset_t *pBindRuleset;	/* ruleset to bind listener to (use system default if unspecified) */
	sbool bEnableLstn;		/* flag to permit disabling of listener in error case */
	statsobj_t *stats;		/* listener stats */
	STATSCOUNTER_DEF(ctrSubmit, mutCtrSubmit)
	/* OpenSSL properties */
	uchar *tlscfgcmd;			/* OpenSSL Config Command used to override any OpenSSL Settings */
	permittedPeers_t *pPermPeersRoot;	/* permitted peers */
	int CertVerifyDepth;			/* Verify Depth for certificate chains */
	/* Instance Variables */
	char *pszRcvBuf;
	int lenRcvBuf;
	/**< -1: empty, 0: connection closed, 1..NSD_OSSL_MAX_RCVBUF-1: data of that size present */
	int ptrRcvBuf;			/**< offset for next recv operation if 0 < lenRcvBuf < NSD_OSSL_MAX_RCVBUF */

	/* OpenSSL and Config Cert vars inside net_ossl_t now */
	net_ossl_t *pNetOssl;		/* OSSL shared Config and object vars are here */
	int nClients;			/* */
	dtlsClient_t **dtlsClients;	/* Array of DTSL (SSL) Clients */
	int sockfd;			/* UDP Socket used to bind to */
	struct sockaddr_in server_addr;	/* Server Sockaddr */
	int port;			/* Server Port as integer */

	int id;				/* Thread ID */
	thrdInfo_t *pThrd;		/* Thread Instance Info */
	pthread_t tid;			/* the instances thread ID */

	struct instanceConf_s *next;
	struct instanceConf_s *prev;
};

/* config variables */
struct modConfData_s {
	rsconf_t *pConf;		/* our overall config object */
	instanceConf_t *root, *tail;
	uchar *pszBindRuleset;		/* default name of Ruleset to bind to */
	AuthMode drvrAuthMode;		/* authenticate peer if no other name given */
};

static modConfData_t *loadModConf = NULL;/* modConf ptr to use for the current load process */
static modConfData_t *runModConf = NULL;/* modConf ptr to use for the current load process */

static prop_t *pInputName = NULL;

/* module-global parameters */
static struct cnfparamdescr modpdescr[] = {
	{ "ruleset", eCmdHdlrGetWord, 0 },
	{ "tls.authmode", eCmdHdlrString, 0 },
};
static struct cnfparamblk modpblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(modpdescr)/sizeof(struct cnfparamdescr),
	  modpdescr
	};

/* input instance parameters */
static struct cnfparamdescr inppdescr[] = {
	{ "port", eCmdHdlrString, CNFPARAM_REQUIRED },
	{ "address", eCmdHdlrString, 0 },
	{ "timeout", eCmdHdlrPositiveInt, 0 },
	{ "name", eCmdHdlrString, 0 },
	{ "ruleset", eCmdHdlrString, 0 },
	{ "tls.permittedpeer", eCmdHdlrArray, 0 },
	{ "tls.authmode", eCmdHdlrString, 0 },
	{ "tls.cacert", eCmdHdlrString, 0 },
	{ "tls.mycert", eCmdHdlrString, 0 },
	{ "tls.myprivkey", eCmdHdlrString, 0 },
	{ "tls.tlscfgcmd", eCmdHdlrString, 0 }
};
static struct cnfparamblk inppblk =
	{ CNFPARAMBLK_VERSION,
	  sizeof(inppdescr)/sizeof(struct cnfparamdescr),
	  inppdescr
	};
#include "im-helper.h" /* must be included AFTER the type definitions! */

/* create input instance, set default parameters, and
 * add it to the list of instances.
 */
static rsRetVal
createInstance(instanceConf_t **pinst)
{
	instanceConf_t *inst;
	DEFiRet;
	CHKmalloc(inst = malloc(sizeof(instanceConf_t)));
	inst->next = NULL;

	inst->pszBindAddr = NULL;
	inst->pszBindPort = NULL;
	inst->timeout = 1800;
	inst->pszBindRuleset = loadModConf->pszBindRuleset;
	inst->pszInputName = NULL;
	inst->pBindRuleset = NULL;
	inst->bEnableLstn = 0;

	inst->tlscfgcmd = NULL;
	inst->pPermPeersRoot = NULL;
	inst->CertVerifyDepth = 2;

	/* node created, let's add to config */
	if(loadModConf->tail == NULL) {
		loadModConf->tail = loadModConf->root = inst;
	} else {
		loadModConf->tail->next = inst;
		loadModConf->tail = inst;
	}

	// Construct pNetOssl helper
	CHKiRet(net_ossl.Construct(&inst->pNetOssl));
	inst->pNetOssl->authMode = loadModConf->drvrAuthMode;

	*pinst = inst;
finalize_it:
	RETiRet;
}


/* function to generate an error message if the ruleset cannot be found */
static inline void
std_checkRuleset_genErrMsg(__attribute__((unused)) modConfData_t *modConf, instanceConf_t *inst)
{
	LogError(0, NO_ERRCODE, "imdtls[%s]: ruleset '%s' not found - "
			"using default ruleset instead",
			inst->pszBindPort, inst->pszBindRuleset);
}

static void
DTLSCloseSocket(instanceConf_t *inst) {
	DBGPRINTF("imdtls: DTLSCloseSocket for %s:%d\n", inst->pszBindAddr, inst->port);
	// Close UDP Socket
	close(inst->sockfd);
	inst->sockfd = 0;
}

static rsRetVal
DTLSCreateSocket(instanceConf_t *inst) {
	DEFiRet;
	int optval = 1;
	int flags;

	struct in_addr ip_struct;
	DBGPRINTF("imdtls: DTLSCreateSocket for %s:%d\n", inst->pszBindAddr, inst->port);

	// Create UDP Socket
	inst->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (inst->sockfd < 0) {
		LogError(0, NO_ERRCODE, "imdtls: Unable to create DTLS listener,"
				" failed to create socket, "
				" ignoring port %d bind-address %s.",
				inst->port, inst->pszBindAddr);
		ABORT_FINALIZE(RS_RET_ERR);
	}
	setsockopt(inst->sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	setsockopt(inst->sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

	// Set NON Blcoking Flags
	flags = fcntl(inst->sockfd, F_GETFL, 0);
	fcntl(inst->sockfd, F_SETFL, flags | O_NONBLOCK);

	// Convert IP Address into numeric
	if (inet_pton(AF_INET, (char*) inst->pszBindAddr, &ip_struct) <= 0) {
		LogError(0, NO_ERRCODE, "imdtls: Unable to create DTLS listener,"
				" invalid Bind Address, "
				" ignoring port %d bind-address %s.",
				inst->port, inst->pszBindAddr);
		ABORT_FINALIZE(RS_RET_ERR);
	}

	// Set Server Address
	memset(&inst->server_addr, 0, sizeof(struct sockaddr_in));
	inst->server_addr.sin_family = AF_INET;
	inst->server_addr.sin_port = htons(inst->port);
	inst->server_addr.sin_addr.s_addr = htonl(ip_struct.s_addr);

	// Bind UDP Socket
	if (bind(inst->sockfd, (struct sockaddr*)&inst->server_addr, sizeof(struct sockaddr_in)) < 0) {
		LogError(0, NO_ERRCODE, "imdtls: Unable to create DTLS listener,"
				" unable to bind, "
				" ignoring port %d bind-address %s.",
				inst->port, inst->pszBindAddr);
		ABORT_FINALIZE(RS_RET_ERR);
	}
finalize_it:
	RETiRet;
}

/* Verify Callback for X509 Certificate validation. Force visibility as this function is not called anywhere but
*  only used as callback!
*/
static int
imdtls_verify_callback(int status, SSL* ssl)
{
	DEFiRet;
	X509 *certpeer;
	instanceConf_t *inst = NULL;

	dbgprintf("imdtls_verify_callback: get SSL [%p]\n", (void *)ssl);
	inst = (instanceConf_t*) SSL_get_ex_data(ssl, 2);

	/* Continue check if certificate verify was valid */
	if (status == 1) {
		if (	ssl != NULL &&
			inst != NULL) {
			/* call the actual function based on current auth mode */
			switch(inst->pNetOssl->authMode) {
				case OSSL_AUTH_CERTNAME:
					/* if we check the name, we must ensure the cert is valid */
					certpeer = net_ossl.osslGetpeercert(inst->pNetOssl, ssl, NULL);
					dbgprintf("imdtls_verify_callback: Check peer certname[%p]=%s\n",
						(void *)ssl, (certpeer != NULL ? "VALID" : "NULL"));
					CHKiRet(net_ossl.osslChkpeercertvalidity(inst->pNetOssl, ssl, NULL));
					CHKiRet(net_ossl.osslChkpeername(inst->pNetOssl, certpeer, NULL));
					break;
				case OSSL_AUTH_CERTFINGERPRINT:
					certpeer = net_ossl.osslGetpeercert(inst->pNetOssl, ssl, NULL);
					dbgprintf("imdtls_verify_callback: Check peer fingerprint[%p]=%s\n",
						(void *)ssl, (certpeer != NULL ? "VALID" : "NULL"));
					CHKiRet(net_ossl.osslChkpeercertvalidity(inst->pNetOssl, ssl, NULL));
					CHKiRet(net_ossl.osslPeerfingerprint(inst->pNetOssl, certpeer, NULL));
					break;
				case OSSL_AUTH_CERTVALID:
					certpeer = net_ossl.osslGetpeercert(inst->pNetOssl, ssl, NULL);
					dbgprintf("imdtls_verify_callback: Check peer valid[%p]=%s\n",
						(void *)ssl, (certpeer != NULL ? "VALID" : "NULL"));
					CHKiRet(net_ossl.osslChkpeercertvalidity(inst->pNetOssl, ssl, NULL));
					break;
				case OSSL_AUTH_CERTANON:
					dbgprintf("imdtls_verify_callback: ANON[%p]\n", (void *)ssl);
					FINALIZE;
					break;
			}
		} else {
			LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING, "imdtls_verify_callback: MISSING ssl or inst!");
		}
	}
finalize_it:
	if(iRet != RS_RET_OK) {
		DBGPRINTF("imdtls_verify_callback: FAILED\n");
		status = 0;
	}
	return status;
}


static rsRetVal
addListner(modConfData_t __attribute__((unused)) *modConf, instanceConf_t *inst)
{
	uchar statname[64];
	DEFiRet;
	DBGPRINTF("imdtls: addListner ENTER\n");

	if(!inst->bEnableLstn) {
		DBGPRINTF("imdtls: DTLS Listener not started because it is disabled by config error\n");
		FINALIZE;
	}

	inst->pszInputName = ustrdup((inst->pszInputName == NULL) ?  UCHAR_CONSTANT("imdtls") : inst->pszInputName);
	CHKiRet(prop.Construct(&inst->pInputName));
	CHKiRet(prop.SetString(inst->pInputName, inst->pszInputName, ustrlen(inst->pszInputName)));
	CHKiRet(prop.ConstructFinalize(inst->pInputName));

	/* Init defaults */
	if (inst->pszBindPort == NULL) {
		CHKmalloc(inst->pszBindPort = ustrdup((uchar*) DTLS_LISTEN_PORT));
	}

	/* Init SSL Handles! */
	CHKmalloc(inst->dtlsClients = (dtlsClient_t**) calloc(MAX_DTLS_CLIENTS, sizeof(dtlsClient_t*)));
	for (int i = 0; i < MAX_DTLS_CLIENTS; ++i) {
		CHKmalloc(inst->dtlsClients[i] = (dtlsClient_t*) calloc(1, sizeof(dtlsClient_t)));
	}
	inst->nClients = 0;

	/* support statistics gathering */
	CHKiRet(statsobj.Construct(&(inst->stats)));
	snprintf((char*)statname, sizeof(statname), "%s(%s)",
		 inst->pszInputName, inst->pszBindPort);
	statname[sizeof(statname)-1] = '\0'; /* just to be on the save side... */
	CHKiRet(statsobj.SetName(inst->stats, statname));
	CHKiRet(statsobj.SetOrigin(inst->stats, (uchar*)"imdtls"));
	STATSCOUNTER_INIT(inst->ctrSubmit, inst->mutCtrSubmit);
	CHKiRet(statsobj.AddCounter(inst->stats, UCHAR_CONSTANT("submitted"),
		ctrType_IntCtr, CTR_FLAG_RESETTABLE, &(inst->ctrSubmit)));
	CHKiRet(statsobj.ConstructFinalize(inst->stats));
	/* end stats counters */

	// Init OpenSSL Context with DTLS_server_method
	CHKiRet(net_ossl.osslCtxInit(inst->pNetOssl, DTLS_method()));

#	if OPENSSL_VERSION_NUMBER >= 0x10002000L
	// Init OpenSSL Cookie Callbacks
	CHKiRet(net_ossl.osslCtxInitCookie(inst->pNetOssl));
#	endif
	// Run openssl config commands in Context
	CHKiRet(net_ossl.osslApplyTlscgfcmd(inst->pNetOssl, inst->tlscfgcmd));

	// Init Socket
	CHKiRet(DTLSCreateSocket(inst));
finalize_it:
	if(iRet != RS_RET_OK) {
		LogError(0, NO_ERRCODE, "DTLS Listener for thread failed to create UDP socket "
				"for thread %s is not functional!", inst->pszInputName);
	} else {
		DBGPRINTF("imdtls: DTLS Listener for thread %s added\n", inst->pszInputName);
	}
	RETiRet;
}

static rsRetVal
processMsg(instanceConf_t *inst, dtlsClient_t *dtlsClient, char *msg, size_t lenMsg)
{
	DEFiRet;
	smsg_t *pMsg = NULL;
	prop_t *pProp = NULL;
	BIO *wbio;
	BIO_ADDR *peer_addr;

	/* Get Gentime */
	time_t ttGenTime = 0;
	struct syslogTime stTime;
	datetime.getCurrTime(&stTime, &ttGenTime, TIME_IN_LOCALTIME);

	/* we now create our own message object and submit it to the queue */
	CHKiRet(msgConstructWithTime(&pMsg, &stTime, ttGenTime));
	MsgSetRawMsg(pMsg, msg, lenMsg);
	MsgSetInputName(pMsg, inst->pInputName);
	MsgSetRuleset(pMsg, inst->pBindRuleset);
	MsgSetFlowControlType(pMsg, eFLOWCTL_NO_DELAY);
	pMsg->msgFlags  = NEEDS_PARSING | PARSE_HOSTNAME;

	// Obtain Sender from BIO
	wbio = SSL_get_wbio(dtlsClient->sslClient);
	peer_addr = BIO_ADDR_new();
	if (BIO_dgram_get_peer(wbio, peer_addr)) {
		char *pHostname = BIO_ADDR_hostname_string(peer_addr, 1);
		DBGPRINTF("imdtls: processMsg Received message from %s: %s\n", pHostname, msg);
		MsgSetRcvFromStr(pMsg, (uchar *)pHostname, strlen(pHostname), &pProp);
		CHKiRet(prop.Destruct(&pProp));
		OPENSSL_free(pHostname);
	} else {
		DBGPRINTF("imdtls: processMsg Received message from UNKNOWN: %s\n", msg);
	}
	BIO_ADDR_free(peer_addr);

	// Update Activity
	dtlsClient->lastActivityTime = time(NULL);

	// Submit Message
	CHKiRet(submitMsg2(pMsg));
	STATSCOUNTER_INC(inst->ctrSubmit, inst->mutCtrSubmit);
finalize_it:
	RETiRet;
}

static void
DTLScleanupSession(instanceConf_t *inst, int idx) {
	if (inst->dtlsClients[idx]->sslClient != NULL) {
		BIO *rbio = SSL_get_rbio(inst->dtlsClients[idx]->sslClient);
		if (rbio) {
			// Close socket FIRST!
			int clientfd = -1;
			BIO_get_fd(rbio, &clientfd);
			if (clientfd != -1) {
				close(clientfd);
			}
		}
		SSL_free(inst->dtlsClients[idx]->sslClient);
		DBGPRINTF("imdtls: DTLScleanupSession Socket/SSL for Client idx (%d) terminated.\n", idx);
	}
	inst->dtlsClients[idx]->sslClient = NULL;
	inst->dtlsClients[idx]->lastActivityTime = 0;
}

static void
DTLSAcceptSession(instanceConf_t *inst, int idx) {
	int ret, err;
	SSL* ssl = inst->dtlsClients[idx]->sslClient;
	DBGPRINTF("imdtls: DTLSAcceptSession for Client idx (%d).\n", idx);

	// Check if the handshake has already been completed
	ret = SSL_get_state(ssl);
	if (ret != TLS_ST_OK) {
		// Existing client Finish handshake
		ret = SSL_accept(ssl);
		if (ret <= 0) {
			err = SSL_get_error(ssl, ret);
			if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
				// Non-blocking operation did not complete; retry later
				DBGPRINTF("imdtls: SSL_accept didn't complete (%d). Will retry.\n", err);
			} else if(err == SSL_ERROR_SYSCALL) {
				DBGPRINTF("imdtls: SSL_accept failed SSL_ERROR_SYSCALL idx (%d), removing client.\n",
					idx);
				net_ossl.osslLastOpenSSLErrorMsg(NULL, err, ssl, LOG_WARNING,
					"DTLSHandleSessions", "SSL_accept");
				DTLScleanupSession(inst, idx);
			} else {
				// An actual error occurred
				DBGPRINTF("imdtls: SSL_accept failed (%d) idx (%d), removing client.\n", err, idx);
				net_ossl.osslLastOpenSSLErrorMsg(NULL, err, ssl, LOG_ERR,
					"DTLSHandleSessions", "SSL_accept");
				DTLScleanupSession(inst, idx);
			}
		} else {
			DBGPRINTF("imdtls: SSL_accept success idx (%d), adding client.\n", idx);
			inst->dtlsClients[idx]->lastActivityTime = time(NULL);

			int status = 1;
			status = imdtls_verify_callback(status, ssl);
			if (status == 0) {
				LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING,
					"imdtls: Cert Verify FAILED for DTLS client idx (%d)",idx);
			} else {
				DBGPRINTF("imdtls: Cert Verify SUCCESS for DTLS client idx (%d)\n", idx);
			}
		}
	} else {
		DBGPRINTF("imdtls: SSL_get_state for DTLS client idx (%d) is %d\n", idx, ret);
	}
}

static void
DTLSTerminateClients(instanceConf_t *inst) {
	// Process pending Client Data first!
	for (int i = 0; i < MAX_DTLS_CLIENTS; ++i) {
		if (inst->dtlsClients[i]->sslClient != NULL) {
			DTLScleanupSession(inst, i);
		}
	}
}

static void
DTLSReadClient(instanceConf_t *inst, int idx, short revents) {
	int err;
	SSL* ssl = inst->dtlsClients[idx]->sslClient;
	DBGPRINTF("imdtls: DEBUG Check Client activity on index %d.\n", idx);
	if (revents & POLLIN) {
		if (ssl == NULL) {
			DBGPRINTF("imdtls: DTLSHandleSessions MISSING SSL OBJ for index %d.\n", idx);
			return;
		}
		DBGPRINTF("imdtls: Read Client activity on index %d.\n", idx);
		char buf[MAX_DTLS_MSGSIZE];
		int len = 0;
		do {
			len = SSL_read(ssl, buf, sizeof(buf) - 1);
			if (len > 0) {
				buf[len] = '\0';
				processMsg(inst, inst->dtlsClients[idx], buf, len);
			} else {
				err = SSL_get_error(ssl, len);
				if (err == SSL_ERROR_WANT_READ) {
					DBGPRINTF("imdtls: SSL_ERROR_WANT_READ flush rbio on index %d.\n", idx);
					BIO *rbio = SSL_get_rbio(ssl);
					BIO_flush(rbio);
				} else if (err == SSL_ERROR_WANT_WRITE) {
					DBGPRINTF("imdtls: SSL_ERROR_WANT_WRITE flush wbio on index %d.\n", idx);
					BIO *wbio = SSL_get_wbio(ssl);
					BIO_flush(wbio);
				} else if (err == SSL_ERROR_ZERO_RETURN) {
					DBGPRINTF("imdtls: SSL_ERROR_ZERO_RETURN on index %d.\n", idx);
					break;
				} else if (err == SSL_ERROR_SYSCALL) {
					DBGPRINTF("imdtls: SSL_ERROR_SYSCALL on index %d ERRNO %d\n", idx, errno);
					net_ossl.osslLastOpenSSLErrorMsg(NULL, err, ssl, LOG_ERR,
						"DTLSReadClient", "SSL_read");
					DTLScleanupSession(inst, idx);
					break;
				} else {
					DBGPRINTF("imdtls: SSL_read error %d (%d) on idx %d, rem client.\n",
						err, errno, idx);
					DTLScleanupSession(inst, idx);
					// Exit the loop if any error occurs
					break;
				}
			}
		} while (len > 0);
	}
}

static void
DTLSHandleSessions(instanceConf_t *inst) {
	int fdToIndex[MAX_DTLS_CLIENTS + 1];
	struct pollfd fds[MAX_DTLS_CLIENTS + 1];
	int optval = 1;
	int fdcount = 0;
	int ret, err;
	memset(fdToIndex, 0, sizeof(fdToIndex));
	memset(fds, 0, sizeof(fds));
	fds[0].fd = inst->sockfd;
	fds[0].events = POLLIN;
	DBGPRINTF("imdtls: DTLSHandleSessions ENTER \n");

	// Create FDS Array for Polling sockets
	for (int i = 0; i < MAX_DTLS_CLIENTS; ++i) {
		if (inst->dtlsClients[i]->sslClient != NULL) {
			int clientfd = -1;
			fdcount++;
			BIO_get_fd(SSL_get_wbio(inst->dtlsClients[i]->sslClient), &clientfd);
			DBGPRINTF("imdtls: DTLSHandleSessions handle client %d (%d)\n", fdcount, clientfd);
			fds[fdcount].fd = clientfd;
			fds[fdcount].events = POLLIN;
			fdToIndex[clientfd] = i; // Map fd to dtlsClients index
		}
	}

	DBGPRINTF("imdtls: Waiting for poll (clients %d) ...\n", fdcount);
	ret = poll(fds, fdcount+1, -1);
	if(glbl.GetGlobalInputTermState() == 1) {
		DBGPRINTF("imdtls: DTLSHandleSessions Terminate State\n");
		return; /* terminate input! */
	}
	if (ret < 0) {
		DBGPRINTF("imdtls: DTLSHandleSessions ERROR poll failed %d with err %d\n", ret , errno);
		return;
	}

	// Process pending Client Data first!
	DBGPRINTF("imdtls: DTLSHandleSessions handle client sockets (%d) \n", fdcount);
	for (int i = 1; i <= fdcount; ++i) {
		DTLSReadClient(inst, fdToIndex[fds[i].fd], fds[i].revents);
	}

	// Check session timeouts
	for (int i = 0; i < MAX_DTLS_CLIENTS; ++i) {
		if (inst->dtlsClients[i]->sslClient != NULL) {
			if (difftime(time(NULL), inst->dtlsClients[i]->lastActivityTime) > inst->timeout) {
				DBGPRINTF("imdtls: Session timeout (%d) for client index %d.\n", i, inst->timeout);
				DTLScleanupSession(inst, i);
				continue;
			}
		}
	}

	// Check MAIN socket for new connections
	if (fds[0].revents & POLLIN) {
		DBGPRINTF("imdtls: DTLSHandleSessions handle main socket\n");

		// Create BIO Object for potential new client
		BIO *sbio = BIO_new_dgram(inst->sockfd, BIO_NOCLOSE);
		BIO_ctrl(sbio, BIO_CTRL_DGRAM_MTU_DISCOVER, 0, NULL);

		// Create SSL Object for new client and apply default callbacks
		SSL *ssl = SSL_new(inst->pNetOssl->ctx);
		SSL_set_bio(ssl, sbio, sbio);
		SSL_set_accept_state(ssl);
		if (inst->pNetOssl->authMode != OSSL_AUTH_CERTANON) {
			dbgprintf("imdtls: enable certificate checking (Mode=%d, VerifyDepth=%d)\n",
				inst->pNetOssl->authMode, inst->CertVerifyDepth);
			net_ossl.osslSetSslVerifyCallback(ssl,
				SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT);
			if (inst->CertVerifyDepth != 0) {
				SSL_set_verify_depth(ssl, inst->CertVerifyDepth);
			}
		}

		// Store reference in SSL obj
		SSL_set_ex_data(ssl, 0, NULL); /* Reserved for pTcp */
		SSL_set_ex_data(ssl, 1, NULL); /* Reserved for permitExpiredCerts */
		SSL_set_ex_data(ssl, 2, inst); /* Used in imdtls */

		// Debug Callback for conn sbio!
		net_ossl.osslSetBioCallback(sbio);

		// Connect the new Client
		BIO_ADDR *client_addr = BIO_ADDR_new();

		// Start DTLS Listen and Session
		do {
			ret = DTLSv1_listen(ssl, client_addr);
			if (ret > 0) {
				dbgprintf("imdtls: DTLSHandleSessions DTLSv1_listen SUCCESS\n");
				// Create new CLIENT socket for communication!
				int clientfd = socket(BIO_ADDR_family(client_addr), SOCK_DGRAM, 0);
				if (clientfd < 0) {
					LogError(0, NO_ERRCODE, "imdtls: DTLSHandleSessions unable to create"
						" client socket");
					return;
				}
				setsockopt(clientfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
				setsockopt(clientfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
				// Bind and Connect Client Socket
				if (bind(clientfd,
					(struct sockaddr*) &inst->server_addr, sizeof(struct sockaddr_in)) < 0) {
					LogError(0, NO_ERRCODE, "imdtls: DTLSHandleSessions unable to bind"
							" client socket"
							" ignoring port %d bind-address %s.",
							inst->port, inst->pszBindAddr);
					return;
				}
				// Set new fd and set BIO to connected
				BIO *rbio = SSL_get_rbio(ssl);
				BIO_set_fd(rbio, clientfd, BIO_NOCLOSE);

				// Set and activate timeouts
				struct timeval timeout;
				timeout.tv_sec = inst->timeout;
				timeout.tv_usec = 0;
				BIO_ctrl(rbio, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);

				// Set BIO to connected
				ret = BIO_connect(clientfd, client_addr, 0);
				if (ret == 0) {
					err = SSL_get_error(ssl, ret);
					DBGPRINTF("imdtls: DTLSHandleSessions BIO_connect ERROR %d\n", err);
					net_ossl.osslLastOpenSSLErrorMsg(NULL, err, ssl, LOG_WARNING,
						"DTLSHandleSessions", "BIO_connect");
					LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING,
						"imdtls: BIO_connect failed for DTLS client");
					SSL_free(ssl);
				} else {
					BIO_ctrl_set_connected(rbio, client_addr);
					DBGPRINTF("imdtls: BIO_connect succeeded.\n");

					// Add to DTLS Clients
					for (int i = 0; i < MAX_DTLS_CLIENTS; ++i) {
						if (inst->dtlsClients[i]->sslClient == NULL) {
							inst->dtlsClients[i]->sslClient = ssl;
							inst->dtlsClients[i]->lastActivityTime = time(NULL);
							DBGPRINTF("imdtls: New Client added at idx %d.\n", i);
							DTLSAcceptSession(inst, i);
							break;
						}
					}
				}
				break;
			} else {
				err = SSL_get_error(ssl, ret);
				if (	(ret == 0 && err == SSL_ERROR_SYSCALL) ||
					(err == SSL_ERROR_SYSCALL && errno == EAGAIN)) {
					DBGPRINTF("imdtls: DTLSv1_listen RET %d (ERR %d / ERRNO %d), retry\n",
						ret, err, errno);
					// Wait little and retry DTLSv1_listen
					srSleep(0, 100000);
					continue;
				} else {
					DBGPRINTF("imdtls: DTLSv1_listen RET %d (ERR %d / ERRNO %d), abort\n",
						ret, err, errno);
					net_ossl.osslLastOpenSSLErrorMsg(NULL, err, ssl, LOG_WARNING,
						"DTLSHandleSessions", "DTLSv1_listen");
					LogMsg(0, RS_RET_NO_ERRCODE, LOG_WARNING,
						"imdtls: DTLSv1_listen failed for DTLS client");
					SSL_free(ssl);
					break;
				}
			}
		}
		while (1);
		BIO_ADDR_free(client_addr);
	}
}

static void*
startDtlsHandler(void *myself) {
	instanceConf_t *inst = (instanceConf_t *) myself;
	DBGPRINTF("imdtls: start DtlsHandler for thread %s\n", inst->pszInputName);

	/* DTLS Receiving Loop */
	while(glbl.GetGlobalInputTermState() == 0) {
		DBGPRINTF("imdtls: begin handle DTSL Client Sessions\n");
		DTLSHandleSessions(inst);
	}

	/* DTLS Terminate Sessions */
	DBGPRINTF("imdtls: Terminate DTLS Client Sessions\n");
	DTLSTerminateClients(inst);

	DBGPRINTF("imdtls: stop DtlsHandler for thread %s\n", inst->pszInputName);
	return NULL;
}

/* Set permitted peers. It is depending on the auth mode if this are
 * fingerprints or names. -- rgerhards, 2008-05-19
 */
static rsRetVal
SetPermPeers(instanceConf_t *inst, permittedPeers_t *pPermPeers)
{
	DEFiRet;
	if(pPermPeers == NULL)
		FINALIZE;

	if(inst->pNetOssl->authMode != OSSL_AUTH_CERTFINGERPRINT && inst->pNetOssl->authMode != OSSL_AUTH_CERTNAME) {
		LogError(0, RS_RET_VALUE_NOT_IN_THIS_MODE, "authentication not supported by "
				"imdtls in the configured authentication mode - ignored");
		ABORT_FINALIZE(RS_RET_VALUE_NOT_IN_THIS_MODE);
	}
	inst->pNetOssl->pPermPeers = pPermPeers;
finalize_it:
	RETiRet;
}
BEGINnewInpInst
	struct cnfparamvals *pvals;
	instanceConf_t *inst = NULL;
	int i,j;
	FILE *fp;
CODESTARTnewInpInst
	DBGPRINTF("newInpInst (imdtls)\n");

	if((pvals = nvlstGetParams(lst, &inppblk, NULL)) == NULL) {
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		dbgprintf("input param blk in imdtls:\n");
		cnfparamsPrint(&inppblk, pvals);
	}

	CHKiRet(createInstance(&inst));

	for(i = 0 ; i < inppblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(inppblk.descr[i].name, "port")) {
			inst->pszBindPort = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "address")) {
			inst->pszBindAddr = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "timeout")) {
			inst->timeout = (int) pvals[i].val.d.n;
		} else if(!strcmp(inppblk.descr[i].name, "ruleset")) {
			inst->pszBindRuleset = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "name")) {
			inst->pszInputName = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "tls.authmode")) {
			char* pszAuthMode = es_str2cstr(pvals[i].val.d.estr, NULL);
			if(!strcasecmp(pszAuthMode, "fingerprint"))
				inst->pNetOssl->authMode = OSSL_AUTH_CERTFINGERPRINT;
			else if(!strcasecmp(pszAuthMode, "name"))
				inst->pNetOssl->authMode = OSSL_AUTH_CERTNAME;
			else if(!strcasecmp(pszAuthMode, "certvalid"))
				inst->pNetOssl->authMode = OSSL_AUTH_CERTVALID;
			else
				inst->pNetOssl->authMode = OSSL_AUTH_CERTANON;
			free(pszAuthMode);
		} else if(!strcmp(inppblk.descr[i].name, "tls.cacert")) {
			inst->pNetOssl->pszCAFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char*)inst->pNetOssl->pszCAFile, "r");
			if(fp == NULL) {
				char errStr[1024];
				rs_strerror_r(errno, errStr, sizeof(errStr));
				LogError(0, RS_RET_NO_FILE_ACCESS,
				"error: certificate file %s couldn't be accessed: %s\n",
				inst->pNetOssl->pszCAFile, errStr);
			} else {
				fclose(fp);
			}
		} else if(!strcmp(inppblk.descr[i].name, "tls.mycert")) {
			inst->pNetOssl->pszCertFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char*)inst->pNetOssl->pszCertFile, "r");
			if(fp == NULL) {
				char errStr[1024];
				rs_strerror_r(errno, errStr, sizeof(errStr));
				LogError(0, RS_RET_NO_FILE_ACCESS,
				"error: certificate file %s couldn't be accessed: %s\n",
				inst->pNetOssl->pszCertFile, errStr);
			} else {
				fclose(fp);
			}
		} else if(!strcmp(inppblk.descr[i].name, "tls.myprivkey")) {
			inst->pNetOssl->pszKeyFile = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
			fp = fopen((const char*)inst->pNetOssl->pszKeyFile, "r");
			if(fp == NULL) {
				char errStr[1024];
				rs_strerror_r(errno, errStr, sizeof(errStr));
				LogError(0, RS_RET_NO_FILE_ACCESS,
				"error: certificate file %s couldn't be accessed: %s\n",
				inst->pNetOssl->pszKeyFile, errStr);
			} else {
				fclose(fp);
			}
		} else if(!strcmp(inppblk.descr[i].name, "tls.tlscfgcmd")) {
			inst->tlscfgcmd = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else if(!strcmp(inppblk.descr[i].name, "tls.permittedpeer")) {
			for(j = 0 ; j <  pvals[i].val.d.ar->nmemb ; ++j) {
				uchar *const peer = (uchar*) es_str2cstr(pvals[i].val.d.ar->arr[j], NULL);
				CHKiRet(net.AddPermittedPeer(&inst->pPermPeersRoot, peer));
				free(peer);
			}
		} else {
			dbgprintf("imdtls: program error, non-handled "
			  "param '%s'\n", inppblk.descr[i].name);
		}
	}

	/* check if no port is set. If not, we use DEFAULT of 4433 */
	if(inst->pszBindPort == NULL) {
		CHKmalloc(inst->pszBindPort = (uchar*)strdup("4433"));
	}
	inst->port = atoi((char*)inst->pszBindPort);
	/* check if BinAddr is set. If not, we use DEFAULT of 0.0.0.0 */
	if(inst->pszBindAddr == NULL) {
		CHKmalloc(inst->pszBindAddr = (uchar*)strdup("0.0.0.0"));
	}

	// Assign Loaded Peers to Netossl
	SetPermPeers(inst, inst->pPermPeersRoot);

	/* all ok, ready to start up */
	inst->bEnableLstn = -1;

finalize_it:
CODE_STD_FINALIZERnewInpInst
	cnfparamvalsDestruct(pvals, &inppblk);
ENDnewInpInst


BEGINbeginCnfLoad
CODESTARTbeginCnfLoad
	loadModConf = pModConf;
	pModConf->pConf = pConf;
	pModConf->pszBindRuleset = NULL;
	pModConf->drvrAuthMode = OSSL_AUTH_CERTANON;
	/* init legacy config variables */
	cs.pszBindRuleset = NULL;
ENDbeginCnfLoad


BEGINsetModCnf
	struct cnfparamvals *pvals = NULL;
	int i;
CODESTARTsetModCnf
	pvals = nvlstGetParams(lst, &modpblk, NULL);
	if(pvals == NULL) {
		LogError(0, RS_RET_MISSING_CNFPARAMS, "imdtls: error processing module "
				"config parameters [module(...)]");
		ABORT_FINALIZE(RS_RET_MISSING_CNFPARAMS);
	}

	if(Debug) {
		dbgprintf("module (global) param blk for imdtls:\n");
		cnfparamsPrint(&modpblk, pvals);
	}

	for(i = 0 ; i < modpblk.nParams ; ++i) {
		if(!pvals[i].bUsed)
			continue;
		if(!strcmp(modpblk.descr[i].name, "tls.authmode")) {
			char* pszAuthMode = es_str2cstr(pvals[i].val.d.estr, NULL);
			if(!strcasecmp(pszAuthMode, "fingerprint"))
				loadModConf->drvrAuthMode = OSSL_AUTH_CERTFINGERPRINT;
			else if(!strcasecmp(pszAuthMode, "name"))
				loadModConf->drvrAuthMode = OSSL_AUTH_CERTNAME;
			else if(!strcasecmp(pszAuthMode, "certvalid"))
				loadModConf->drvrAuthMode = OSSL_AUTH_CERTVALID;
			else
				loadModConf->drvrAuthMode = OSSL_AUTH_CERTANON;
			free(pszAuthMode);
		} else if(!strcmp(modpblk.descr[i].name, "ruleset")) {
			loadModConf->pszBindRuleset = (uchar*)es_str2cstr(pvals[i].val.d.estr, NULL);
		} else {
			dbgprintf("imdtls: program error, non-handled "
			  "param '%s' in beginCnfLoad\n", modpblk.descr[i].name);
		}
	}
finalize_it:
	if(pvals != NULL)
		cnfparamvalsDestruct(pvals, &modpblk);
ENDsetModCnf


BEGINendCnfLoad
CODESTARTendCnfLoad
	if(loadModConf->pszBindRuleset == NULL) {
		if((cs.pszBindRuleset == NULL) || (cs.pszBindRuleset[0] == '\0')) {
			loadModConf->pszBindRuleset = NULL;
		} else {
			CHKmalloc(loadModConf->pszBindRuleset = ustrdup(cs.pszBindRuleset));
		}
	} else {
		if((cs.pszBindRuleset != NULL) && (cs.pszBindRuleset[0] != '\0')) {
			LogError(0, RS_RET_DUP_PARAM, "imdtls: ruleset "
					"set via legacy directive ignored");
		}
	}
finalize_it:
	free(cs.pszBindRuleset);
	cs.pszBindRuleset = NULL;
	loadModConf = NULL; /* done loading */
ENDendCnfLoad


BEGINcheckCnf
	instanceConf_t *inst;
CODESTARTcheckCnf
	for(inst = pModConf->root ; inst != NULL ; inst = inst->next) {
		if(inst->pszBindRuleset == NULL && pModConf->pszBindRuleset != NULL) {
			CHKmalloc(inst->pszBindRuleset = ustrdup(pModConf->pszBindRuleset));
		}
		std_checkRuleset(pModConf, inst);
	}
finalize_it:
ENDcheckCnf


BEGINactivateCnfPrePrivDrop
	instanceConf_t *inst;
CODESTARTactivateCnfPrePrivDrop
	runModConf = pModConf;
	DBGPRINTF("imdtls: activate addListners for dtls\n");
	for(inst = runModConf->root ; inst != NULL ; inst = inst->next) {
		addListner(pModConf, inst);
	}
ENDactivateCnfPrePrivDrop

BEGINactivateCnf
CODESTARTactivateCnf
ENDactivateCnf


BEGINfreeCnf
	instanceConf_t *inst, *del;
	int i;
CODESTARTfreeCnf
	for(inst = pModConf->root ; inst != NULL ; ) {
		free(inst->pszBindPort);
		if (inst->pszBindAddr != NULL) {
			free(inst->pszBindAddr);
		}
		free(inst->pszBindRuleset);
		free(inst->pszInputName);

		// --- CleanUP OpenSSL ressources
		// Remove SSL CLients
		if(inst->dtlsClients != NULL) {
			for (i = 0; i < MAX_DTLS_CLIENTS; ++i) {
				DTLScleanupSession(inst, i);
				free(inst->dtlsClients[i]);
			}
			free(inst->dtlsClients);
		}

		// DeConstruct pNetOssl helper
		net_ossl.Destruct(&inst->pNetOssl);

		// ---

		if(inst->pPermPeersRoot != NULL) {
			net.DestructPermittedPeers(&inst->pPermPeersRoot);
		}

		if(inst->bEnableLstn) {
			prop.Destruct(&inst->pInputName);
			statsobj.Destruct(&(inst->stats));
		}
		del = inst;
		inst = inst->next;
		free(del);
	}
	free(pModConf->pszBindRuleset);
ENDfreeCnf



/* This function is called to gather input.
 * In essence, it just starts the pool of workers. To save resources,
 * we run one of the workers on our own thread -- otherwise that thread would
 * just idle around and wait for the workers to finish.
 */
BEGINrunInput
	instanceConf_t *inst;
	pthread_attr_t wrkrThrdAttr;
CODESTARTrunInput
	pthread_attr_init(&wrkrThrdAttr);
	pthread_attr_setstacksize(&wrkrThrdAttr, 4096*1024);

	DBGPRINTF("imdtls: create dtls handling threads\n");
	for(inst = runModConf->root ; inst != NULL ; inst = inst->next) {
		if(inst->bEnableLstn) {
			pthread_create(&inst->tid, &wrkrThrdAttr, startDtlsHandler, inst);
		}
	}
	pthread_attr_destroy(&wrkrThrdAttr);

	DBGPRINTF("imdtls: starting to wait for close condition\n");
	while(glbl.GetGlobalInputTermState() == 0) {
		srSleep(0, 400000);
	}

	DBGPRINTF("imdtls: received close signal, signaling instance threads...\n");
	for (inst = runModConf->root; inst != NULL; inst = inst->next) {
		pthread_kill(inst->tid, SIGTTIN);
		DTLSCloseSocket(inst);
	}

	DBGPRINTF("imdtls: threads signaled, waiting for join...");
	for (inst = runModConf->root ; inst != NULL ; inst = inst->next) {
		pthread_join(inst->tid, NULL);
	}

	DBGPRINTF("imdtls: finished threads, stopping\n");
ENDrunInput


BEGINwillRun
CODESTARTwillRun
	/* we need to create the inputName property (only once during our lifetime) */
	CHKiRet(prop.Construct(&pInputName));
	CHKiRet(prop.SetString(pInputName, UCHAR_CONSTANT("imdtls"), sizeof("imdtls") - 1));
	CHKiRet(prop.ConstructFinalize(pInputName));
finalize_it:
ENDwillRun

/* This function is called by the framework after runInput() has been terminated. It
 * shall free any resources and prepare the module for unload.
 * CODEqueryEtryPt_STD_IMOD_QUERIES
 */
BEGINafterRun
CODESTARTafterRun
	/* TODO: do cleanup here ?! */
	dbgprintf("imdtls: AfterRun\n");
	if(pInputName != NULL)
		prop.Destruct(&pInputName);
ENDafterRun

BEGINmodExit
CODESTARTmodExit
	DBGPRINTF("imdtls: modExit\n");
	/* release objects we used */
	objRelease(net_ossl, LM_NET_OSSL_FILENAME);
	objRelease(statsobj, CORE_COMPONENT);
	objRelease(ruleset, CORE_COMPONENT);
	objRelease(datetime, CORE_COMPONENT);
	objRelease(prop, CORE_COMPONENT);
	objRelease(net, LM_NET_FILENAME);
	objRelease(glbl, CORE_COMPONENT);
ENDmodExit

BEGINisCompatibleWithFeature
CODESTARTisCompatibleWithFeature
	if(eFeat == sFEATURENonCancelInputTermination)
		iRet = RS_RET_OK;
ENDisCompatibleWithFeature

BEGINqueryEtryPt
CODESTARTqueryEtryPt
CODEqueryEtryPt_STD_IMOD_QUERIES
CODEqueryEtryPt_STD_CONF2_QUERIES
CODEqueryEtryPt_STD_CONF2_setModCnf_QUERIES
CODEqueryEtryPt_STD_CONF2_PREPRIVDROP_QUERIES
CODEqueryEtryPt_STD_CONF2_IMOD_QUERIES
CODEqueryEtryPt_IsCompatibleWithFeature_IF_OMOD_QUERIES
ENDqueryEtryPt

BEGINmodInit()
CODESTARTmodInit
	*ipIFVersProvided = CURR_MOD_IF_VERSION; /* we only support the current interface specification */
CODEmodInit_QueryRegCFSLineHdlr
	DBGPRINTF("imdtls: modInit\n");
	/* request objects we use */
	CHKiRet(objUse(glbl, CORE_COMPONENT));
	CHKiRet(objUse(prop, CORE_COMPONENT));
	CHKiRet(objUse(net, LM_NET_FILENAME));
	CHKiRet(objUse(net_ossl, LM_NET_OSSL_FILENAME));
	CHKiRet(objUse(datetime, CORE_COMPONENT));
	CHKiRet(objUse(ruleset, CORE_COMPONENT));
	CHKiRet(objUse(statsobj, CORE_COMPONENT));
ENDmodInit
