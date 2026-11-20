/* SPDX-License-Identifier: Apache-2.0 */

/* cfgfiles.c - record of the configuration files read at load time
 *
 * See cfgfiles.h for the purpose and the design constraints.
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
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <glob.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cfgfiles.h"

#ifndef O_CLOEXEC
    #define O_CLOEXEC 0
#endif
#ifndef O_NONBLOCK
    #define O_NONBLOCK 0
#endif
#ifndef O_NOCTTY
    #define O_NOCTTY 0
#endif

#ifdef HAVE_GLOB_NOMAGIC
    #define CFGFILES_GLOB_FLAGS (GLOB_MARK | GLOB_NOMAGIC)
#else
    #define CFGFILES_GLOB_FLAGS (GLOB_MARK)
#endif

/* files larger than this are not hashed (treated as "stat data only") */
#define CFGFILES_MAX_HASH_SIZE ((off_t)(64 * 1024 * 1024))
#define FNV64_OFFSET UINT64_C(14695981039346656037)
#define FNV64_PRIME UINT64_C(1099511628211)

typedef struct cfgfile_s {
    char *path;
    int exists; /* stat() succeeded */
    int hashed; /* regular file, hash is valid */
    dev_t dev;
    ino_t ino;
    off_t size;
    int64_t mtime_s;
    long mtime_ns;
    uint64_t hash;
} cfgfile_t;

typedef struct cfgglob_s {
    char *pattern; /* absolute, with glob metacharacters of the cwd escaped */
    char **paths; /* expansion at the time of recording */
    size_t npaths;
} cfgglob_t;

struct cfgfiles_s {
    cfgfile_t *files;
    size_t nfiles, capfiles;
    cfgglob_t *globs;
    size_t nglobs, capglobs;
    int incomplete; /* out of memory while recording: never "equal" */
};

static cfgfiles_t record; /* the load-time record */
static int bSealed = 0;


static uint64_t fnvUpdate(uint64_t h, const unsigned char *buf, size_t len) {
    size_t i;
    for (i = 0; i < len; ++i) {
        h ^= buf[i];
        h *= FNV64_PRIME;
    }
    return h;
}

static void getMtime(const struct stat *st, int64_t *sec, long *nsec) {
    *sec = (int64_t)st->st_mtime;
#if defined(__APPLE__)
    *nsec = st->st_mtimespec.tv_nsec;
#elif defined(HAVE_STRUCT_STAT_ST_MTIM) || defined(__linux__) || defined(__FreeBSD__) || defined(__sun) || \
    defined(__NetBSD__) || defined(__OpenBSD__)
    *nsec = st->st_mtim.tv_nsec;
#else
    *nsec = 0;
#endif
}

static void fillStat(cfgfile_t *f, const struct stat *st) {
    f->exists = 1;
    f->dev = st->st_dev;
    f->ino = st->st_ino;
    f->size = st->st_size;
    getMtime(st, &f->mtime_s, &f->mtime_ns);
}

static int sameStat(const cfgfile_t *a, const cfgfile_t *b) {
    return a->exists && b->exists && a->dev == b->dev && a->ino == b->ino && a->size == b->size &&
           a->mtime_s == b->mtime_s && a->mtime_ns == b->mtime_ns;
}

/* Returns an absolute version of @p path (malloc'ed) or NULL. If @p forGlob
 * is set, glob metacharacters in the added cwd prefix are escaped. */
static char *makeAbsolute(const char *path, int forGlob) {
    char cwd[PATH_MAX];
    size_t i, n = 0, plen = strlen(path);
    char *res;

    if (path[0] == '/' || getcwd(cwd, sizeof(cwd)) == NULL) {
        return strdup(path);
    }
    res = malloc(2 * strlen(cwd) + plen + 2);
    if (res == NULL) return NULL;
    for (i = 0; cwd[i] != '\0'; ++i) {
        if (forGlob && (cwd[i] == '*' || cwd[i] == '?' || cwd[i] == '[' || cwd[i] == '\\')) res[n++] = '\\';
        res[n++] = cwd[i];
    }
    if (n == 0 || res[n - 1] != '/') res[n++] = '/';
    memcpy(res + n, path, plen + 1);
    return res;
}

/* hash the whole open stream, then rewind it */
static int hashStream(FILE *fp, uint64_t *hash) {
    unsigned char buf[8192];
    size_t n;
    uint64_t h = FNV64_OFFSET;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) h = fnvUpdate(h, buf, n);
    if (ferror(fp)) {
        clearerr(fp);
        return -1;
    }
    *hash = h;
    return fseek(fp, 0, SEEK_SET);
}

