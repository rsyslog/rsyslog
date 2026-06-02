.. _param-omrelp-tls-permanentfailuredisablesaction:
.. _omrelp.parameter.action.tls-permanentfailuredisablesaction:

.. meta::
   :description: Configure whether omrelp disables or suspends an action after TLS authentication failure.
   :keywords: rsyslog, omrelp, RELP, TLS, authentication failure, action queue

TLS.PermanentFailureDisablesAction
==================================

.. index::
   single: omrelp; TLS.PermanentFailureDisablesAction
   single: TLS.PermanentFailureDisablesAction

.. summary-start

Controls whether TLS authentication failures disable the action or suspend it for retry.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: TLS.PermanentFailureDisablesAction
:Scope: action
:Type: boolean
:Default: on
:Required?: no
:Introduced: 8.2606.0

Description
-----------
Controls how omrelp handles TLS authentication failures such as certificate
validation or permitted peer mismatches.

When enabled, the default, omrelp treats these failures as permanent and
disables the action. This preserves the historical behavior and prevents
continuous retry loops for configuration errors that usually need operator
intervention.

When disabled, omrelp suspends the action instead. This lets configured action
queues continue accepting messages while rsyslog retries according to the
action retry settings. Use this mode when certificate or CA changes may be
temporary and queued messages should be retained until the TLS configuration is
corrected.

Action usage
------------
.. _param-omrelp-action-tls-permanentfailuredisablesaction:
.. _omrelp.parameter.action.tls-permanentfailuredisablesaction-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" tls="on"
          tls.permanentFailureDisablesAction="off")

YAML usage
----------
.. code-block:: yaml

   actions:
     - type: omrelp
       target: centralserv
       tls: "on"
       tls.permanentFailureDisablesAction: "off"

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
