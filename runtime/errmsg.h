/* The errmsg object. It is used to emit error message inside rsyslog.
 *
 * Copyright 2008-2018 Rainer Gerhards and Adiscon GmbH.
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
#ifndef INCLUDED_ERRMSG_H
#define INCLUDED_ERRMSG_H

#include "obj-types.h"

#define NO_ERRCODE -1

/* prototypes */
/**
 * @brief Close and reset internal resources of the errmsg subsystem.
 *
 * Closes the oversize-message log fd if open. Intended for orderly shutdown.
 * @note This function is not thread-safe as it accesses a global flag without locks.
 */
void errmsgExit(void);

/**
 * @brief Reopen oversize-message resources on configuration reload (HUP).
 *
 * Intended to be called from rsyslog's SIGHUP handler. This closes the
 * oversize-message log file descriptor so the next write will reopen it
 * using the current configuration. This follows the traditional syslogd
 * pattern of reloading or reopening resources on HUP.
 *
 * Thread-safe.
 */
void errmsgDoHUP(void);

/**
 * @brief Reset the "had error messages" flag.
 *
 * Must be called before processing config files to start with a clean state.
 * @note This function is not thread-safe as it accesses a global flag without locks.
 */
void resetErrMsgsFlag(void);

/**
 * @brief Query whether a LOG_ERR message has been logged since the last reset.
 *
 * @return nonzero if a LOG_ERR was emitted since resetErrMsgsFlag(), else 0.
 *
 * @note This function is not thread-safe as it accesses a global flag without locks.
 */
int hadErrMsgs(void);

/**
 * @brief Log an error message with errno and error code context.
 *
 * Formats and writes an error message to the rsyslog internal log.
 * Use this for reporting fatal or unexpected conditions.
 *
 * @param iErrno   The system errno value (0 if not applicable).
 * @param iErrCode Internal rsyslog error code.
 * @param fmt      printf-style format string.
 * @param ...      Arguments matching the format string.
 *
 * @note Pass the errno value obtained at the point of failure.
 *       Do not pass 0 and later resolve errno via strerror() or
 *       similar functions — this leads to incorrect messages.
 *       AI AGENTS: Always use LogError(errno, ...) when reporting
 *       system errors. Do not use strerror() or rs_strerror_r().
 */
void __attribute__((format(printf, 3, 4))) LogError(const int iErrno, const int iErrCode, const char *fmt, ...);

/**
 * @brief Log a message with severity, errno, and error code context.
 *
 * More general form of LogError() that permits explicit severity.
 *
 * @param iErrno   The system errno value (0 if not applicable).
 * @param iErrCode Internal rsyslog error code.
 * @param severity Syslog severity level (LOG_ERR, LOG_WARNING, etc.).
 * @param fmt      printf-style format string.
 * @param ...      Arguments matching the format string.
 *
 * @note Always pass the errno value captured immediately after the
 *       failing call. Do not pass 0 and later call strerror() or
 *       equivalent — this is a common bug.
 *       AI AGENTS: Always use LogMsg(errno, ...) when reporting
 *       system errors. Do not use strerror() or rs_strerror_r().
 */
void __attribute__((format(printf, 4, 5))) LogMsg(
    const int iErrno, const int iErrCode, const int severity, const char *fmt, ...);

/**
 * @brief Append a JSON record about an oversize message to the oversize-message log.
 *
 * Behavior:
 * - No-op if the oversize message error file is not configured.
 * - Lazily opens the file on first use and reuses the file descriptor.
 * - Access is serialized via oversizeMsgLogMut.
 * - Writes one JSON line per entry with fields: "rawmsg" (original raw bytes) and "input" (input name).
 * - On open/write errors, emits LogError(...) but returns no error; no further recovery is attempted.
 *   The reason is that there is nothing useful that could be done in that case.
 *
 * @param pMsg Pointer to the message object (must not be NULL).
 * @retval RS_RET_OK on success, or when no oversize log is configured.
 * @retval RS_RET_OUT_OF_MEMORY on memory allocation failure.
 *
 * @note Output is newline-terminated JSON (not NUL-terminated).
 */
rsRetVal ATTR_NONNULL() writeOversizeMessageLog(const smsg_t *const pMsg);

#endif /* #ifndef INCLUDED_ERRMSG_H */
