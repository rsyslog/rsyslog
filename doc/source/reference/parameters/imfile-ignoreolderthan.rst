.. _param-imfile-ignoreolderthan:
.. _imfile.parameter.input.ignoreolderthan:
.. _imfile.parameter.ignoreolderthan:

ignoreOlderThan
===============

.. index::
   single: imfile; ignoreOlderThan
   single: ignoreOlderThan

.. summary-start

Ignores newly discovered files older than the specified number of seconds.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: ignoreOlderThan
:Scope: input
:Type: integer
:Default: 0
:Required?: no
:Introduced: 8.2108.0

Description
-----------
Instructs imfile to ignore a discovered file that has not been modified in
the given number of seconds. Once the file is discovered, it is no longer
ignored and new data will be read. The default ``0`` disables this option.

Input usage
-----------
.. _param-imfile-input-ignoreolderthan:
.. _imfile.parameter.input.ignoreolderthan-usage:

.. code-block:: rsyslog

   input(type="imfile" ignoreOlderThan="0")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
