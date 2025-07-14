#! /usr/bin/env python
# -*- coding: utf-8 -*-

#   * Copyright (C) 2013-2018 Adiscon GmbH.
#   * This file is part of RSyslog
#   *
#   * Helper script which includes REGEX definitions
#   *

import re

# Create regex for loglines
loglineregexes = []
loglineindexes = []

# Traditional Format
# Sample Line:      Jun 26 14:21:44 nhpljt084 rsyslogd-pstats: main Q[DA]: size=0 enqueued=0 full=0 discarded.full=0 discarded.nf=0 maxqsize=0
loglineregexes.append( re.compile(r"(...)(?:.|..)([0-9]{1,2}) ([0-9]{1,2}:[0-9]{1,2}:[0-9]{1,2}) ([a-zA-Z0-9_\-\.]{1,256}) ([A-Za-z0-9_\-\/\.]{1,32}): (.*?): (.*?) \n") )
loglineindexes.append( {"LN_YEAR":-1, "LN_MONTH":1, "LN_DAY":2, "LN_TIME":3, "LN_HOST":4, "LN_SYSLOGTAG":5, "LN_LOGOBJECT":6, "LN_LOGDATA":7} )

# log.file Format (out-of-band logfiles)
# Sample Line:      Wed Nov 20 10:38:20 2013: action 1: processed=21881 failed=0
loglineregexes.append( re.compile(r"(...) (...)(?:.|..)([0-9]{1,2}) ([0-9]{1,2}:[0-9]{1,2}:[0-9]{1,2}) ([0-9]{4,4}): (.*?): (.*?)") )
loglineindexes.append( {"LN_SYSLOGTAG":-1, "LN_HOST":-1, "LN_MONTH":2, "LN_DAY":3, "LN_TIME":4, "LN_YEAR":5, "LN_LOGOBJECT":6, "LN_LOGDATA":8} )

# Newer Format
# Sample format:    2013-07-03T17:22:55.680078+02:00 devdebian6 rsyslogd-pstats: main Q: size=358 enqueued=358 full=0 discarded.full=0 discarded.nf=0 maxqsize=358
loglineregexes.append( re.compile(r"([0-9]{4,4})-([0-9]{1,2})-([0-9]{1,2})T([0-9]{1,2}:[0-9]{1,2}:[0-9]{1,2})\.[0-9]{1,6}.[0-9]{1,2}:[0-9]{1,2} ([a-zA-Z0-9_\-\.]{1,256}) ([A-Za-z0-9_\-\/\.]{1,32}): (.*?): (.*?) \n") )
loglineindexes.append( {"LN_YEAR":1, "LN_MONTH":2, "LN_DAY":3, "LN_TIME":4, "LN_HOST":5, "LN_SYSLOGTAG":6, "LN_LOGOBJECT":7, "LN_LOGDATA":8} )

# Format
# Json format:      2013-12-10T20:01:44.800512+00:00 stha8p00g rsyslogd-pstats: {"name":"LB-Ciscofw-prod","size":0,"enqueued":728022,"full":0,"discarded.full":0,"discarded.nf":0,"maxqsize":7298}
loglineregexes.append( re.compile(r"([0-9]{4,4})-([0-9]{1,2})-([0-9]{1,2})T([0-9]{1,2}:[0-9]{1,2}:[0-9]{1,2})\.[0-9]{1,6}.[0-9]{1,2}:[0-9]{1,2} ([a-zA-Z0-9_\-\.]{1,256}) ([A-Za-z0-9_\-\/\.]{1,32}): (.*?)\n") )
loglineindexes.append( {"LN_YEAR":1, "LN_MONTH":2, "LN_DAY":3, "LN_TIME":4, "LN_HOST":5, "LN_SYSLOGTAG":6, "LN_LOGOBJECT":-1, "LN_LOGDATA":7} )

