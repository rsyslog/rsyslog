.. _param-imdiag-injectdelaymode:
.. _imdiag.parameter.module.injectdelaymode:

InjectDelayMode
===============

.. index::
   single: imdiag; InjectDelayMode
   single: InjectDelayMode

.. summary-start

Sets the flow-control classification applied to messages injected by imdiag.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imdiag`.

:Name: InjectDelayMode
:Scope: module
:Type: string (``no``, ``light``, ``full``)
:Default: module=no
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
The diagnostics interface can inject messages into rsyslog's queues on demand.
:ref:`InjectDelayMode <param-imdiag-injectdelaymode>` assigns the flow-control
policy used for those messages:

``no``
    Injected messages bypass delay throttling.
``light``
    Injected messages are marked "light delayable" so that queue congestion
    slows them while preserving capacity for non-throttleable inputs.
``full``
    Injected messages are fully delayable and subject to the strongest
    throttling when queues fill up.

Use this setting to prevent diagnostics traffic from overwhelming production
inputs when the main queue is near capacity.

Module usage
------------
.. _param-imdiag-module-injectdelaymode:
.. _imdiag.parameter.module.injectdelaymode-usage:

.. code-block:: rsyslog

   module(load="imdiag" injectDelayMode="light")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imdiag.parameter.legacy.imdiaginjectdelaymode:

- $IMDiagInjectDelayMode — maps to InjectDelayMode (status: legacy)

.. index::
   single: imdiag; $IMDiagInjectDelayMode
   single: $IMDiagInjectDelayMode

See also
--------
See also :doc:`../../configuration/modules/imdiag`.
