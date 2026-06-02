.. _param-imdiag-serverinputname:
.. _imdiag.parameter.module.serverinputname:

ServerInputName
================

.. index::
   single: imdiag; ServerInputName
   single: ServerInputName

.. summary-start

Overrides the ``inputname`` property for the diagnostic TCP listener's
own log messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdiag`.

:Name: ServerInputName
:Scope: module
:Type: string
:Default: module=imdiag
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Sets the ``inputname`` metadata assigned to log messages originating from the
imdiag TCP listener itself (for example, connection warnings). This does **not**
affect the ``inputname`` of messages injected via the control channel, which
always remains ``imdiag``. This parameter must be configured before using
:ref:`ServerRun <param-imdiag-serverrun>`. This ensures the listener logs use
the desired identifier from startup onward.

Module usage
------------
.. _param-imdiag-module-serverinputname:
.. _imdiag.parameter.module.serverinputname-usage:

.. code-block:: rsyslog

   module(load="imdiag"
          listenPortFileName="/var/run/rsyslog/imdiag.port"
          serverInputName="diag-main"
          serverRun="19998")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imdiag.parameter.legacy.imdiagserverinputname:

- $IMDiagServerInputName — maps to ServerInputName (status: legacy)

.. index::
   single: imdiag; $IMDiagServerInputName
   single: $IMDiagServerInputName

See also
--------
See also :doc:`../../configuration/modules/imdiag`.
