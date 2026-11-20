/* SPDX-License-Identifier: Apache-2.0
 *
 * Unit test for the config file record used by config.reloadOnHUP
 * (runtime/cfgfiles.c).
 *
 * Intent: two captures taken after the load must differ whenever any
 * configuration content differs, including the content of a file that only
 * started to match an include glob after the load. Such a file is not in the
 * load-time record; without its own content snapshot, an edit to it between
 * two HUPs coalesced into one check would go unnoticed.
 *
 * Also checked: a literal include name that is absent at load time (glob
 * NOMAGIC returns such a name unchanged) is detected when the file appears,
 * and a permission-only change of a recorded file is a change.
 *
 * Oracle: cfgfilesEqual() on captures. Setup is a private temp directory with
 * a main file and an include glob; no daemon, no timing involved.
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "cfgfiles.h"

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            goto done;                                                               \
        }                                                                            \
    } while (0)

static int writeFile(const char *path, const char *content) {
    FILE *fp = fopen(path, "w");
    if (fp == NULL) return -1;
    if (fputs(content, fp) == EOF) {
        fclose(fp);
        return -1;
    }
    return fclose(fp) == 0 ? 0 : -1;
}

int main(void) {
    char dirTmpl[] = "/tmp/rsyslog-cfgfiles-test-XXXXXX";
    char *dir;
    char mainPath[256], incDir[256], pattern[256], oldInc[256], newInc[256], litInc[256];
    FILE *fp = NULL;
    cfgfiles_t *c1 = NULL, *c2 = NULL, *c3 = NULL, *c4 = NULL, *c5 = NULL;
    int ret = 1;

    dir = mkdtemp(dirTmpl);
    if (dir == NULL) {
        perror("mkdtemp");
        return 1;
    }
    snprintf(mainPath, sizeof(mainPath), "%s/main.conf", dir);
    snprintf(incDir, sizeof(incDir), "%s/inc", dir);
    snprintf(pattern, sizeof(pattern), "%s/inc/*.conf", dir);
    snprintf(oldInc, sizeof(oldInc), "%s/inc/a.conf", dir);
    snprintf(newInc, sizeof(newInc), "%s/inc/b.conf", dir);
    snprintf(litInc, sizeof(litInc), "%s/literal.conf", dir);

    CHECK(writeFile(mainPath, "include(file=\"inc/*.conf\")\n") == 0);
    CHECK(mkdir(incDir, 0700) == 0);
    CHECK(writeFile(oldInc, "# a\n") == 0);
    CHECK(chmod(oldInc, 0644) == 0); /* known start mode, independent of umask */

    /* what a load records: the files the lexer opened plus the include glob */
    fp = fopen(mainPath, "r");
    CHECK(fp != NULL);
    cfgfilesRecordStream(fp, mainPath);
    fclose(fp);
    fp = fopen(oldInc, "r");
    CHECK(fp != NULL);
    cfgfilesRecordStream(fp, oldInc);
    fclose(fp);
    fp = NULL;
    cfgfilesRecordGlob(pattern);
    cfgfilesRecordGlob(litInc); /* literal name, does not exist yet */
    cfgfilesSeal();
    CHECK(cfgfilesFileCount(cfgfilesGetRecord()) == 2);

    /* nothing changed */
    c1 = cfgfilesCapture(cfgfilesGetRecord());
    CHECK(c1 != NULL);
    CHECK(cfgfilesEqual(cfgfilesGetRecord(), c1));
    cfgfilesDestruct(c1);
    c1 = NULL;

    /* edit of a recorded file */
    CHECK(writeFile(oldInc, "# a, edited\n") == 0);
    c1 = cfgfilesCapture(cfgfilesGetRecord());
    CHECK(c1 != NULL);
    CHECK(!cfgfilesEqual(cfgfilesGetRecord(), c1));
    CHECK(writeFile(oldInc, "# a\n") == 0);
    cfgfilesDestruct(c1);
    c1 = NULL;

    /* a file newly matching the glob changes the result ... */
    CHECK(writeFile(newInc, "# b, version 1\n") == 0);
    c1 = cfgfilesCapture(cfgfilesGetRecord());
    CHECK(c1 != NULL);
    CHECK(!cfgfilesEqual(cfgfilesGetRecord(), c1));
    /* ... two captures without an edit in between are equal ... */
    c2 = cfgfilesCapture(cfgfilesGetRecord());
    CHECK(c2 != NULL);
    CHECK(cfgfilesEqual(c1, c2));
    /* ... and an edit of that new file between two captures is seen */
    CHECK(writeFile(newInc, "# b, version 2 with different length\n") == 0);
    c3 = cfgfilesCapture(cfgfilesGetRecord());
    CHECK(c3 != NULL);
    CHECK(!cfgfilesEqual(c1, c3));

    /* a literal include that did not exist at load time and now appears */
    CHECK(writeFile(litInc, "# literal\n") == 0);
    c4 = cfgfilesCapture(cfgfilesGetRecord());
    CHECK(c4 != NULL);
    CHECK(!cfgfilesEqual(c3, c4));

    /* permission-only change of a recorded file */
    CHECK(chmod(oldInc, 0600) == 0);
    c5 = cfgfilesCapture(cfgfilesGetRecord());
    CHECK(c5 != NULL);
    CHECK(!cfgfilesEqual(c4, c5));

    ret = 0;
done:
    if (fp != NULL) fclose(fp);
    cfgfilesDestruct(c1);
    cfgfilesDestruct(c2);
    cfgfilesDestruct(c3);
    cfgfilesDestruct(c4);
    cfgfilesDestruct(c5);
    unlink(litInc);
    unlink(newInc);
    unlink(oldInc);
    unlink(mainPath);
    rmdir(incDir);
    rmdir(dir);
    return ret;
}
