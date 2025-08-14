.. _param-imtcp-listenportfilename:
.. _imtcp.parameter.input.listenportfilename:

ListenPortFileName
==================

.. index::
   single: imtcp; ListenPortFileName
   single: ListenPortFileName

.. summary-start

Writes the dynamically assigned listener port number to a specified file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: ListenPortFileName
:Scope: input
:Type: string
:Default: none
:Required?: no
:Introduced: 8.38.0

Description
-----------
This parameter specifies a file name into which the actual port number the input is listening on is written. It is primarily intended for cases when the ``port`` parameter is set to ``0``, which lets the operating system automatically assign a free port number.

If this parameter is set, a ``port`` value of ``0`` is accepted. If it is not set and ``port`` is ``0``, the port will default to 514.

Input usage
-----------
.. _imtcp.parameter.input.listenportfilename-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="0" listenPortFileName="/var/run/rsyslog-imtcp.port")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

This parameter has no legacy name.

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
