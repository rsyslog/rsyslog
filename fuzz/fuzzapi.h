/* SPDX-License-Identifier: Apache-2.0 */
/* fuzzapi.h
 * Runtime fixture and target entry-point API for rsyslog fuzzing.
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
#ifndef RSYSLOG_RUNTIME_FUZZAPI_H
#define RSYSLOG_RUNTIME_FUZZAPI_H

#include <stddef.h>
#include <stdint.h>

#include "rsyslog.h"

typedef struct rs_fuzz_legacy_state_s {
    int main_queue_size;
    int debug_print_template_list;
    int uid_drop_priv;
    int gid_drop_priv;
    int main_queue_filename_present;
    size_t main_queue_filename_length;
    uint64_t main_queue_filename_hash;
} rs_fuzz_legacy_state_t;

rsRetVal rs_fuzz_init(void);
void rs_fuzz_reset_iteration(void);
void rs_fuzz_dispatch_input(const uint8_t *data, size_t len);
int rs_fuzz_get_legacy_state(rs_fuzz_legacy_state_t *state);

void rs_fuzz_parse_entry(const uint8_t *data, size_t len);
void rs_fuzz_parse_rfc3164_entry(const uint8_t *data, size_t len);
void rs_fuzz_parse_rfc5424_entry(const uint8_t *data, size_t len);
void rs_fuzz_ruleset_entry(const uint8_t *data, size_t len);
void rs_fuzz_template_entry(const uint8_t *data, size_t len);
void rs_fuzz_template_json_entry(const uint8_t *data, size_t len);
void rs_fuzz_timestamp_entry(const uint8_t *data, size_t len);
void rs_fuzz_timestamp_format_entry(const uint8_t *data, size_t len);
void rs_fuzz_json_props_entry(const uint8_t *data, size_t len);
void rs_fuzz_json_accessor_entry(const uint8_t *data, size_t len);
void rs_fuzz_json_msgset_entry(const uint8_t *data, size_t len);
void rs_fuzz_structured_data_entry(const uint8_t *data, size_t len);
void rs_fuzz_template_dynamic_entry(const uint8_t *data, size_t len);
void rs_fuzz_tcp_framing_entry(const uint8_t *data, size_t len);
void rs_fuzz_queue_lifecycle_entry(const uint8_t *data, size_t len);
void rs_fuzz_imfile_line_entry(const uint8_t *data, size_t len);
void rs_fuzz_action_format_entry(const uint8_t *data, size_t len);
void rs_fuzz_cfsysline_entry(const uint8_t *data, size_t len);

#endif /* RSYSLOG_RUNTIME_FUZZAPI_H */
