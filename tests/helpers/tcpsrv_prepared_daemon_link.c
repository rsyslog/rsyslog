/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Copyright 2026 Rainer Gerhards and Adiscon GmbH.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 */

/* Supply the normal daemon symbols needed by librsyslog and RTLD_NOW-loaded
 * network modules without replacing any production callback with a stub.
 * Only the daemon entry point is renamed; the integration test owns main(). */
int prepared_backend_daemon_main(int argc, char **argv);
#define main prepared_backend_daemon_main
#include "../../tools/rsyslogd.c"
#undef main
