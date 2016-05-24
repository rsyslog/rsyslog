/* The RELPCLT object.
 *
 * Copyright 2008-2013 by Rainer Gerhards and Adiscon GmbH.
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
 * along with librelp.  If not, see <http://www.gnu.org/licenses/>.
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
#ifndef RELPCLT_H_INCLUDED
#define	RELPCLT_H_INCLUDED

/* the RELPCLT object 
 * rgerhards, 2008-03-16
 */
struct relpClt_s {
	BEGIN_RELP_OBJ;
	relpEngine_t *pEngine;
	relpSess_t *pSess;	/**< our session (the one and only!) */
	void *pUsr;		/**< user pointer (opaque data) */
	int bEnableTLS;		/**< is TLS to be used? */
	int bEnableTLSZip;	/**< is compression to be used together with TLS? */
	int sizeWindow;		/**< size of our app-level communications window */
	char *pristring;	/**< priority string for GnuTLS */
	relpAuthMode_t authmode;
	char *caCertFile;
	char *ownCertFile;
	char *privKey;
	relpPermittedPeers_t permittedPeers;
	int protFamily;		/**< protocol family to connect over (IPv4, v6, ...) */
	unsigned char *port;	/**< server port to connect to */
	unsigned char *host;	/**< host(name) to connect to */
	unsigned char *clientIP;/**< ip to bind to, or NULL if irrelevant */
	unsigned timeout;	/**< session timeout */
};


/* prototypes */
relpRetVal relpCltConstruct(relpClt_t **ppThis, relpEngine_t *pEngine);
relpRetVal relpCltDestruct(relpClt_t **ppThis);

#endif /* #ifndef RELPCLT_H_INCLUDED */
