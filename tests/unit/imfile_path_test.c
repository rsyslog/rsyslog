/* Unit test for imfile parent-path extraction. In particular, this verifies
 * that a top-level absolute path such as "/LN" has parent "/" without
 * creating or modifying any path in the host root filesystem.
 */
#include "config.h"

#include <stdio.h>
#include <string.h>

#include "imfile_path.h"

static int expect_parent(const char *const path, const char *const expected_parent) {
    const char *parent_path = NULL;
    size_t parent_len = 0;
    const int has_parent = imfile_get_parent_path(path, &parent_path, &parent_len);
    const size_t expected_len = strlen(expected_parent);

    if (has_parent != 1 || parent_path != path || parent_len != expected_len ||
        memcmp(parent_path, expected_parent, expected_len) != 0) {
        fprintf(stderr, "parent of '%s' did not match '%s'\n", path, expected_parent);
        return 1;
    }
    return 0;
}

static int expect_no_parent(const char *const path) {
    const char *parent_path = NULL;
    size_t parent_len = 0;

    if (imfile_get_parent_path(path, &parent_path, &parent_len) != 0) {
        fprintf(stderr, "path '%s' unexpectedly has a parent\n", path);
        return 1;
    }
    return 0;
}

int main(void) {
    /* This is the exact boundary that used to produce an empty parent, along
     * with nested, relative, root, and slash-free paths to pin path semantics.
     * The oracle is the returned parent slice; no filesystem state is needed.
     */
    if (expect_parent("/LN", "/") || expect_parent("/some/directory/file.log", "/some/directory") ||
        expect_parent("relative/file.log", "relative") || expect_parent("/", "/") || expect_no_parent("file.log")) {
        return 1;
    }
    return 0;
}
