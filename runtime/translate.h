/** @file translate.h
 * @brief Interfaces for canonical config translation.
 *
 * Copyright 2026 Rainer Gerhards and Adiscon GmbH.
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
#ifndef RUNTIME_TRANSLATE_H_INCLUDED
#define RUNTIME_TRANSLATE_H_INCLUDED

#include "rsyslog.h"
#include "grammar/rainerscript.h"

/** @brief Supported translation output formats. */
enum rsconfTranslateFormat { RSCONF_TRANSLATE_NONE = 0, RSCONF_TRANSLATE_RAINERSCRIPT, RSCONF_TRANSLATE_YAML };

/**
 * @brief Configure translation mode for the current validation run.
 *
 * Resets any previously retained translation state before enabling the
 * selected output format.
 *
 * @param fmt Translation output format, or @c RSCONF_TRANSLATE_NONE to disable.
 */
void rsconfTranslateConfigure(enum rsconfTranslateFormat fmt);

/**
 * @brief Release all retained translation state.
 */
void rsconfTranslateCleanup(void);

/**
 * @brief Check whether translation capture is enabled.
 *
 * @return Non-zero when config translation is active.
 */
int rsconfTranslateEnabled(void);

/**
 * @brief Check whether translation capture hit a fatal error.
 *
 * @return Non-zero when translation should fail instead of writing output.
 */
int rsconfTranslateHasFatal(void);

/**
 * @brief Clone a name-value list for later translation output.
 *
 * @param lst List head to clone.
 * @return Cloned list, or NULL on allocation failure.
 */
struct nvlst *rsconfTranslateCloneNvlst(const struct nvlst *lst);

/**
 * @brief Capture a config object before normal processing consumes it.
 *
 * @param o Parsed config object.
 * @param source Source file name, if known.
 * @param line Source line number.
 */
void rsconfTranslateCaptureObj(const struct cnfobj *o, const char *source, int line);

/**
 * @brief Capture executable script statements for later emission.
 *
 * @param script Statement tree to serialize.
 * @param source Source file name, if known.
 * @param line Source line number.
 */
void rsconfTranslateCaptureScript(const struct cnfstmt *script, const char *source, int line);

/**
 * @brief Record a fatal unsupported construct for translation.
 *
 * @param source Source file name, if known.
 * @param line Source line number.
 * @param fmt Printf-style explanation of the unsupported construct.
 */
void rsconfTranslateAddUnsupported(const char *source, int line, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

/**
 * @brief Write the translated configuration document.
 *
 * @param path Output path, or "-" for stdout.
 * @return rsyslog status code for the write operation.
 */
rsRetVal rsconfTranslateWriteFile(const char *path);

#endif
