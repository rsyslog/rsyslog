/* This test checks runtime initialization and exit. Other than that, it
 * also serves as the most simplistic sample of how a test can be coded.
 *
 * Part of the testbench for rsyslog.
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#include "config.h"
#include "rsyslog.h"
#include "testbench.h"
#include <stdio.h>	/* must be last, else we get a zlib compile error on some platforms */

rsconf_t *ourConf;
MODULE_TYPE_TESTBENCH

BEGINInit
CODESTARTInit
ENDInit

BEGINExit
CODESTARTExit
ENDExit

BEGINTest
CODESTARTTest
/*finalize_it:*/
	/* room for custom error reporter, leave blank if not needed */
ENDTest
