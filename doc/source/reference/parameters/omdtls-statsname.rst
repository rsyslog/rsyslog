.. _param-omdtls-statsname:
.. _omdtls.parameter.input.statsname:

statsname
=========

.. index::
   single: omdtls; statsname
   single: statsname

.. summary-start

Enables a dedicated per-action statistics counter identified by the provided name.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omdtls`.

:Name: statsname
:Scope: input
:Type: word
:Default: input=none
:Required?: no
:Introduced: v8.2402.0

Description
-----------
This parameter assigns a unique name to a dedicated statistics counter for the
``omdtls`` action instance. When set, rsyslog tracks separate "submitted" and
"failures" counters for this action that can be retrieved via the
:doc:`impstats <../../configuration/modules/impstats>` module by referencing the
specified name. This makes it easier to monitor the performance and health of
individual DTLS forwarding actions.

If left unset, the action contributes only to the global ``omdtls`` statistics
origin.

Input usage
-----------
.. _omdtls.parameter.input.statsname-usage:

.. code-block:: rsyslog

   action(type="omdtls"
          target="192.0.2.1"
          port="4433"
          statsName="dtls_to_server1")

See also
--------
See also :doc:`../../configuration/modules/omdtls`,
:doc:`../../configuration/modules/impstats`.
