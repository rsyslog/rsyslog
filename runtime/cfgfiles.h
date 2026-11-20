/* SPDX-License-Identifier: Apache-2.0 */

/* cfgfiles.h - record of the configuration files read at load time
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
#ifndef INCLUDED_CFGFILES_H
#define INCLUDED_CFGFILES_H

#include <stdio.h>

/**
 * @file cfgfiles.h
 * @brief Record of config files (RainerScript and YAML) and include globs.
 *
 * While the configuration is loaded, the lexer and the YAML loader report
 * every file they open and every include() glob pattern they expand. For
 * each file we keep device, inode, size, mtime (ns) and a 64-bit FNV-1a
 * content hash. The record is taken on every start; it is cheap and does not
 * change any output. It is used to answer "did the config on disk change
 * since it was loaded?" (see reload-preflight.c).
 *
 * All paths are stored as absolute paths, because rsyslogd changes its
 * working directory to "/" after loading the configuration.
 *
 * The FNV hash is a change detector, not a security boundary: whoever can
 * edit the config already controls the daemon.
 */
typedef struct cfgfiles_s cfgfiles_t;

/** Record an opened config file (load time; ignored once sealed). */
void cfgfilesRecordStream(FILE *fp, const char *name);
/** Record an include glob pattern and its current expansion. */
void cfgfilesRecordGlob(const char *pattern);
/** End of load phase: later record calls are ignored. */
void cfgfilesSeal(void);
/** The record taken at load time (may be empty, never NULL). */
const cfgfiles_t *cfgfilesGetRecord(void);

/** Look at the files on disk now. Hashes only files whose stat data differ
 * from @p base. Returns NULL on out-of-memory. */
cfgfiles_t *cfgfilesCapture(const cfgfiles_t *base);
/** Returns 1 if both states are identical, 0 if not. An incomplete record
 * (out-of-memory while recording) never compares equal. */
int cfgfilesEqual(const cfgfiles_t *a, const cfgfiles_t *b);
void cfgfilesDestruct(cfgfiles_t *p);

/** Number of recorded files (for tests/debug output). */
size_t cfgfilesFileCount(const cfgfiles_t *p);

#endif /* #ifndef INCLUDED_CFGFILES_H */
