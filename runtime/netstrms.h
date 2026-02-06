/* Definitions for the stream-based netstrm sworking class.
 *
 * Copyright 2007-2025 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * The rsyslog runtime library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The rsyslog runtime library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the rsyslog runtime library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */

#ifndef INCLUDED_NETSTRMS_H
#define INCLUDED_NETSTRMS_H

#include "nsd.h" /* we need our driver interface to be defined */

/* the netstrms object */
struct netstrms_s {
    BEGINobjInstance
        ; /* Data to implement generic object - MUST be the first data element! */
        uchar *pBaseDrvrName; /**< nsd base driver name to use, or NULL if system default */
        uchar *pDrvrName; /**< full base driver name (set when driver is loaded) */
        int iDrvrMode; /**< current default driver mode */
        uchar *pszDrvrAuthMode; /**< current driver authentication mode */
        int DrvrChkExtendedKeyUsage; /**< if true, verify extended key usage in certs */
        int DrvrPrioritizeSan; /**< if true, perform stricter checking of names in certs */
        int DrvrVerifyDepth; /**< Verify Depth for certificate chains */
        int DrvrTlsRevocationCheck; /**< Enable TLS revocation checking (OCSP/CRL) */
        uchar *pszDrvrPermitExpiredCerts;
        const uchar *pszDrvrCAFile;
        const uchar *pszDrvrCRLFile;
        const uchar *pszDrvrKeyFile;
        const uchar *pszDrvrCertFile;
        uchar *gnutlsPriorityString; /**< priorityString for connection */
        uchar *remoteSNI; /**< Remote SNI to use instead of the server hostname */
        int iSynBacklog; /**< socket layer syn backlog for listen() */
        permittedPeers_t *pPermPeers; /**< current driver's permitted peers */
        rsRetVal (*fLstnInitDrvr)(netstrm_t *); /**< "late" driver-specific lstn init function NULL if none */

        nsd_if_t Drvr; /**< our stream driver */
};


/* interface */
BEGINinterface(netstrms) /* name must also be changed in ENDinterface macro! */
    rsRetVal (*Construct)(netstrms_t **ppThis);
    rsRetVal (*ConstructFinalize)(netstrms_t *pThis);
    rsRetVal (*Destruct)(netstrms_t **ppThis);
    rsRetVal (*CreateStrm)(netstrms_t *pThis, netstrm_t **ppStrm);
    rsRetVal (*SetDrvrName)(netstrms_t *pThis, uchar *pszName);
    rsRetVal (*SetDrvrMode)(netstrms_t *pThis, int iMode);
    rsRetVal (*SetDrvrAuthMode)(netstrms_t *pThis, uchar *);
    rsRetVal (*SetDrvrPermitExpiredCerts)(netstrms_t *pThis, uchar *);
    rsRetVal (*SetDrvrPermPeers)(netstrms_t *pThis, permittedPeers_t *);
    int (*GetDrvrMode)(netstrms_t *pThis);
    uchar *(*GetDrvrAuthMode)(netstrms_t *pThis);
    uchar *(*GetDrvrPermitExpiredCerts)(netstrms_t *pThis);
    permittedPeers_t *(*GetDrvrPermPeers)(netstrms_t *pThis);
    rsRetVal (*SetDrvrGnutlsPriorityString)(netstrms_t *pThis, uchar *);
    uchar *(*GetDrvrGnutlsPriorityString)(netstrms_t *pThis);
    rsRetVal (*SetDrvrCheckExtendedKeyUsage)(netstrms_t *pThis, int ChkExtendedKeyUsage);
    int (*GetDrvrCheckExtendedKeyUsage)(netstrms_t *pThis);
    rsRetVal (*SetDrvrPrioritizeSAN)(netstrms_t *pThis, int prioritizeSan);
    int (*GetDrvrPrioritizeSAN)(netstrms_t *pThis);
    rsRetVal (*SetDrvrTlsVerifyDepth)(netstrms_t *pThis, int verifyDepth);
    rsRetVal (*SetDrvrTlsRevocationCheck)(netstrms_t *pThis, int enabled);
    int (*GetDrvrTlsVerifyDepth)(netstrms_t *pThis);
    int (*GetDrvrTlsRevocationCheck)(netstrms_t *pThis);
    rsRetVal (*SetDrvrRemoteSNI)(netstrms_t *pThis, uchar *);
    uchar *(*GetDrvrRemoteSNI)(netstrms_t *pThis);
    /* v2 */
    rsRetVal (*SetDrvrTlsCAFile)(netstrms_t *pThis, const uchar *);
    const uchar *(*GetDrvrTlsCAFile)(netstrms_t *pThis);
    rsRetVal (*SetDrvrTlsKeyFile)(netstrms_t *pThis, const uchar *);
    const uchar *(*GetDrvrTlsKeyFile)(netstrms_t *pThis);
    rsRetVal (*SetDrvrTlsCertFile)(netstrms_t *pThis, const uchar *);
    const uchar *(*GetDrvrTlsCertFile)(netstrms_t *pThis);
    /* v3 */
    rsRetVal (*SetDrvrTlsCRLFile)(netstrms_t *pThis, const uchar *);
    const uchar *(*GetDrvrTlsCRLFile)(netstrms_t *pThis);

    /* v4 */
    rsRetVal (*SetSynBacklog)(netstrms_t *pThis, const int);

ENDinterface(netstrms)
#define netstrmsCURR_IF_VERSION 5 /* increment whenever you change the interface structure! */

/* prototypes */
PROTOTYPEObj(netstrms);
rsRetVal netstrmsConstruct(netstrms_t **ppThis);
rsRetVal netstrmsDestruct(netstrms_t **ppThis);

/* the name of our library binary */
#define LM_NETSTRMS_FILENAME "lmnetstrms"

#endif /* #ifndef INCLUDED_NETSTRMS_H */
