.. _param-imfile-freshstarttail:
.. _imfile.parameter.module.freshstarttail:

freshStartTail
==============

.. index::
   single: imfile; freshStartTail
   single: freshStartTail

.. summary-start

This is used to tell rsyslog to seek to the end/tail of input files (discard old logs) **at its first start(freshStart)** and process only new log messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: freshStartTail
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
This is used to tell rsyslog to seek to the end/tail of input files
(discard old logs) **at its first start(freshStart)** and process only new
log messages.

When deploy rsyslog to a large number of servers, we may only care about
new log messages generated after the deployment. set **freshstartTail**
to **on** will discard old logs. Otherwise, there may be vast useless
message burst on the remote central log receiver

This parameter only applies to files that are already existing during
rsyslog's initial processing of the file monitors.

.. warning::

   Depending on the number and location of existing files, this initial
   startup processing may take some time as well. If another process
   creates a new file at exactly the time of startup processing and writes
   data to it, rsyslog might detect this file and it's data as preexisting
   and may skip it. This race is inevitable. So when freshStartTail is used,
   some risk of data loss exists. The same holds true if between the last
   shutdown of rsyslog and its restart log file content has been added.
   As such, the rsyslog team advises against activating the freshStartTail
   option.

Input usage
-----------
.. _param-imfile-input-freshstarttail:
.. _imfile.parameter.input.freshstarttail:
.. code-block:: rsyslog

   input(type="imfile" freshStartTail="...")

Notes
-----
- Legacy docs describe this as a binary option; it is a boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
