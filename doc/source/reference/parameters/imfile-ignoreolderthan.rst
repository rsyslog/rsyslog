.. _param-imfile-ignoreolderthan:
.. _imfile.parameter.module.ignoreolderthan:

ignoreOlderThan
===============

.. index::
   single: imfile; ignoreOlderThan
   single: ignoreOlderThan

.. summary-start

Instructs imfile to ignore discovered files that have not been modified within a specified time. Once a file is being monitored, it is no longer ignored.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: ignoreOlderThan
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: 8.2108.0

Description
-----------
.. versionadded:: 8.2108.0

Instructs imfile to ignore a discovered file that has not been modified in the
specified number of seconds. Once a file is discovered, the file is no longer
ignored and new data will be read. This option is disabled (set to 0) by default.



Input usage
-----------
.. _param-imfile-input-ignoreolderthan:
.. _imfile.parameter.input.ignoreolderthan:
.. code-block:: rsyslog

   input(type="imfile" ignoreOlderThan="...")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
