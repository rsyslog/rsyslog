/* SPDX-License-Identifier: Apache-2.0 */
/* fuzzdriver.c
 * Shared initialization for the rsyslog fuzz drivers.
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
#include "config.h"

#include <stdio.h>

#include "fuzzapi.h"
#include "fuzzdriver.h"

int rs_fuzz_driver_init(void) {
    const rsRetVal ret = rs_fuzz_init();
    if (ret != RS_RET_OK) {
        fprintf(stderr, "rsyslog fuzz initialization failed: %d\n", ret);
        return -1;
    }
    return 0;
}
