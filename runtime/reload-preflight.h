/* SPDX-License-Identifier: Apache-2.0 */

/* reload-preflight.h - config reload "report" mode (pre-flight child)
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
#ifndef INCLUDED_RELOAD_PREFLIGHT_H
#define INCLUDED_RELOAD_PREFLIGHT_H

#include <stdint.h>
#include <sys/types.h>

/**
 * @file reload-preflight.h
 * @brief global(config.reloadOnHUP="report"): validate a changed config on HUP.
 *
 * On HUP the main loop compares the config files on disk with the record
 * taken at load time (cfgfiles.h). If they differ, the same binary is run as
 * a child with the original config options plus "-N1". The exit status is the
 * verdict. Nothing is applied: the outcome is logged as one line.
 *
 * Everything runs on the main thread and never blocks: the child's output
 * pipe is drained non-blocking on every main loop wake-up, the main loop
 * timeout is capped while a child runs, and the child is killed after a
 * deadline (default 30 s).
 */

/** Remember how to find our own binary. Call first thing in main(). */
void reloadPreflightInitExe(const char *argv0);
/** Remember one command line option that is replayed in the child. */
void reloadPreflightNoteOpt(int opt, const char *optarg);
/** Remember the current working directory (call right before loading the config). */
void reloadPreflightNoteCwd(void);
/** Set up statistics; call after the config was activated. */
void reloadPreflightInit(void);
/** To be called by the main loop after doHUP(). */
void reloadPreflightOnHUP(void);
/** To be called by the main loop on every wake-up. */
void reloadPreflightPoll(uint64_t now_ms);
/** Cap the main loop timeout while a child runs. */
int reloadPreflightCapTimeoutMs(int timeout_ms);
/** reapChild() hook: returns 1 if @p pid was our child (status consumed). */
int reloadPreflightReapChild(pid_t pid, int status);
/** Kill and reap a running child; release resources. */
void reloadPreflightShutdown(void);
/** Non-zero in the validation child (it must not touch live state files). */
int reloadPreflightIsChild(void);

#define RELOAD_PREFLIGHT_CHILD_ENV "RSYSLOG_RELOAD_PREFLIGHT_CHILD"

#endif /* #ifndef INCLUDED_RELOAD_PREFLIGHT_H */
