/* yamlconf.h - YAML configuration file loader for rsyslog
 *
 * Provides yamlconf_load(), which parses a .yaml/.yml config file and
 * populates the rsyslog loadConf object by building the same cnfobj/nvlst
 * structures the RainerScript bison parser produces, then routing them
 * through the existing cnfDoObj() / cnfDoScript() dispatcher.
 *
 * Compilation is conditional on HAVE_LIBYAML (configure.ac detects yaml-0.1
 * via pkg-config). The caller in rsconf.c is responsible for the #ifdef guard.
 *
 * Copyright 2025 Rainer Gerhards and Adiscon GmbH.
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
#ifndef YAMLCONF_H_INCLUDED
#define YAMLCONF_H_INCLUDED

#include "rsyslog.h"

#ifdef HAVE_LIBYAML

/* Load a rsyslog configuration from a YAML file.
 *
 * fname   - path to the .yaml / .yml config file
 *
 * Returns RS_RET_OK on success, RS_RET_CONF_FILE_NOT_FOUND if the file cannot
 * be opened, RS_RET_CONF_PARSE_ERROR on YAML or semantic errors.  On error
 * LogError() is called with a descriptive message before returning.
 *
 * This function populates the global loadConf object (runtime/rsconf.c) by
 * calling cnfDoObj() / cnfDoScript() / cnfAddConfigBuffer() exactly as the
 * RainerScript yyparse() path does.  The caller (rsconf.c::load()) must have
 * already called rsconfConstruct() before invoking this function.
 */
rsRetVal yamlconf_load(const char *fname);

#endif /* HAVE_LIBYAML */

#endif /* YAMLCONF_H_INCLUDED */
