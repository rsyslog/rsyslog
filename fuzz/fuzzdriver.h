/* SPDX-License-Identifier: Apache-2.0 */
/* fuzzdriver.h
 * Shared driver API for standalone, AFL++, and libFuzzer execution.
 *
 * Copyright 2026 Rainer Gerhards and Others
 *
 * This file is part of rsyslog.
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
#ifndef RSYSLOG_FUZZ_DRIVER_H
#define RSYSLOG_FUZZ_DRIVER_H

#include <stddef.h>
#include <stdint.h>

int rs_fuzz_driver_init(void);
int rs_fuzz_file_main(int argc, char **argv);
int LLVMFuzzerInitialize(int *argc, char ***argv);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

#endif /* RSYSLOG_FUZZ_DRIVER_H */