/* Fill @p f from the file system. Never blocks on non-regular files. */
static void statAndHash(cfgfile_t *f) {
    struct stat st;
    int fd;
    ssize_t n;
    unsigned char buf[8192];
    uint64_t h = FNV64_OFFSET;

    f->exists = f->hashed = 0;
    if (stat(f->path, &st) != 0) return;
    fillStat(f, &st);
    if (!S_ISREG(st.st_mode)) {
        /* FIFO, device, ...: no content to compare, and size/mtime change with
         * every use. Only the identity (dev, ino) is of interest. */
        f->size = 0;
        f->mtime_s = 0;
        f->mtime_ns = 0;
        return;
    }
    if (st.st_size > CFGFILES_MAX_HASH_SIZE) return;
    fd = open(f->path, O_RDONLY | O_NONBLOCK | O_NOCTTY | O_CLOEXEC);
    if (fd < 0) return;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        return;
    }
    while ((n = read(fd, buf, sizeof(buf))) > 0) h = fnvUpdate(h, buf, (size_t)n);
    close(fd);
    if (n < 0) return;
    f->hash = h;
    f->hashed = 1;
}

static void freeGlob(cfgglob_t *g) {
    size_t i;
    for (i = 0; i < g->npaths; ++i) free(g->paths[i]);
    free(g->paths);
    free(g->pattern);
}

/* expand g->pattern into g->paths; returns 0 on success */
static int expandGlob(cfgglob_t *g) {
    glob_t gl;
    size_t i;
    int r;

    g->paths = NULL;
    g->npaths = 0;
    memset(&gl, 0, sizeof(gl));
    r = glob(g->pattern, CFGFILES_GLOB_FLAGS, NULL, &gl);
    if (r == 0 && gl.gl_pathc > 0) {
        g->paths = calloc(gl.gl_pathc, sizeof(char *));
        if (g->paths == NULL) {
            globfree(&gl);
            return -1;
        }
        for (i = 0; i < gl.gl_pathc; ++i) {
            g->paths[i] = strdup(gl.gl_pathv[i]);
            if (g->paths[i] == NULL) {
                g->npaths = i;
                globfree(&gl);
                return -1;
            }
        }
        g->npaths = gl.gl_pathc;
    }
    globfree(&gl); /* safe also after a failed glob() */
    return 0;
}

static cfgfile_t *addFile(cfgfiles_t *c, char *path) {
    if (c->nfiles == c->capfiles) {
        size_t ncap = c->capfiles ? 2 * c->capfiles : 16;
        cfgfile_t *tmp = realloc(c->files, ncap * sizeof(cfgfile_t));
        if (tmp == NULL) return NULL;
        c->files = tmp;
        c->capfiles = ncap;
    }
    memset(&c->files[c->nfiles], 0, sizeof(cfgfile_t));
    c->files[c->nfiles].path = path;
    return &c->files[c->nfiles++];
}

/* is path part of the expansion of glob number gi in c? */
static int globHasPath(const cfgfiles_t *c, size_t gi, const char *path) {
    size_t j;
    if (gi >= c->nglobs) return 0;
    for (j = 0; j < c->globs[gi].npaths; ++j)
        if (!strcmp(c->globs[gi].paths[j], path)) return 1;
    return 0;
}

static const cfgfile_t *findFile(const cfgfiles_t *c, const char *path) {
    size_t i;
    for (i = 0; i < c->nfiles; ++i)
        if (!strcmp(c->files[i].path, path)) return &c->files[i];
    return NULL;
}

static cfgglob_t *addGlob(cfgfiles_t *c, char *pattern) {
    if (c->nglobs == c->capglobs) {
        size_t ncap = c->capglobs ? 2 * c->capglobs : 8;
        cfgglob_t *tmp = realloc(c->globs, ncap * sizeof(cfgglob_t));
        if (tmp == NULL) return NULL;
        c->globs = tmp;
        c->capglobs = ncap;
    }
    memset(&c->globs[c->nglobs], 0, sizeof(cfgglob_t));
    c->globs[c->nglobs].pattern = pattern;
    return &c->globs[c->nglobs++];
}


void cfgfilesRecordStream(FILE *fp, const char *name) {
    struct stat st;
    char *path;
    cfgfile_t *f;

    if (bSealed || fp == NULL || name == NULL) return;
    if (fstat(fileno(fp), &st) != 0 || !S_ISREG(st.st_mode)) return; /* e.g. stdin */
    path = makeAbsolute(name, 0);
    if (path == NULL || (f = addFile(&record, path)) == NULL) {
        free(path);
        record.incomplete = 1;
        return;
    }
    fillStat(f, &st);
    if (hashStream(fp, &f->hash) == 0) f->hashed = 1;
}

