`back <rsyslog_conf_global.html>`_

$FileCreateMode
---------------

**Type:** global configuration directive

**Default:** 0644

**Description:**

The $FileCreateMode directive allows to specify the creation mode with
which rsyslogd creates new files. If not specified, the value 0644 is
used (which retains backward-compatibility with earlier releases). The
value given must always be a 4-digit octal number, with the initial
digit being zero.

Please note that the actual permission depend on rsyslogd's process
umask. If in doubt, use "$umask 0000" right at the beginning of the
configuration file to remove any restrictions.

$FileCreateMode may be specified multiple times. If so, it specifies the
creation mode for all selector lines that follow until the next
$FileCreateMode directive. Order of lines is vitally important.

**Sample:**

``$FileCreateMode 0600``

This sample lets rsyslog create files with read and write access only
for the users it runs under.

The following sample is deemed to be a complete rsyslog.conf:

``$umask 0000 # make sure nothing interferes with the following  definitions *.* /var/log/file-with-0644-default $FileCreateMode 0600 *.* /var/log/file-with-0600 $FileCreateMode 0644 *.* /var/log/file-with-0644``

As you can see, open modes depend on position in the config file. Note
the first line, which is created with the hardcoded default creation
mode.

[`rsyslog.conf overview <rsyslog_conf.html>`_\ ] [`manual
index <manual.html>`_\ ] [`rsyslog site <http://www.rsyslog.com/>`_\ ]

This documentation is part of the `rsyslog <http://www.rsyslog.com/>`_
project.
Copyright © 2007 by `Rainer Gerhards <http://www.gerhards.net/rainer>`_
and `Adiscon <http://www.adiscon.com/>`_. Released under the GNU GPL
version 3 or higher.
