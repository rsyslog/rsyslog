.. _param-omdtls-target:
.. _omdtls.parameter.input.target:

target
======

.. index::
   single: omdtls; target
   single: target

.. summary-start

Specifies the destination host name or IP address for omdtls.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omdtls`.

:Name: target
:Scope: input
:Type: word
:Default: input=none
:Required?: no
:Introduced: v8.2402.0

Description
-----------
Specifies the target hostname or IP address to send log messages to. If not
set, the action has no destination and messages cannot be delivered.

Input usage
-----------
.. _param-omdtls-input-target:
.. _omdtls.parameter.input.target-usage:

.. code-block:: rsyslog

   action(type="omdtls" target="192.0.2.1" port="4433")

See also
--------
See also :doc:`../../configuration/modules/omdtls`.
