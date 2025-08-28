.. _param-imfile-freshstarttail:
.. _imfile.parameter.input.freshstarttail:
.. _imfile.parameter.freshstarttail:

freshStartTail
==============

.. index::
   single: imfile; freshStartTail
   single: freshStartTail

.. summary-start

On first start, seeks to the end of existing files and processes only new lines.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: freshStartTail
:Scope: input
:Type: boolean
:Default: off
:Required?: no
:Introduced: at least 8.x, possibly earlier

Description
-----------
Causes rsyslog to seek to the end of input files at the initial start and
process only new log messages. Useful when deploying rsyslog and only new
logs are of interest. This parameter applies only to files that exist when
rsyslog performs its initial processing of file monitors.

.. warning::

   Depending on the number and location of existing files, startup may take
   time. If another process creates a new file exactly during startup and
   writes data to it, rsyslog may treat that file as preexisting and skip
   it. The same risk exists for log data written between the last shutdown
   and the next start. Some data loss is therefore possible. The rsyslog
   team advises against activating ``freshStartTail``.

Input usage
-----------
.. _param-imfile-input-freshstarttail:
.. _imfile.parameter.input.freshstarttail-usage:

.. code-block:: rsyslog

   input(type="imfile"
         File="/var/log/example.log"
         Tag="example"
         freshStartTail="on")

Notes
-----
- Legacy documentation used the term ``binary`` for the type. It is treated as boolean.

See also
--------
See also :doc:`../../configuration/modules/imfile`.
