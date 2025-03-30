/* An implementation of the nsd interface for Mbed TLS.
 *
 * Copyright 2008-2021 Adiscon GmbH.
 * Copyright (C) 2023 CS Group.
 *
 * This file is part of the rsyslog runtime library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
 *		 -or-
 *		 see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDED_NSD_MBEDTLS_H
#define INCLUDED_NSD_MBEDTLS_H

#include <mbedtls/net_sockets.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include "nsd.h"

typedef nsd_if_t nsd_mbedtls_if_t; /* we just *implement* this interface */

/* the nsd_mbedtls object */
struct nsd_mbedtls_s {
	BEGINobjInstance;		  /* Data to implement generic object - MUST be the first data element! */
	nsd_t *pTcp;			  /**< our aggregated nsd_ptcp data */
	const uchar *pszCAFile;
	const uchar *pszCRLFile;
	const uchar *pszKeyFile;
	const uchar *pszCertFile;
	int iMode;			  /* 0 - plain tcp, 1 - TLS */
	int bAbortConn;			  /* if set, abort conncection (fatal error had happened) */
	int authMode;			  /* Mbed TLS auth mode for mbedtls_ssl_conf_authmode() */
	int bChkName;			  /* if true, we check the certificate name */
	int bHaveSess;			  /* true if a tls session is active */
	int DrvrVerifyDepth;		  /* Verify Depth for certificate chains */
	const uchar *pPermPeer;		  /* permitted peer */
	int sock;

	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
	int bHaveCaCert;
	mbedtls_x509_crt cacert;
	int bHaveCrl;
	mbedtls_x509_crl crl;
	int bHaveKey;
	mbedtls_pk_context pkey;
	int bHaveCert;
	mbedtls_x509_crt srvcert;
};

/* interface is defined in nsd.h, we just implement it! */
#define nsd_mbedtlsCURR_IF_VERSION nsdCURR_IF_VERSION

/* prototypes */
PROTOTYPEObj(nsd_mbedtls);

/* the name of our library binary */
#define LM_NSD_MBEDTLS_FILENAME "lmnsd_mbedtls"

#endif /* #ifndef INCLUDED_NSD_MBEDTLS_H */
