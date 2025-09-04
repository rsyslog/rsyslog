.. _param-mmnormalize-userawmsg:
.. _mmnormalize.parameter.action.userawmsg:

useRawMsg
=========

.. index::
   single: mmnormalize; useRawMsg
   single: useRawMsg

.. summary-start

Uses the raw message instead of just the MSG part during normalization.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmnormalize`.

:Name: useRawMsg
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: at least 6.1.2, possibly earlier

Description
-----------
Specifies if the raw message should be used for normalization (on) or just the MSG part of the message (off).

Action usage
-------------
.. _param-mmnormalize-action-userawmsg:
.. _mmnormalize.parameter.action.userawmsg-usage:

.. code-block:: rsyslog

   action(type="mmnormalize" useRawMsg="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _mmnormalize.parameter.legacy.mmnormalizeuserawmsg:

- $mmnormalizeUseRawMsg — maps to useRawMsg (status: legacy)

.. index::
   single: mmnormalize; $mmnormalizeUseRawMsg
   single: $mmnormalizeUseRawMsg

See also
--------
See also :doc:`../../configuration/modules/mmnormalize`.
