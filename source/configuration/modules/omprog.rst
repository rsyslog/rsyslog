Program integration Output module
=================================

**Module Name:    omprog**

**Available since:   ** 4.3.0

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

This module permits to integrate arbitrary external programs into
rsyslog's logging. It is similar to the "execute program (^)" action,
but offers better security and much higher performance. While "execute
program (^)" can be a useful tool for executing programs if rare events
occur, omprog can be used to provide massive amounts of log data to a
program.

Executes the configured program and feeds log messages to that binary
via stdin. The binary is free to do whatever it wants with the supplied
data. If the program terminates, it is re-started. If rsyslog
terminates, the program's stdin will see EOF. The program must than
terminate. The message format passed to the program can, as usual, be
modified by defining rsyslog templates.

Note that each time an omprog action is defined, the corresponding
programm is invoked. A single instance is **not** being re-used. There
are arguments pro and con re-using existing binaries. For the time
being, it simply is not done. In the future, we may add an option for
such pooling, provided that some demand for that is voiced. You can also
mimic the same effect by defining multiple rulesets and including them
(at the price of some slight performance loss).

**Configuration Directives**:

-  **$ActionOMProgBinary** <binary>
    The binary program to be executed.

**Caveats/Known Bugs:**

Currently none known.

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2008-2011 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
