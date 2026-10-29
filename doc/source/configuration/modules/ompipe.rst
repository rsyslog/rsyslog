.. SPDX-License-Identifier: Apache-2.0

ompipe: Pipe Output Module
==========================

**Module Name:    ompipe**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

The ompipe plug-in provides the core functionality for logging output to named pipes (fifos). It is a built-in module that does not need to be loaded.

**Global Configuration Parameters:**

Note: parameter names are case-insensitive.

-  Template: [templateName] sets the default template for pipe actions that
   omit an action-specific template. If unset, ``RSYSLOG_FileFormat`` is used.
   Configure this module parameter before defining the pipe actions.

**Action specific Configuration Parameters:**

Note: parameter names are case-insensitive.

-  Pipe: string a fifo or named pipe can be used as a destination for log messages.
-  Template: [templateName] overrides the module-level template for this action.
-  tryResumeReopen: Sometimes we need to reopen a pipe after an ompipe action gets suspended. Sending a HUP signal does the job but requires an interaction with rsyslog. When set to "on" and a resume action fails, the file descriptor is closed, causing a new open in the next resume. Default: "off" to preserve existing behavior before introduction of this option.

**Caveats/Known Bugs:**
None

**Sample:**
The following command sends all syslog messages to a pipe named "NameofPipe".

::

        Module (path="builtin:ompipe")
        *.* action(type="ompipe" Pipe="NameofPipe")

**Legacy Configuration Parameters:**

rsyslog has support for logging output to named pipes (fifos). A fifo or named pipe can be used as a destination for log messages by prepending a pipe symbol ("|") to the name of the file. This is handy for debugging. Note that the fifo must be created with the mkfifo(1) command before rsyslogd is started.

**Legacy Sample:**

The following command sends all syslog messages to a pipe named /var/log/pipe.

::

        $ModLoad ompipe
        *.* |/var/log/pipe

