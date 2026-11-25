/* dtls_authmode.h - shared tls.authmode parsing for imdtls and omdtls
 *
 * Copyright 2026 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
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
#ifndef INCLUDED_DTLS_AUTHMODE_H
#define INCLUDED_DTLS_AUTHMODE_H

#include <strings.h>
#include "rsyslog.h"
#include "rsconf.h"
#include "errmsg.h"
#include "net_ossl.h"

/* Translate a DTLS tls.authmode value into its AuthMode.
 *
 * Supported values are "fingerprint", "name", "certvalid" and the explicit
 * anonymous modes "anon" and "certanon" (case-insensitive). Older versions
 * silently treated every other value as anonymous mode, so a typo disabled
 * peer verification without notice. An unsupported value is now rejected
 * when global(compatibility.defaults.secure="strict") is active; otherwise
 * the anonymous fallback is kept for compatibility and a warning is emitted.
 *
 * modName  - module name used as message prefix ("imdtls"/"omdtls")
 * value    - configured parameter value
 * mode     - receives the resulting mode (unchanged on error)
 * returns RS_RET_OK or RS_RET_PARAM_ERROR (strict mode, unsupported value)
 */
static inline rsRetVal dtls_parse_auth_mode(const char *const modName, const char *const value, AuthMode *const mode) {
    if (!strcasecmp(value, "fingerprint")) {
        *mode = OSSL_AUTH_CERTFINGERPRINT;
    } else if (!strcasecmp(value, "name")) {
        *mode = OSSL_AUTH_CERTNAME;
    } else if (!strcasecmp(value, "certvalid")) {
        *mode = OSSL_AUTH_CERTVALID;
    } else if (!strcasecmp(value, "anon") || !strcasecmp(value, "certanon")) {
        *mode = OSSL_AUTH_CERTANON;
    } else if (loadConf != NULL && loadConf->globals.compatDefaultsSecure == COMPAT_DEFAULTS_SECURE_STRICT) {
        LogError(0, RS_RET_PARAM_ERROR,
                 "%s: unsupported tls.authmode \"%s\"; supported values are fingerprint, name, certvalid, "
                 "anon and certanon; compatibility.defaults.secure=\"strict\" rejects this configuration",
                 modName, value);
        return RS_RET_PARAM_ERROR;
    } else {
        LogMsg(0, RS_RET_PARAM_ERROR, LOG_WARNING,
               "%s: unsupported tls.authmode \"%s\"; using anonymous mode WITHOUT peer verification for "
               "compatibility; supported values are fingerprint, name, certvalid, anon and certanon; set "
               "global(compatibility.defaults.secure=\"strict\") to reject this configuration",
               modName, value);
        *mode = OSSL_AUTH_CERTANON;
    }
    return RS_RET_OK;
}

#endif /* #ifndef INCLUDED_DTLS_AUTHMODE_H */
