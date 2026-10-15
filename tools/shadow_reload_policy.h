/* SPDX-License-Identifier: Apache-2.0
 * @file
 * @brief Strict scalar lowering shared by the controller and daemon-linked unit.
 */
#ifndef SHADOW_RELOAD_POLICY_H_INCLUDED
#define SHADOW_RELOAD_POLICY_H_INCLUDED
#include "rsyslog.h"

/** @brief Lower one byte-valued scalar, using the default only when omitted.
 * Input is a borrowed, NUL-terminated string already checked for embedded NULs
 * by the owned source-profile builder. Empty/multibyte values fail closed.
 * On success only the caller's output byte changes; no runtime state is touched.
 */
static inline rsRetVal shadowReloadParseCharPolicy(const char *value, const uchar defaultValue, uchar *parsed) {
    if (parsed == NULL) return RS_RET_PARAM_ERROR;
    if (value == NULL)
        *parsed = defaultValue;
    else if (value[0] != '\0' && value[1] == '\0')
        *parsed = (uchar)value[0];
    else
        return RS_RET_CONF_PARAM_INVLD;
    return RS_RET_OK;
}
#endif
