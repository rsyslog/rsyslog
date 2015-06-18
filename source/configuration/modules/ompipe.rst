ompipe: Pipe Output Module
==========================

**Module Name:    ompipe**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Description**:

The ompipe plug-in provides the core functionality for logging output to named pipes (fifos). It is a built-in module that does not need to be loaded.
**Global Configuration Directives:**

-  Template: [templateName] sets a new default template for file actions.

**Action specific Configuration Directives:**

-  Pipe: string a fifo or named pipe can be used as a destination for log messages. 

**Caveats/Known Bugs:**
None

**Sample:**
The following command sends all syslog messages to a remote server via TCP port 10514.

::

        Module (path="builtin:ompipe") 
        *.* action(type="ompipe"
        Pipe="NameofPipe")
    
**Legacy Configuration Directives:**

rsyslog has support for logging output to named pipes (fifos). A fifo or named pipe can be used as a destination for log messages by prepending a pipe symbol ("|") to the name of the file. This is handy for debugging. Note that the fifo must be created with the mkfifo(1) command before rsyslogd is started. 

**Legacy Sample:**

The following command sends all syslog messages to a remote server via TCP port 10514.

::

        $ModLoad ompipe 
        *.* |/var/log/pipe 
    
This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2008-2014 by `Rainer
Gerhards <http://www.gerhards.net/rainer>`_ and
`Adiscon <http://www.adiscon.com/>`_. Released under the ASL 2.0.
