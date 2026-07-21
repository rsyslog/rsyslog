.. _param-imtcp-maxsessions:
.. _imtcp.parameter.module.maxsessions:
.. _imtcp.parameter.input.maxsessions:

MaxSessions
===========

.. index::
   single: imtcp; MaxSessions
   single: MaxSessions

.. summary-start

Sets the maximum number of sessions supported.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: MaxSessions
:Scope: module, input
:Type: integer
:Default: module=200, input=module parameter
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Sets the maximum number of sessions supported. This must be set
before the first $InputTCPServerRun directive.

The same-named input parameter can override this module setting.

Each session owns its framing, transport, TLS, and optional decompressor state.
Consequently, unusually large values multiply the memory retained by idle or
incomplete connections.  A zlib ``stream:always`` session has a bounded history
window of at most 32 KiB plus decoder bookkeeping; completed zlib streams release
that decoder state immediately, but incomplete streams retain it until the
connection closes.  Size ``MaxSessions`` for the listener's expected concurrency
and available memory.


Module usage
------------
.. _param-imtcp-module-maxsessions:
.. _imtcp.parameter.module.maxsessions-usage:

.. code-block:: rsyslog

   module(load="imtcp" maxSessions="500")

Input usage
-----------
.. _param-imtcp-input-maxsessions:
.. _imtcp.parameter.input.maxsessions-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" maxSessions="500")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imtcp.parameter.legacy.inputtcpmaxsessions:

- $InputTCPMaxSessions — maps to MaxSessions (status: legacy)

.. index::
   single: imtcp; $InputTCPMaxSessions
   single: $InputTCPMaxSessions

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
