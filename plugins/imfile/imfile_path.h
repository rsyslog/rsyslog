/* SPDX-License-Identifier: Apache-2.0 */

#ifndef IMFILE_PATH_H
#define IMFILE_PATH_H

#include <stddef.h>
#include <string.h>

/* Return a pointer and length for path's parent component. The parent of an
 * absolute path with one component (for example, "/LN") is "/". A path with
 * no slash has no parent component and returns 0.
 */
static inline int imfile_get_parent_path(const char *const path,
                                         const char **const parent_path,
                                         size_t *const parent_len) {
    const char *const slash = strrchr(path, '/');
    if (slash == NULL) {
        return 0;
    }

    *parent_path = path;
    *parent_len = (slash == path) ? 1 : (size_t)(slash - path);
    return 1;
}

#endif
