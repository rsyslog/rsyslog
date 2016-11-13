omprog: Program integration Output module
=========================================

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
terminates, the program's stdin will see EOF. The program must then
terminate. The message format passed to the program can, as usual, be
modified by defining rsyslog templates.

Note that each time an omprog action is defined, the corresponding
program is invoked. A single instance is **not** being re-used. There
are arguments pro and con for re-using existing binaries. For the time
being, it simply is not done. In the future, we may add an option for
such pooling, provided that some demand for that is voiced. You can also
mimic the same effect by defining multiple rulesets and including them.

Note that in order to execute the given program, rsyslog needs to have
sufficient permissions on the binary file. This is especially true if
not running as root. Also, keep in mind that default SELinux policies
most probably do not permit rsyslogd to execute arbitrary binaries. As
such, permissions must be appropriately added. Note that SELinux
restrictions also apply if rsyslogd runs under root. To check if a
problem is SELinux-related, you can temporarily disable SELinux and
retry. If it then works, you know for sure you have a SELinux issue.

Starting with 8.4.0, rsyslogd emits an error message via the ``syslog()``
API call when there is a problem executing the binary. This can be
extremely valuable in troubleshooting. For those technically savy:
when we execute a binary, we need to fork, and we do not have
full access to rsyslog's usual error-reporting capabilities after the
fork. As the actual execution must happen after the fork, we cannot
use the default error logger to emit the error message. As such,
we use ``syslog()``. In most cases, there is no real difference 
between both methods. However, if you run multiple rsyslog instances,
the messae shows up in that instance that processes the default
log socket, which may be different from the one where the error occured.
Also, if you redirected the log destination, that redirection may
not work as expected.
 

**Module Parameters**:

-  **Template**\ [templateName]
    sets a new default template for file actions.

 

**Action Parameters**:

-  **binary**
   Mostly equivalent to the "binary" action parameter, but must contain
   the binary name only. In legacy config, it is **not possible** to
   specify command line parameters.

-  **hup.signal** [v8.9.0+]
   Specifies which signal, if any, is to be forwarded to the executed program.
   Currently, HUP, USR1, USR2, INT, and TERM are supported. If unset, no signal
   is sent on HUP. This is the default and what pre 8.9.0 versions did.

-  **signalOnClose** [switch] [v8.23.0+]
   *Default: off*

   Signal the child process when worker-instance is stopped or Rsyslog is about
   to shutdown. To signal shutdown, SIGTERM is issued to child and Rsyslog
   reaps the process before proceeding.

   No signal is issued if this switch is set to 'off' (default). The child-process
   can still detect shutdown because 'read' from stdin would EOF. However its
   possible to have process-leak due to careless error-handling around read.
   Rsyslog won't try to reap the child process in this case.

   Additionaly, this controls the following **GNU/Linux specific behavior**:
   If 'on', Rsyslog waits for upto 5 seconds for child process to terminate
   after issuing SIGTERM, after which a SIGKILL is issued ensuring child-death.
   This ensures even an unresponsive child is reaped before shutdown.

**Caveats/Known Bugs:**

-  None.

**Sample:**

The following command writes all syslog messages into a file.

::

  module(load="omprog")
  action(type="omprog"
         binary="/pathto/omprog.py --parm1=\"value 1\" --parm2=\"value2\""
         template="RSYSLOG_TraditionalFileFormat")

**Legacy Configuration Directives**:

-  **$ActionOMProgBinary** <binary>
   The binary program to be executed.

**Caveats/Known Bugs:**

Currently none known.

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2008-2014 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL version
3 or higher.
