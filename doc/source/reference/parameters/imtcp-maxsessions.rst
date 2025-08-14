.. _param-imtcp-maxsessions:
.. _imtcp.parameter.module.maxsessions:
.. _imtcp.parameter.input.maxsessions:

MaxSessions
===========

.. index::
   single: imtcp; MaxSessions
   single: MaxSessions

.. summary-start

Sets the maximum number of concurrent TCP sessions supported.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: MaxSessions
:Scope: module, input
:Type: integer
:Default: ``200`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: at least 5.x, possibly earlier (module), 8.2106.0 (input)

Description
-----------
This parameter defines the maximum number of simultaneous TCP sessions that the ``imtcp`` module can support.

This can be set at the module level to define a global default for all ``imtcp`` listeners. This value must be set when the module is loaded, before any ``input()`` listeners are defined. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.maxsessions-usage:

.. code-block:: rsyslog

   module(load="imtcp" maxSessions="500")

Input usage
-----------
.. _imtcp.parameter.input.maxsessions-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" maxSessions="100")

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
