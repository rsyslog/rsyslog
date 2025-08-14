.. _param-imudp-name-appendport:
.. _imudp.parameter.module.name-appendport:

Name.appendPort
===============

.. index::
   single: imudp; Name.appendPort
   single: Name.appendPort

.. summary-start

Appends the listener's port number to the ``inputname`` value.

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
When enabled, the port number is appended to the ``inputname`` property. If no
explicit ``name`` is set, the default ``imudp`` is used before the port, yielding
values like ``imudp514``. This helps make input names unique when multiple ports
are defined. IPv4 and IPv6 listeners on the same port are not distinguished.

Input usage
-----------
.. _param-imudp-input-name-appendport:
.. _imudp.parameter.input.name-appendport:
.. code-block:: rsyslog

   input(type="imudp" Name.appendPort="...")

Notes
-----
- Historic documentation called this a ``binary`` option; it is boolean.

See also
--------
See also :doc:`../../configuration/modules/imudp`.

