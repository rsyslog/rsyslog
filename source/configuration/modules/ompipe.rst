ompipe: Pipe Output Module
==========================

**Module Name:    ompipe**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

The ompipe plug-in provides the core functionality for logging output to named pipes (fifos). It is a built-in module that does not need to be loaded.

**Global Configuration Parameters:**

Note: parameter names are case-insensitive.

-  Template: [templateName] sets a new default template for file actions.

**Action specific Configuration Parameters:**

Note: parameter names are case-insensitive.

-  Pipe: string a fifo or named pipe can be used as a destination for log messages.

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

