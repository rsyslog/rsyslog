.. _param-imdiag-serverinputname:
.. _imdiag.parameter.input.serverinputname:

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
:Scope: input
:Type: string
:Default: input=imdiag
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Sets the ``inputname`` metadata assigned to log messages originating from the
imdiag TCP listener itself (for example, connection warnings). This does **not**
affect the ``inputname`` of messages injected via the control channel, which
always remains ``imdiag``. Configure this value before :ref:`ServerRun
<param-imdiag-serverrun>` so the listener logs use the desired identifier from
startup onward.

Input usage
-----------
.. _param-imdiag-input-serverinputname:
.. _imdiag.parameter.input.serverinputname-usage:

.. code-block:: rsyslog

   module(load="imdiag")
   input(type="imdiag" listenPortFileName="/var/run/imdiag.port"
         serverInputName="diag-main" serverRun="19998")

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
