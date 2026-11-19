/* a testcase generator
 * THis program reads stdin, which must consist of (name,stmt) tupels
 * where name is a part of the config name  (small!) and stmt is an actual
 * config statement. These tupels must be encoded as
 * name<SP>stmt<LF>
 * on stdin. After all tupels are read, the power set of all possible
 * configurations is generated.
 * Copyright (C) 2011 by Rainer Gerhards and Adiscon GmbH
 * Released under the GPLv3 as part of the rsyslog project.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int arr[128];
static char *name[128];
static char *stmt[128];

void output(int n) {
    int i;

    printf("name:");
    for (i = 0; i < n; ++i) {
        if (arr[i]) {
            printf("-%s", name[i]);
        }
    }
    printf("\n");
}

void pows(int n, int i) {
    if (i == 0) {
        output(n);
    } else {
        --i;
        arr[i] = 0;
        pows(n, i);
        arr[i] = 1;
        pows(n, i);
    }
}


int main(int argc, char *argv[]) {
    int n;
    int ret = 0;
    char iname[512];
    char istmt[2048];
    int nscanned;

    (void)argc;
    (void)argv;
    n = 0;
    while (!feof(stdin)) {
        nscanned = scanf("%s %[^\n]s\n", iname, istmt);
        if (nscanned == EOF)
            break;
        else if (nscanned != 2) {
            fprintf(stderr, "problem scanning entry %d, scanned %d\n", n, nscanned);
            ret = 1;
            break;
        }
        if (n >= (int)(sizeof(name) / sizeof(name[0]))) {
            fprintf(stderr, "too many entries (maximum %zu)\n", sizeof(name) / sizeof(name[0]));
            ret = 1;
            break;
        }
        name[n] = strdup(iname);
        stmt[n] = strdup(istmt);
        if (name[n] == NULL || stmt[n] == NULL) {
            fprintf(stderr, "out of memory while storing entry %d\n", n);
            ret = 1;
            break;
        }
        n++;
        printf("name: %s, stmt: %s\n", iname, istmt);
    }
    /* n is on to high for an index, but just right as the actual number! */

    if (ret == 0) {
        printf("read %d entries\n", n);
        pows(n, n);
    }
    for (size_t i = 0; i < sizeof(name) / sizeof(name[0]); ++i) {
        free(name[i]);
        free(stmt[i]);
    }
    return ret;
}
