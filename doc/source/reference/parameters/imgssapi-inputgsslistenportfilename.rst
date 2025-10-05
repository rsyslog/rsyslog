.. _param-imgssapi-inputgsslistenportfilename:
.. _imgssapi.parameter.input.inputgsslistenportfilename:

InputGSSListenPortFileName
==========================

.. index::
   single: imgssapi; InputGSSListenPortFileName
   single: InputGSSListenPortFileName

.. summary-start

Writes the listener's bound port number to the specified file.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imgssapi`.

:Name: InputGSSListenPortFileName
:Scope: input
:Type: word
:Default: input=none
:Required?: no
:Introduced: 8.38.0

Description
-----------
.. versionadded:: 8.38.0

Specifies the path to a file where the listener will write its bound port
number upon startup. This is useful for integration with other services or in
test environments that use dynamic ports.

.. note::

   If this parameter is set, a port of ``0`` for ``InputGSSServerRun`` is
   accepted. Otherwise, a port of ``0`` defaults to ``514``.

Input usage
-----------
.. _param-imgssapi-input-inputgsslistenportfilename:
.. _imgssapi.parameter.input.inputgsslistenportfilename-usage:

.. code-block:: rsyslog

   module(load="imgssapi")
   $InputGSSListenPortFileName "/var/run/rsyslog-gss-port"

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imgssapi.parameter.legacy.inputgsslistenportfilename:

- $InputGSSListenPortFileName — maps to InputGSSListenPortFileName (status: legacy)

.. index::
   single: imgssapi; $InputGSSListenPortFileName
   single: $InputGSSListenPortFileName

See also
--------
See also :doc:`../../configuration/modules/imgssapi`.