void cfgfilesRecordGlob(const char *pattern) {
    char *abspat;
    cfgglob_t *g;

    if (bSealed || pattern == NULL) return;
    abspat = makeAbsolute(pattern, 1);
    if (abspat == NULL || (g = addGlob(&record, abspat)) == NULL) {
        free(abspat);
        record.incomplete = 1;
        return;
    }
    if (expandGlob(g) != 0) record.incomplete = 1;
}

void cfgfilesSeal(void) {
    bSealed = 1;
}

const cfgfiles_t *cfgfilesGetRecord(void) {
    return &record;
}

size_t cfgfilesFileCount(const cfgfiles_t *p) {
    return p->nfiles;
}

cfgfiles_t *cfgfilesCapture(const cfgfiles_t *base) {
    cfgfiles_t *cur;
    size_t i;

    cur = calloc(1, sizeof(*cur));
    if (cur == NULL) return NULL;
    cur->incomplete = base->incomplete;
    for (i = 0; i < base->nfiles; ++i) {
        cfgfile_t *f;
        cfgfile_t probe;
        char *path = strdup(base->files[i].path);
        if (path == NULL || (f = addFile(cur, path)) == NULL) {
            free(path);
            goto fail;
        }
        /* cheap check first: only hash if the stat data differ */
        memset(&probe, 0, sizeof(probe));
        {
            struct stat st;
            if (stat(path, &st) == 0) fillStat(&probe, &st);
        }
        if (base->files[i].hashed && sameStat(&base->files[i], &probe)) {
            *f = base->files[i];
            f->path = path;
        } else {
            statAndHash(f);
        }
    }
    for (i = 0; i < base->nglobs; ++i) {
        cfgglob_t *g;
        char *pattern = strdup(base->globs[i].pattern);
        if (pattern == NULL || (g = addGlob(cur, pattern)) == NULL) {
            free(pattern);
            goto fail;
        }
        if (expandGlob(g) != 0) goto fail;
    }
    /* Files that newly match an include glob are not in the load-time record.
     * Snapshot their content too, so that an edit to such a file between two
     * captures is seen (e.g. a HUP coalesced while a check runs). Files these
     * new files would include in turn are not followed; changing an include
     * statement changes the including file's hash anyway. */
    for (i = 0; i < cur->nglobs; ++i) {
        size_t j;
        for (j = 0; j < cur->globs[i].npaths; ++j) {
            cfgfile_t *f;
            char *path;
            const char *const gp = cur->globs[i].paths[j];
            const size_t gl = strlen(gp);
            /* only paths that are new since the load: everything matched at
             * load time is already covered by the record (or was skipped by
             * the loader, e.g. a directory) */
            if (globHasPath(base, i, gp) || findFile(cur, gp) != NULL) continue;
            if (gl > 0 && gp[gl - 1] == '/') continue; /* GLOB_MARK: directory */
            path = strdup(gp);
            if (path == NULL || (f = addFile(cur, path)) == NULL) {
                free(path);
                goto fail;
            }
            statAndHash(f);
        }
    }
    return cur;
fail:
    cfgfilesDestruct(cur);
    return NULL;
}

int cfgfilesEqual(const cfgfiles_t *a, const cfgfiles_t *b) {
    size_t i, j;

    if (a->incomplete || b->incomplete) return 0;
    if (a->nfiles != b->nfiles || a->nglobs != b->nglobs) return 0;
    for (i = 0; i < a->nglobs; ++i) {
        if (a->globs[i].npaths != b->globs[i].npaths) return 0;
        for (j = 0; j < a->globs[i].npaths; ++j)
            if (strcmp(a->globs[i].paths[j], b->globs[i].paths[j]) != 0) return 0;
    }
    for (i = 0; i < a->nfiles; ++i) {
        const cfgfile_t *x = &a->files[i], *y = &b->files[i];
        if (x->exists != y->exists || x->hashed != y->hashed) return 0;
        if (x->hashed && (x->hash != y->hash || x->size != y->size)) return 0;
        /* unhashable (not regular, too large): fall back to stat data */
        if (x->exists && !x->hashed && !sameStat(x, y)) return 0;
    }
    return 1;
}

void cfgfilesDestruct(cfgfiles_t *p) {
    size_t i;
    if (p == NULL) return;
    for (i = 0; i < p->nfiles; ++i) free(p->files[i].path);
    free(p->files);
    for (i = 0; i < p->nglobs; ++i) freeGlob(&p->globs[i]);
    free(p->globs);
    free(p);
}
