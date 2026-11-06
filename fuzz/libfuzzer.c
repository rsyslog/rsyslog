/* SPDX-License-Identifier: Apache-2.0 */
/* libfuzzer.c
 * libFuzzer lifecycle adapter for the shared rsyslog fuzz dispatcher.
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
/**
 * @file libfuzzer.c
 * @brief Initializes the runtime once and dispatches each testcase through
 * the shared per-iteration reset path.
 */
#include "config.h"

#include <stddef.h>
#include <stdint.h>

#include "fuzzapi.h"
#include "fuzzdriver.h"

int LLVMFuzzerInitialize(int *argc, char ***argv) {
    (void)argc;
    (void)argv;
    return rs_fuzz_driver_init();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    rs_fuzz_dispatch_input(data, size);
    return 0;
}
