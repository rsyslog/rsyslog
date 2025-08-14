.. _param-imudp-name-appendport:
.. _imudp.parameter.module.name-appendport:

Name.appendPort
===============

.. index::
   single: imudp; Name.appendPort
   single: Name.appendPort

.. summary-start

Appends the listener port to the `inputname` when enabled.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imudp`.

:Name: Name.appendPort
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: 7.3.9

Description
-----------
When set to `on`, the current port number is concatenated to the value of `Name`, yielding unique input identifiers per port.

Input usage
-----------
.. _param-imudp-input-name-appendport:
.. _imudp.parameter.input.name-appendport:
.. code-block:: rsyslog

   input(type="imudp" Name.appendPort="...")

Notes
-----
.. versionadded:: 7.3.9

The original documentation described this option as ``binary``; it maps to a boolean.

See also
--------
See also :doc:`../../configuration/modules/imudp`.
